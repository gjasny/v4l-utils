// SPDX-License-Identifier: MIT
/*
 * Copyright 2024 Cisco Systems, Inc. and/or its affiliates. All rights reserved.
 *
 * Author: Hans Verkuil <hverkuil+cisco@kernel.org>
 */

#include <cctype>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <ctime>
#include <string>

#include <fcntl.h>
#include <getopt.h>
#include <sys/ioctl.h>
#include <sys/epoll.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <math.h>
#include <dirent.h>

#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <linux/types.h>

#include "edid-decode.h"

// i2c addresses for edid
#define EDID_ADDR 0x50
#define SEGMENT_POINTER_ADDR 0x30

// i2c address for SCDC
#define SCDC_ADDR 0x54

// i2c addresses for HDCP
#define HDCP_PRIM_ADDR 0x3a
#define HDCP_SEC_ADDR 0x3b

static struct timespec start_monotonic;
static struct timeval start_timeofday;
static time_t valid_until_t;

static std::string ts2s(__u64 ts)
{
	static char buf[64];
	static unsigned last_secs;
	static time_t last_t;
	std::string s;
	struct timeval sub = {};
	struct timeval res;
	unsigned secs;
	__s64 diff;
	time_t t;

	diff = ts - start_monotonic.tv_sec * 1000000000ULL - start_monotonic.tv_nsec;
	if (diff >= 0) {
		sub.tv_sec = diff / 1000000000ULL;
		sub.tv_usec = (diff % 1000000000ULL) / 1000;
	}
	timeradd(&start_timeofday, &sub, &res);
	t = res.tv_sec;
	if (t >= valid_until_t) {
		struct tm tm = *localtime(&t);
		last_secs = tm.tm_min * 60 + tm.tm_sec;
		last_t = t;
		valid_until_t = t + 60 - last_secs;
		strftime(buf, sizeof(buf), "%a %b %e %T.000000", &tm);
	}
	secs = last_secs + t - last_t;
	sprintf(buf + 14, "%02u:%02u.%06llu", secs / 60, secs % 60, (__u64)res.tv_usec);
	return buf;
}

static __u64 current_ts()
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}


int request_i2c_adapter(const char *device)
{
	int fd = open(device, O_RDWR);

	if (fd >= 0)
		return fd;
	fprintf(stderr, "Error accessing i2c adapter %s\n", device);
	return fd;
}

static int read_edid_block(int adapter_fd, __u8 *edid,
			   uint8_t segment, uint8_t offset, uint8_t blocks,
			   bool silent)
{
	struct i2c_rdwr_ioctl_data data;
	struct i2c_msg write_message;
	struct i2c_msg read_message;
	struct i2c_msg seg_message;
	int err;

	seg_message = {
		.addr = SEGMENT_POINTER_ADDR,
		.len = 1,
		.buf = &segment
	};
	write_message = {
		.addr = EDID_ADDR,
		.len = 1,
		.buf = &offset
	};
	read_message = {
		.addr = EDID_ADDR,
		.flags = I2C_M_RD,
		.len = (__u16)(blocks * EDID_PAGE_SIZE),
		.buf = edid
	};

	if (segment) {
		struct i2c_msg msgs[2] = { seg_message, read_message };

		data.msgs = msgs;
		data.nmsgs = ARRAY_SIZE(msgs);
		err = ioctl(adapter_fd, I2C_RDWR, &data);
	} else {
		struct i2c_msg msgs[2] = { write_message, read_message };

		data.msgs = msgs;
		data.nmsgs = ARRAY_SIZE(msgs);
		err = ioctl(adapter_fd, I2C_RDWR, &data);
	}

	if (err < 0) {
		if (!silent)
			fprintf(stderr, "Unable to read edid: %s\n", strerror(errno));
		return err;
	}
	return 0;
}

int read_edid(int adapter_fd, unsigned char *edid, bool silent)
{
	unsigned n_extension_blocks;
	int err;

	err = read_edid_block(adapter_fd, edid, 0, 0, 2, silent);
	if (err)
		return err;
	n_extension_blocks = edid[126];
	if (!n_extension_blocks)
		return 1;

	// Check for HDMI Forum EDID Extension Override Data Block
	if (n_extension_blocks >= 1 &&
	    edid[128] == 2 &&
	    edid[133] == 0x78 &&
	    (edid[132] & 0xe0) == 0xe0 &&
	    (edid[132] & 0x1f) >= 2 &&
	    edid[134] > 1)
		n_extension_blocks = edid[134];

	for (unsigned i = 2; i <= n_extension_blocks; i += 2) {
		err = read_edid_block(adapter_fd, edid + i * 128, i / 2, 0,
				      (i + 1 > n_extension_blocks ? 1 : 2),
				      silent);
		if (err)
			return err;
	}
	return n_extension_blocks + 1;
}

int test_reliability(int adapter_fd, unsigned secs, unsigned msleep)
{
	unsigned char edid[EDID_PAGE_SIZE * EDID_MAX_BLOCKS];
	unsigned char edid_tmp[EDID_PAGE_SIZE * EDID_MAX_BLOCKS];
	unsigned iter = 0;
	unsigned blocks;
	int ret;

	ret = read_edid(adapter_fd, edid);
	if (ret <= 0) {
		printf("FAIL: could not read initial EDID.\n");
		return ret;
	}
	blocks = ret;

	if (secs)
		printf("Read EDID (%u bytes) for %u seconds with %u milliseconds between each read.\n\n",
		       blocks * EDID_PAGE_SIZE, secs, msleep);
	else
		printf("Read EDID (%u bytes) forever with %u milliseconds between each read.\n\n",
		       blocks * EDID_PAGE_SIZE, msleep);

	clock_gettime(CLOCK_MONOTONIC, &start_monotonic);
	gettimeofday(&start_timeofday, nullptr);

	time_t start = time(NULL);
	time_t start_test = start;

	while (true) {
		iter++;
		if (msleep)
			usleep(msleep * 1000);
		ret = read_edid(adapter_fd, edid_tmp);
		if (ret <= 0) {
			printf("\nFAIL: failed to read EDID (iteration %u).\n", iter);
			return -1;
		}
		if ((unsigned)ret != blocks) {
			printf("\nFAIL: expected %u blocks, read %d blocks (iteration %u).\n",
			       blocks, ret, iter);
			return -1;
		}
		if (memcmp(edid, edid_tmp, blocks * EDID_PAGE_SIZE)) {
			printf("Initial EDID:\n\n");
			for (unsigned i = 0; i < blocks; i++) {
				hex_block("", edid + i * EDID_PAGE_SIZE, EDID_PAGE_SIZE, false);
				printf("\n");
			}
			printf("EDID of iteration %u:\n\n", iter);
			for (unsigned i = 0; i < blocks; i++) {
				hex_block("", edid_tmp + i * EDID_PAGE_SIZE, EDID_PAGE_SIZE, false);
				printf("\n");
			}
			printf("FAIL: mismatch between EDIDs (iteration %u).\n", iter);
			return -1;
		}
		time_t cur = time(NULL);
		if (secs && cur - start_test > (time_t)secs)
			break;
		if (cur - start >= 10) {
			start = cur;
			printf("At iteration %u...\n", iter);
			fflush(stdout);
		}
	}

	printf("\n%u iterations over %u seconds: PASS\n", iter, secs);
	return 0;
}

static int read_hdcp_registers(int adapter_fd, __u8 *hdcp_prim, __u8 *hdcp_sec, __u8 *ksv_fifo)
{
	struct i2c_rdwr_ioctl_data data;
	struct i2c_msg write_message;
	struct i2c_msg read_message;
	__u8 offset = 0;
	__u8 ksv_fifo_offset = 0x43;
	int err;

	write_message = {
		.addr = HDCP_PRIM_ADDR,
		.len = 1,
		.buf = &offset
	};
	read_message = {
		.addr = HDCP_PRIM_ADDR,
		.flags = I2C_M_RD,
		.len = 256,
		.buf = hdcp_prim
	};

	struct i2c_msg msgs[2] = { write_message, read_message };

	data.msgs = msgs;
	data.nmsgs = ARRAY_SIZE(msgs);
	err = ioctl(adapter_fd, I2C_RDWR, &data);

	if (err < 0) {
		fprintf(stderr, "Unable to read Primary Link HDCP: %s\n",
			strerror(errno));
		return -1;
	}

	write_message = {
		.addr = HDCP_PRIM_ADDR,
		.len = 1,
		.buf = &ksv_fifo_offset
	};
	read_message = {
		.addr = HDCP_PRIM_ADDR,
		.flags = I2C_M_RD,
		.len = (__u16)((hdcp_prim[0x41] & 0x7f) * 5),
		.buf = ksv_fifo
	};

	if (read_message.len) {
		struct i2c_msg ksv_fifo_msgs[2] = { write_message, read_message };

		data.msgs = ksv_fifo_msgs;
		data.nmsgs = ARRAY_SIZE(msgs);
		err = ioctl(adapter_fd, I2C_RDWR, &data);
		if (err < 0) {
			fprintf(stderr, "Unable to read KSV FIFO: %s\n",
				strerror(errno));
			return -1;
		}
	}

	write_message = {
		.addr = HDCP_SEC_ADDR,
		.len = 1,
		.buf = &offset
	};
	read_message = {
		.addr = HDCP_SEC_ADDR,
		.flags = I2C_M_RD,
		.len = 256,
		.buf = hdcp_sec
	};

	struct i2c_msg sec_msgs[2] = { write_message, read_message };
	data.msgs = sec_msgs;
	data.nmsgs = ARRAY_SIZE(msgs);
	ioctl(adapter_fd, I2C_RDWR, &data);

	return 0;
}

int read_hdcp(int adapter_fd)
{
	__u8 hdcp_prim[256];
	__u8 hdcp_sec[256];
	__u8 ksv_fifo[128 * 5];

	hdcp_prim[5] = 0xdd;
	hdcp_sec[5] = 0xdd;
	if (read_hdcp_registers(adapter_fd, hdcp_prim, hdcp_sec, ksv_fifo))
		return -1;
	printf("HDCP Primary Link Hex Dump:\n\n");
	hex_block("", hdcp_prim, 128, false);
	printf("\n");
	hex_block("", hdcp_prim + 128, 128, false);
	printf("\n");
	if (hdcp_sec[5] != 0xdd) {
		printf("HDCP Secondary Link Hex Dump:\n\n");
		hex_block("", hdcp_sec, 128, false);
		printf("\n");
		hex_block("", hdcp_sec + 128, 128, false);
		printf("\n");
	}
	printf("HDCP Primary Link:\n\n");
	printf("Bksv: %02x %02x %02x %02x %02x\n",
	       hdcp_prim[0], hdcp_prim[1], hdcp_prim[2], hdcp_prim[3], hdcp_prim[4]);
	printf("Ri': %02x %02x\n", hdcp_prim[9], hdcp_prim[8]);
	printf("Pj': %02x\n", hdcp_prim[0x0a]);
	printf("Aksv: %02x %02x %02x %02x %02x\n",
	       hdcp_prim[0x10], hdcp_prim[0x11], hdcp_prim[0x12], hdcp_prim[0x13], hdcp_prim[0x14]);
	printf("Ainfo: %02x\n", hdcp_prim[0x15]);
	printf("An: %02x %02x %02x %02x %02x %02x %02x %02x\n",
	       hdcp_prim[0x18], hdcp_prim[0x19], hdcp_prim[0x1a], hdcp_prim[0x1b],
	       hdcp_prim[0x1c], hdcp_prim[0x1d], hdcp_prim[0x1e], hdcp_prim[0x1f]);
	printf("V'.H0: %02x %02x %02x %02x\n",
	       hdcp_prim[0x20], hdcp_prim[0x21], hdcp_prim[0x22], hdcp_prim[0x23]);
	printf("V'.H1: %02x %02x %02x %02x\n",
	       hdcp_prim[0x24], hdcp_prim[0x25], hdcp_prim[0x26], hdcp_prim[0x27]);
	printf("V'.H2: %02x %02x %02x %02x\n",
	       hdcp_prim[0x28], hdcp_prim[0x29], hdcp_prim[0x2a], hdcp_prim[0x2b]);
	printf("V'.H3: %02x %02x %02x %02x\n",
	       hdcp_prim[0x2c], hdcp_prim[0x2d], hdcp_prim[0x2e], hdcp_prim[0x2f]);
	printf("V'.H4: %02x %02x %02x %02x\n",
	       hdcp_prim[0x30], hdcp_prim[0x31], hdcp_prim[0x32], hdcp_prim[0x33]);
	__u8 v = hdcp_prim[0x40];
	printf("Bcaps: %02x\n", v);
	if (v & 0x01)
		printf("\tFAST_REAUTHENTICATION\n");
	if (v & 0x02)
		printf("\t1.1_FEATURES\n");
	if (v & 0x10)
		printf("\tFAST\n");
	if (v & 0x20)
		printf("\tREADY\n");
	if (v & 0x40)
		printf("\tREPEATER\n");
	__u16 vv = hdcp_prim[0x41] | (hdcp_prim[0x42] << 8);
	printf("Bstatus: %04x\n", vv);
	printf("\tDEVICE_COUNT: %u\n", vv & 0x7f);
	if (vv & 0x80)
		printf("\tMAX_DEVS_EXCEEDED\n");
	printf("\tDEPTH: %u\n", (vv >> 8) & 0x07);
	if (vv & 0x800)
		printf("\tMAX_CASCADE_EXCEEDED\n");
	if (vv & 0x1000)
		printf("\tHDMI_MODE\n");
	if (vv & 0x7f) {
		printf("KSV FIFO:\n");
		for (unsigned i = 0; i < (vv & 0x7f); i++)
			printf("\t%03u: %02x %02x %02x %02x %02x\n", i,
			       ksv_fifo[i * 5], ksv_fifo[i * 5 + 1],
			       ksv_fifo[i * 5 + 2], ksv_fifo[i * 5 + 3],
			       ksv_fifo[i * 5 + 4]);
	}
	v = hdcp_prim[0x50];
	printf("HDCP2Version: %02x\n", v);
	if (v & 4)
		printf("\tHDCP2.2\n");
	vv = hdcp_prim[0x70] | (hdcp_prim[0x71] << 8);
	printf("RxStatus: %04x\n", vv);
	if (vv & 0x3ff)
		printf("\tMessage_Size: %u\n", vv & 0x3ff);
	if (vv & 0x400)
		printf("\tREADY\n");
	if (vv & 0x800)
		printf("\tREAUTH_REQ\n");

	if (hdcp_sec[5] == 0xdd)
		return 0;

	printf("HDCP Secondary Link:\n\n");
	printf("Bksv: %02x %02x %02x %02x %02x\n",
	       hdcp_sec[0], hdcp_sec[1], hdcp_sec[2], hdcp_sec[3], hdcp_sec[4]);
	printf("Ri': %02x %02x\n", hdcp_sec[9], hdcp_sec[9]);
	printf("Pj': %02x\n", hdcp_sec[0x0a]);
	printf("Aksv: %02x %02x %02x %02x %02x\n",
	       hdcp_sec[0x10], hdcp_sec[0x11], hdcp_sec[0x12], hdcp_sec[0x13], hdcp_sec[0x14]);
	printf("Ainfo: %02x\n", hdcp_sec[0x15]);
	printf("An: %02x %02x %02x %02x %02x %02x %02x %02x\n",
	       hdcp_sec[0x18], hdcp_sec[0x19], hdcp_sec[0x1a], hdcp_sec[0x1b],
	       hdcp_sec[0x1c], hdcp_sec[0x1d], hdcp_sec[0x1e], hdcp_sec[0x1f]);
	return 0;
}

static int read_hdcp_ri_register(int adapter_fd, __u16 *v)
{
	struct i2c_rdwr_ioctl_data data;
	struct i2c_msg write_message;
	struct i2c_msg read_message;
	__u8 ri[2];
	__u8 offset = 8;
	int err;

	write_message = {
		.addr = HDCP_PRIM_ADDR,
		.len = 1,
		.buf = &offset
	};
	read_message = {
		.addr = HDCP_PRIM_ADDR,
		.flags = I2C_M_RD,
		.len = 2,
		.buf = ri
	};

	struct i2c_msg msgs[2] = { write_message, read_message };

	data.msgs = msgs;
	data.nmsgs = ARRAY_SIZE(msgs);
	err = ioctl(adapter_fd, I2C_RDWR, &data);

	if (err < 0)
		fprintf(stderr, "Unable to read Ri: %s\n", strerror(errno));
	else
		*v = ri[1] << 8 | ri[0];

	return err < 0 ? err : 0;
}

int read_hdcp_ri(int adapter_fd, double ri_time)
{
	__u64 last_ts = current_ts();
	bool first = true;
	__u16 ri = 0;
	__u16 last_ri = 0;

	clock_gettime(CLOCK_MONOTONIC, &start_monotonic);
	gettimeofday(&start_timeofday, nullptr);

	while (1) {
		__u64 ts = current_ts();

		printf("Timestamp: %s", ts2s(ts).c_str());
		if (!read_hdcp_ri_register(adapter_fd, &ri)) {
			printf(" Ri': 0x%04x", ri);
			if (!first && ri != last_ri) {
				printf(" (changed from 0x%04x after %llu ms)",
				       last_ri, (ts - last_ts) / 1000000);
				last_ts = ts;
				last_ri = ri;
			}
			first = false;
		}
		printf("\n");
		fflush(stdout);
		usleep(ri_time * 1000000);
	}
	return 0;
}
