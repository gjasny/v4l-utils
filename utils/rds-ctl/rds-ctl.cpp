/*
 * rds-ctl.cpp is based on v4l2-ctl.cpp
 *
 * the following applies for all RDS related parts:
 * Copyright 2012 Cisco Systems, Inc. and/or its affiliates. All rights reserved.
 * Author: Konke Radlow <koradlow@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Suite 500, Boston, MA  02110-1335  USA
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <locale.h>
#include <inttypes.h>
#include <getopt.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <dirent.h>
#include <config.h>
#include <signal.h>

#include <linux/videodev2.h>
#include <libv4l2.h>
#include <libv4l2rds.h>

#include <list>
#include <vector>
#include <map>
#include <string>
#include <algorithm>

#define ARRAY_SIZE(arr) ((int)(sizeof(arr) / sizeof((arr)[0])))

typedef std::vector<std::string> dev_vec;
typedef std::map<std::string, std::string> dev_map;

/* Short option list

   Please keep in alphabetical order.
   That makes it easier to see which short options are still free.

   In general the lower case is used to set something and the upper
   case is used to retrieve a setting. */
enum Option {
	OptRBDS = 'b',
	OptSetDevice = 'd',
	OptGetDriverInfo = 'D',
	OptGetFreq = 'F',
	OptSetFreq = 'f',
	OptHelp = 'h',
	OptReadRds = 'R',
	OptGetTuner = 'T',
	OptUseWrapper = 'w',
	OptAll = 128,
	OptFreqSeek,
	OptListDevices,
	OptListFreqBands,
	OptOpenFile,
	OptPrintBlock,
	OptSilent,
	OptTMC,
	OptTunerIndex,
	OptVerbose,
	OptWaitLimit,
	OptLast = 256
};

struct ctl_parameters {
	bool terminate_decoding;
	char options[OptLast];
	char fd_name[80];
	bool filemode_active;
	double freq;
	uint32_t wait_limit;
	uint8_t tuner_index;
	struct v4l2_hw_freq_seek freq_seek;
};

static struct ctl_parameters params;
static int app_result;

static struct option long_options[] = {
	{"all", no_argument, 0, OptAll},
	{"rbds", no_argument, 0, OptRBDS},
	{"device", required_argument, 0, OptSetDevice},
	{"file", required_argument, 0, OptOpenFile},
	{"freq-seek", required_argument, 0, OptFreqSeek},
	{"get-freq", no_argument, 0, OptGetFreq},
	{"get-tuner", no_argument, 0, OptGetTuner},
	{"help", no_argument, 0, OptHelp},
	{"info", no_argument, 0, OptGetDriverInfo},
	{"list-devices", no_argument, 0, OptListDevices},
	{"list-freq-bands", no_argument, 0, OptListFreqBands},
	{"print-block", no_argument, 0, OptPrintBlock},
	{"read-rds", no_argument, 0, OptReadRds},
	{"set-freq", required_argument, 0, OptSetFreq},
	{"tmc", no_argument, 0, OptTMC},
	{"tuner-index", required_argument, 0, OptTunerIndex},
	{"silent", no_argument, 0, OptSilent},
	{"verbose", no_argument, 0, OptVerbose},
	{"wait-limit", required_argument, 0, OptWaitLimit},
	{"wrapper", no_argument, 0, OptUseWrapper},
	{0, 0, 0, 0}
};

static void usage_hint(void)
{
	fprintf(stderr, "Try 'rds-ctl --help' for more information.\n");
}

static void usage_common(void)
{
	printf("\nGeneral/Common options:\n"
	       "  --all              display all device information available\n"
	       "  -D, --info         show driver info [VIDIOC_QUERYCAP]\n"
	       "  -d, --device=<dev> use device <dev>\n"
	       "                     If <dev> starts with a digit, then /dev/radio<dev> is used\n"
	       "                     default: checks for RDS-capable devices,\n"
	       "                     uses device with lowest ID\n"
	       "  -h, --help         display this help message\n"
	       "  -w, --wrapper      use the libv4l2 wrapper library.\n"
	       "  --list-devices     list all v4l radio devices with RDS capabilities\n"
	       );
}

static void usage_tuner(void)
{
	printf("\nTuner/Modulator options:\n"
	       "  -F, --get-freq     query the frequency [VIDIOC_G_FREQUENCY]\n"
	       "  -f, --set-freq=<freq>\n"
	       "                     set the frequency to <freq> MHz [VIDIOC_S_FREQUENCY]\n"
	       "  -T, --get-tuner    query the tuner settings [VIDIOC_G_TUNER]\n"
	       "  --tuner-index=<idx> Use idx as tuner idx for tuner/modulator commands\n"
	       "  --freq-seek=dir=<0/1>,wrap=<0/1>,spacing=<hz>\n"
	       "                     perform a hardware frequency seek [VIDIOC_S_HW_FREQ_SEEK]\n"
	       "                     dir is 0 (seek downward) or 1 (seek upward)\n"
	       "                     wrap is 0 (do not wrap around) or 1 (wrap around)\n"
	       "                     spacing sets the seek resolution (use 0 for default)\n"
	       "  --list-freq-bands  display all frequency bands for the tuner/modulator\n"
	       "                     [VIDIOC_ENUM_FREQ_BANDS]\n"
	       );
}

static void usage_rds(void)
{
	printf("\nRDS options: \n"
	       "  -b, --rbds         parse the RDS data according to the RBDS standard\n"
	       "  -R, --read-rds     enable reading of RDS data from device\n"
	       "  --file=<path>      open a RDS stream file dump instead of a device\n"
	       "                     all General and Tuner Options are disabled in this mode\n"
	       "  --wait-limit=<ms>  defines the maximum wait duration for avaibility of new\n"
	       "                     RDS data\n"
	       "                     <default>: 5000 ms\n"
	       "  --print-block      prints all valid RDS fields, whenever a value is updated\n"
	       "                     instead of printing only updated values\n"
	       "  --tmc              print information about TMC (Traffic Message Channel) messages\n"
	       "  --silent           only set the result code, do not print any messages\n"
	       "  --verbose          turn on verbose mode - every received RDS group\n"
	       "                     will be printed\n"
	       );
}

static void usage(void)
{
	printf("Usage:\n");
	usage_common();
	usage_tuner();
	usage_rds();
}

static void signal_handler_interrupt(int signum)
{
	fprintf(stderr, "Interrupt received: Terminating program\n");
	params.terminate_decoding = true;
}

static int test_open(const char *file, int oflag)
{
 	return params.options[OptUseWrapper] ? v4l2_open(file, oflag) : open(file, oflag);
}

static int test_close(int fd)
{
	return params.options[OptUseWrapper] ? v4l2_close(fd) : close(fd);
}

static int test_ioctl(int fd, int cmd, void *arg)
{
	return params.options[OptUseWrapper] ? v4l2_ioctl(fd, cmd, arg) : ioctl(fd, cmd, arg);
}

static int doioctl_name(int fd, unsigned long int request, void *parm, const char *name)
{
	int retval = test_ioctl(fd, request, parm);

	if (retval < 0) {
		app_result = -1;
	}
	if (params.options[OptSilent]) return retval;
	if (retval < 0)
		printf("%s: failed: %s\n", name, strerror(errno));
	else if (params.options[OptVerbose])
		printf("%s: ok\n", name);

	return retval;
}

#define doioctl(n, r, p) doioctl_name(n, r, p, #r)

static const char *audmode2s(int audmode)
{
	switch (audmode) {
		case V4L2_TUNER_MODE_STEREO: return "stereo";
		case V4L2_TUNER_MODE_LANG1: return "lang1";
		case V4L2_TUNER_MODE_LANG2: return "lang2";
		case V4L2_TUNER_MODE_LANG1_LANG2: return "bilingual";
		case V4L2_TUNER_MODE_MONO: return "mono";
		default: return "unknown";
	}
}

static std::string rxsubchans2s(int rxsubchans)
{
	std::string s;

	if (rxsubchans & V4L2_TUNER_SUB_MONO)
		s += "mono ";
	if (rxsubchans & V4L2_TUNER_SUB_STEREO)
		s += "stereo ";
	if (rxsubchans & V4L2_TUNER_SUB_LANG1)
		s += "lang1 ";
	if (rxsubchans & V4L2_TUNER_SUB_LANG2)
		s += "lang2 ";
	if (rxsubchans & V4L2_TUNER_SUB_RDS)
		s += "rds ";
	return s;
}

static std::string tcap2s(unsigned cap)
{
	std::string s;

	if (cap & V4L2_TUNER_CAP_LOW)
		s += "62.5 Hz ";
	else
		s += "62.5 kHz ";
	if (cap & V4L2_TUNER_CAP_NORM)
		s += "multi-standard ";
	if (cap & V4L2_TUNER_CAP_HWSEEK_BOUNDED)
		s += "hwseek-bounded ";
	if (cap & V4L2_TUNER_CAP_HWSEEK_WRAP)
		s += "hwseek-wrap ";
	if (cap & V4L2_TUNER_CAP_STEREO)
		s += "stereo ";
	if (cap & V4L2_TUNER_CAP_LANG1)
		s += "lang1 ";
	if (cap & V4L2_TUNER_CAP_LANG2)
		s += "lang2 ";
	if (cap & V4L2_TUNER_CAP_RDS)
		s += "rds ";
	if (cap & V4L2_TUNER_CAP_RDS_BLOCK_IO)
		s += "rds-block-I/O ";
	if (cap & V4L2_TUNER_CAP_RDS_CONTROLS)
		s += "rds-controls ";
	if (cap & V4L2_TUNER_CAP_FREQ_BANDS)
		s += "freq-bands ";
	if (cap & V4L2_TUNER_CAP_HWSEEK_PROG_LIM)
		s += "hwseek-prog-lim ";
	return s;
}

static std::string cap2s(unsigned cap)
{
	std::string s;

	if (cap & V4L2_CAP_RDS_CAPTURE)
		s += "\t\tRDS Capture\n";
	if (cap & V4L2_CAP_RDS_OUTPUT)
		s += "\t\tRDS Output\n";
	if (cap & V4L2_CAP_TUNER)
		s += "\t\tTuner\n";
	if (cap & V4L2_CAP_MODULATOR)
		s += "\t\tModulator\n";
	if (cap & V4L2_CAP_AUDIO)
		s += "\t\tAudio\n";
	if (cap & V4L2_CAP_RADIO)
		s += "\t\tRadio\n";
	if (cap & V4L2_CAP_READWRITE)
		s += "\t\tRead/Write\n";
	if (cap & V4L2_CAP_ASYNCIO)
		s += "\t\tAsync I/O\n";
	if (cap & V4L2_CAP_STREAMING)
		s += "\t\tStreaming\n";
	if (cap & V4L2_CAP_DEVICE_CAPS)
		s += "\t\tDevice Capabilities\n";
	return s;
}

static std::string modulation2s(unsigned modulation)
{
	switch (modulation) {
	case V4L2_BAND_MODULATION_VSB:
		return "VSB";
	case V4L2_BAND_MODULATION_FM:
		return "FM";
	case V4L2_BAND_MODULATION_AM:
		return "AM";
	}
	return "Unknown";
}

static bool is_radio_dev(const char *name)
{
	return !memcmp(name, "radio", 5);
}

static void print_devices(dev_vec files)
{
	dev_map cards;
	int fd = -1;
	std::string bus_info;
	struct v4l2_capability vcap;

	for (dev_vec::iterator iter = files.begin();
		iter != files.end(); ++iter) {
		fd = open(iter->c_str(), O_RDWR);
		memset(&vcap, 0, sizeof(vcap));
		if (fd < 0)
			continue;
		doioctl(fd, VIDIOC_QUERYCAP, &vcap);
		close(fd);
		bus_info = (const char *)vcap.bus_info;
	if (cards[bus_info].empty())
			cards[bus_info] += std::string((char *)vcap.card)
				+ " (" + bus_info + "):\n";
		cards[bus_info] += "\t" + (*iter);
		cards[bus_info] += "\n";
	}
	for (dev_map::iterator iter = cards.begin();
			iter != cards.end(); ++iter) {
		printf("%s\n", iter->second.c_str());
	}
}
static dev_vec list_devices(void)
{
	DIR *dp;
	struct dirent *ep;
	dev_vec files;
	dev_map links;

	struct v4l2_tuner vt;

	dp = opendir("/dev");
	if (dp == NULL) {
		perror ("Couldn't open the directory");
		exit(1);
	}
	while ((ep = readdir(dp)))
		if (is_radio_dev(ep->d_name))
			files.push_back(std::string("/dev/") + ep->d_name);
	closedir(dp);

	/* Iterate through all devices, and remove all non-accessible devices
	 * and all devices that don't offer the RDS_BLOCK_IO capability */
	for (dev_vec::iterator iter = files.begin();
			iter != files.end(); ++iter) {
		int fd = open(iter->c_str(), O_RDONLY | O_NONBLOCK);
		std::string bus_info;

		if (fd < 0) {
			iter = files.erase(iter);
			continue;
		}
		memset(&vt, 0, sizeof(vt));
		if (doioctl(fd, VIDIOC_G_TUNER, &vt) != 0) {
			close(fd);
			iter = files.erase(iter);
			continue;
		}
		/* remove device if it doesn't support rds block I/O */
		if (!(vt.capability & V4L2_TUNER_CAP_RDS_BLOCK_IO))
			iter = files.erase(iter);
		close(fd);
	}
	return files;
}

static int parse_subopt(char **subs, const char * const *subopts, char **value)
{
	int opt = getsubopt(subs, (char * const *)subopts, value);

	if (opt == -1) {
		fprintf(stderr, "Invalid suboptions specified\n");
		return -1;
	}
	if (*value == NULL) {
		fprintf(stderr, "No value given to suboption <%s>\n",
				subopts[opt]);
		return -1;
	}
	return opt;
}

static void parse_freq_seek(char *optarg, struct v4l2_hw_freq_seek &seek)
{
	char *value;
	char *subs = optarg;

	while (*subs != '\0') {
		static const char *const subopts[] = {
			"dir",
			"wrap",
			"spacing",
			NULL
		};

		switch (parse_subopt(&subs, subopts, &value)) {
		case 0:
			seek.seek_upward = strtol(value, 0L, 0);
			break;
		case 1:
			seek.wrap_around = strtol(value, 0L, 0);
			break;
		case 2:
			seek.spacing = strtol(value, 0L, 0);
			break;
		default:
			usage_tuner();
			exit(1);
		}
	}
}

static void print_byte(char byte, bool linebreak)
{
	int count = 8;
	printf(" ");
	while (count--) {
		printf("%d", (byte & 128) ? 1 : 0 );
		byte <<= 1;
	}
	if (linebreak)
		printf("\n");
}

static void print_rds_group(const struct v4l2_rds_group *rds_group)
{
	printf("\nComplete RDS data group received \n");
	printf("PI: %04x\n", rds_group->pi);
	printf("Group: %u%c \n", rds_group->group_id, rds_group->group_version);
	printf("Data B:");
	print_byte(rds_group->data_b_lsb, true);
	printf("Data C:");
	print_byte(rds_group->data_c_msb, false);
	print_byte(rds_group->data_c_lsb, true);
	printf("Data D:");
	print_byte(rds_group->data_d_msb, false);
	print_byte(rds_group->data_d_lsb, true);
	printf("\n");
}

static void print_decoder_info(uint8_t di)
{
	printf("\nDI: ");
	if (di & V4L2_RDS_FLAG_STEREO)
		printf("Stereo, ");
	else
		printf("Mono, ");
	if (di & V4L2_RDS_FLAG_ARTIFICIAL_HEAD)
		printf("Artificial Head, ");
	else
		printf("No Artificial Head, ");
	if (di & V4L2_RDS_FLAG_COMPRESSED)
		printf("Compressed");
	else
		printf("Not Compressed");
}

static void print_rds_tmc(const struct v4l2_rds *handle, uint32_t updated_fields)
{
	const struct v4l2_rds_tmc_msg *msg = &handle->tmc.tmc_msg;
	const struct v4l2_tmc_additional_set *set = &msg->additional;

	if (updated_fields & V4L2_RDS_TMC_SG) {
		printf("\nTMC Single-grp: location: %04x, event: %04x, extent: %02x "
			"duration: %02x", msg->location, msg->event,
			msg->extent, msg->dp);
		return;
	}
	if (updated_fields & V4L2_RDS_TMC_MG) {
		printf("\nTMC Multi-grp: length: %02d, location: %04x, event: %04x,\n"
		"               extent: %02x duration: %02x", msg->length, msg->location, msg->event,
			msg->extent, msg->dp);
		for (int i = 0; i < set->size; i++) {
			printf("\n               additional[%02d]: label: %02d, value: %04x",
			i, set->fields[i].label, set->fields[i].data);
		}
		return;
	}
}

static void print_rds_tmc_tuning(const struct v4l2_rds *handle, uint32_t updated_fields)
{
	const struct v4l2_tmc_tuning *tuning = &handle->tmc.tuning;
	const struct v4l2_tmc_station *station;
	
	if (updated_fields & V4L2_RDS_TMC_TUNING) {
		printf("\nTMC Service provider: %s, %u alternative stations\n", handle->tmc.spn, tuning->station_cnt);
		for (int i = 0; i < tuning->station_cnt; i++) {
			station = &tuning->station[i];
			printf("PI(ON %02u) = %04x, AFs: %u, mapped AFs: %u \n", i, station->pi,
					station->afi.af_size, station->afi.mapped_af_size);
			for (int j = 0; j < station->afi.af_size; j++)
				printf("        AF%02d: %.1fMHz\n", j, station->afi.af[j] / 1000000.0);
			for (int k = 0; k < station->afi.mapped_af_size; k++)
				printf("        m_AF%02d: %.1fMHz => %.1fMHz\n", k,
						station->afi.mapped_af_tuning[k] / 1000000.0,
						station->afi.mapped_af[k] / 1000000.0);
			if (station->ltn != 0 || station->msg != 0 || station-> sid != 0)
				printf("        ltn: %02x, msg: %02x, sid: %02x\n", station->ltn,
						station->msg, station->sid);
		}
	}
}

static void print_rds_statistics(const struct v4l2_rds_statistics *statistics)
{
	printf("\n\nRDS Statistics: \n");
	printf("received blocks / received groups: %u / %u\n",
		statistics->block_cnt, statistics->group_cnt);

	float block_error_percentage = 0;

	if (statistics->block_cnt)
		block_error_percentage =
			((float)statistics->block_error_cnt / statistics->block_cnt) * 100.0;
	printf("block errors / group errors: %u (%3.2f%%) / %u \n",
		statistics->block_error_cnt,
		block_error_percentage, statistics->group_error_cnt);

	float block_corrected_percentage = 0;

	if (statistics->block_cnt)
		block_corrected_percentage = (
			(float)statistics->block_corrected_cnt / statistics->block_cnt) * 100.0;
	printf("corrected blocks: %u (%3.2f%%)\n",
		statistics->block_corrected_cnt, block_corrected_percentage);
	for (int i = 0; i < 16; i++)
		printf("Group %02d: %u\n", i, statistics->group_type_cnt[i]);
}

static void print_rds_af(const struct v4l2_rds_af_set *af_set)
{
	int counter = 0;

	printf("\nAnnounced AFs: %u", af_set->announced_af);
	for (int i = 0; i < af_set->size && i < af_set->announced_af; i++, counter++) {
		if (af_set->af[i] >= 87500000 ) {
			printf("\nAF%02d: %.1fMHz", counter, af_set->af[i] / 1000000.0);
			continue;
		}
		printf("\nAF%02d: %.3fkHz", counter, af_set->af[i] / 1000.0);
	}
}

static void print_rds_eon(const struct v4l2_rds_eon_set *eon_set)
{
	int counter = 0;

	printf("\n\nEnhanced Other Network information: %u channels", eon_set->size);
	for (int i = 0; i < eon_set->size; i++, counter++) {
		if (eon_set->eon[i].valid_fields & V4L2_RDS_PI)
			printf("\nPI(ON %02i) =  %04x", i, eon_set->eon[i].pi);
		if (eon_set->eon[i].valid_fields & V4L2_RDS_PS)
			printf("\nPS(ON %02i) =  %s", i, eon_set->eon[i].ps);
		if (eon_set->eon[i].valid_fields & V4L2_RDS_PTY)
			printf("\nPTY(ON %02i) =  %0u", i, eon_set->eon[i].pty);
		if (eon_set->eon[i].valid_fields & V4L2_RDS_LSF)
			printf("\nLSF(ON %02i) =  %0u", i, eon_set->eon[i].lsf);
		if (eon_set->eon[i].valid_fields & V4L2_RDS_AF)
			printf("\nPTY(ON %02i) =  %0u", i, eon_set->eon[i].pty);
		if (eon_set->eon[i].valid_fields & V4L2_RDS_TP)
			printf("\nTP(ON %02i): %s", i, eon_set->eon[i].tp? "yes":"no");
		if (eon_set->eon[i].valid_fields & V4L2_RDS_TA)
			printf("\nTA(ON %02i): %s", i, eon_set->eon[i].tp? "yes":"no");
		if (eon_set->eon[i].valid_fields & V4L2_RDS_AF) {
			printf("\nAF(ON %02i): size=%i", i, eon_set->eon[i].af.size);
			print_rds_af(&(eon_set->eon[i].af));
		}
	}
}

static void print_rds_pi(const struct v4l2_rds *handle)
{
	printf("\nArea Coverage: %s", v4l2_rds_get_coverage_str(handle));
}

static void print_rds_data(const struct v4l2_rds *handle, uint32_t updated_fields)
{
	if (params.options[OptPrintBlock])
		updated_fields = 0xffffffff;

	if ((updated_fields & V4L2_RDS_PI) &&
			(handle->valid_fields & V4L2_RDS_PI)) {
		printf("\nPI: %04x", handle->pi);
		print_rds_pi(handle);
	}

	if (updated_fields & V4L2_RDS_PS &&
			handle->valid_fields & V4L2_RDS_PS) {
		printf("\nPS: %s", handle->ps);
	}

	if (updated_fields & V4L2_RDS_PTY && handle->valid_fields & V4L2_RDS_PTY)
		printf("\nPTY: %0u -> %s",handle->pty, v4l2_rds_get_pty_str(handle));

	if (updated_fields & V4L2_RDS_PTYN && handle->valid_fields & V4L2_RDS_PTYN) {
		printf("\nPTYN: %s", handle->ptyn);
	}

	if (updated_fields & V4L2_RDS_TIME) {
		printf("\nTime: %s", ctime(&handle->time));
	}
	if (updated_fields & V4L2_RDS_RT && handle->valid_fields & V4L2_RDS_RT) {
		printf("\nRT: %s", handle->rt);
	}

	if (updated_fields & V4L2_RDS_TP && handle->valid_fields & V4L2_RDS_TP)
		printf("\nTP: %s  TA: %s", (handle->tp)? "yes":"no",
			handle->ta? "yes":"no");
	if (updated_fields & V4L2_RDS_MS && handle->valid_fields & V4L2_RDS_MS)
		printf("\nMS Flag: %s", (handle->ms)? "Music" : "Speech");
	if (updated_fields & V4L2_RDS_ECC && handle->valid_fields & V4L2_RDS_ECC)
		printf("\nECC: %X%x, Country: %u -> %s",
		handle->ecc >> 4, handle->ecc & 0x0f, handle->pi >> 12,
		v4l2_rds_get_country_str(handle));
	if (updated_fields & V4L2_RDS_LC && handle->valid_fields & V4L2_RDS_LC)
		printf("\nLanguage: %u -> %s ", handle->lc,
		v4l2_rds_get_language_str(handle));
	if (updated_fields & V4L2_RDS_DI && handle->valid_fields & V4L2_RDS_DI)
		print_decoder_info(handle->di);
	if (updated_fields & V4L2_RDS_ODA &&
			handle->decode_information & V4L2_RDS_ODA) {
		for (int i = 0; i < handle->rds_oda.size; ++i)
			printf("\nODA Group: %02u%c, AID: %08x",handle->rds_oda.oda[i].group_id,
			handle->rds_oda.oda[i].group_version, handle->rds_oda.oda[i].aid);
	}
	if (updated_fields & V4L2_RDS_AF && handle->valid_fields & V4L2_RDS_AF)
		print_rds_af(&handle->rds_af);
	if (updated_fields & V4L2_RDS_TMC_TUNING && handle->valid_fields & V4L2_RDS_TMC_TUNING)
		print_rds_tmc_tuning(handle, updated_fields);
	if (params.options[OptPrintBlock])
		printf("\n");
	if (params.options[OptTMC])
		print_rds_tmc(handle, updated_fields);
}

static void read_rds(struct v4l2_rds *handle, const int fd, const int wait_limit)
{
	int byte_cnt = 0;
	int error_cnt = 0;
	uint32_t updated_fields = 0x00;
	struct v4l2_rds_data rds_data; /* read buffer for rds blocks */

	while (!params.terminate_decoding) {
		memset(&rds_data, 0, sizeof(rds_data));
		if ((byte_cnt=read(fd, &rds_data, 3)) != 3) {
			if (byte_cnt == 0) {
				printf("\nEnd of input file reached \n");
				break;
			} else if(++error_cnt > 2) {
				fprintf(stderr, "\nError reading from "
					"device (no RDS data available)\n");
				break;
			}
			/* wait for new data to arrive: transmission of 1
			 * group takes ~88.7ms */
			usleep(wait_limit * 1000);
		}
		else if (byte_cnt == 3) {
			error_cnt = 0;
			/* true if a new group was decoded */
			if ((updated_fields = v4l2_rds_add(handle, &rds_data))) {
				print_rds_data(handle, updated_fields);
				if (params.options[OptVerbose])
					 print_rds_group(v4l2_rds_get_group(handle));
			}
		}
	}
	/* print a summary of all valid RDS-fields before exiting */
	printf("\nSummary of valid RDS-fields:");
	print_rds_data(handle, 0xFFFFFFFF);
}

static void read_rds_from_fd(const int fd)
{
	struct v4l2_rds *rds_handle;

	/* create an rds handle for the current device */
	if (!(rds_handle = v4l2_rds_create(params.options[OptRBDS]))) {
		fprintf(stderr, "Failed to init RDS lib: %s\n", strerror(errno));
		exit(1);
	}

	/* try to receive and decode RDS data */
	read_rds(rds_handle, fd, params.wait_limit);
	if (rds_handle->valid_fields & V4L2_RDS_EON)
		print_rds_eon(&rds_handle->rds_eon);
	print_rds_statistics(&rds_handle->rds_statistics);

	v4l2_rds_destroy(rds_handle);
}

static int parse_cl(int argc, char **argv)
{
	int i = 0;
	int idx = 0;
	int opt = 0;
	/* 26 letters in the alphabet, case sensitive = 26 * 2 possible
	 * short options, where each option requires at most two chars
	 * {option, optional argument} */
	char short_options[26 * 2 * 2 + 1];

	if (argc == 1) {
		usage_hint();
		exit(1);
	}
	for (i = 0; long_options[i].name; i++) {
		if (!isalpha(long_options[i].val))
			continue;
		short_options[idx++] = long_options[i].val;
		if (long_options[i].has_arg == required_argument)
			short_options[idx++] = ':';
	}
	while (1) {
		int option_index = 0;

		short_options[idx] = 0;
		opt = getopt_long(argc, argv, short_options,
				 long_options, &option_index);
		if (opt == -1)
			break;

		params.options[(int)opt] = 1;
		switch (opt) {
		case OptSetDevice:
			strncpy(params.fd_name, optarg, sizeof(params.fd_name));
			if (optarg[0] >= '0' && optarg[0] <= '9' && strlen(optarg) <= 3) {
				snprintf(params.fd_name, sizeof(params.fd_name), "/dev/radio%s", optarg);
			}
			params.fd_name[sizeof(params.fd_name) - 1] = '\0';
			break;
		case OptSetFreq:
			params.freq = strtod(optarg, NULL);
			break;
		case OptListDevices:
			print_devices(list_devices());
			break;
		case OptFreqSeek:
			parse_freq_seek(optarg, params.freq_seek);
			break;
		case OptTunerIndex:
			params.tuner_index = strtoul(optarg, NULL, 0);
			break;
		case OptOpenFile:
		{
			if (access(optarg, F_OK) != -1) {
				params.filemode_active = true;
				strncpy(params.fd_name, optarg, sizeof(params.fd_name));
				params.fd_name[sizeof(params.fd_name) - 1] = '\0';
			} else {
				fprintf(stderr, "Unable to open file: %s\n", optarg);
				return -1;
			}
			/* enable the read-rds option by default for convenience */
			params.options[OptReadRds] = 1;
			break;
		}
		case OptWaitLimit:
			params.wait_limit = strtoul(optarg, NULL, 0);
			break;
		case ':':
			fprintf(stderr, "Option '%s' requires a value\n",
				argv[optind]);
			usage_hint();
			return 1;
		case '?':
			if (argv[optind])
				fprintf(stderr, "Unknown argument '%s'\n", argv[optind]);
			usage_hint();
			return 1;
		}
	}
	if (optind < argc) {
		printf("unknown arguments: ");
		while (optind < argc)
			printf("%s ", argv[optind++]);
		printf("\n");
		usage_hint();
		return 1;
	}
	if (params.options[OptAll]) {
		params.options[OptGetDriverInfo] = 1;
		params.options[OptGetFreq] = 1;
		params.options[OptGetTuner] = 1;
		params.options[OptSilent] = 1;
	}
	/* set default value for wait limit, if not specified by user */
	if (!params.options[OptWaitLimit])
		params.wait_limit = 5000;

	return 0;
}

static void print_driver_info(const struct v4l2_capability *vcap)
{

	printf("Driver Info (%susing libv4l2):\n",
			params.options[OptUseWrapper] ? "" : "not ");
	printf("\tDriver name   : %s\n", vcap->driver);
	printf("\tCard type     : %s\n", vcap->card);
	printf("\tBus info      : %s\n", vcap->bus_info);
	printf("\tDriver version: %d.%d.%d\n",
			vcap->version >> 16,
			(vcap->version >> 8) & 0xff,
			vcap->version & 0xff);
	printf("\tCapabilities  : 0x%08X\n", vcap->capabilities);
	printf("%s", cap2s(vcap->capabilities).c_str());
	if (vcap->capabilities & V4L2_CAP_DEVICE_CAPS) {
		printf("\tDevice Caps   : 0x%08X\n", vcap->device_caps);
		printf("%s", cap2s(vcap->device_caps).c_str());
	}
}

static void set_options(const int fd, const int capabilities, struct v4l2_frequency *vf,
			struct v4l2_tuner *tuner)
{
	double fac = 16;		/* factor for frequency division */

	if (params.options[OptSetFreq]) {
		vf->type = V4L2_TUNER_RADIO;
		tuner->index = params.tuner_index;
		if (doioctl(fd, VIDIOC_G_TUNER, tuner) == 0) {
			fac = (tuner->capability & V4L2_TUNER_CAP_LOW) ? 16000 : 16;
			vf->type = tuner->type;
		}

		vf->tuner = params.tuner_index;
		vf->frequency = __u32(params.freq * fac);
		if (doioctl(fd, VIDIOC_S_FREQUENCY, vf) == 0)
			printf("Frequency for tuner %d set to %d (%f MHz)\n",
				vf->tuner, vf->frequency, vf->frequency / fac);
	}

	if (params.options[OptFreqSeek]) {
		params.freq_seek.tuner = params.tuner_index;
		params.freq_seek.type = V4L2_TUNER_RADIO;
		doioctl(fd, VIDIOC_S_HW_FREQ_SEEK, &params.freq_seek);
	}
}

static void get_options(const int fd, const int capabilities, struct v4l2_frequency *vf,
			struct v4l2_tuner *tuner)
{
	double fac = 16;		/* factor for frequency division */

	if (params.options[OptGetFreq]) {
		vf->type = V4L2_TUNER_RADIO;
		tuner->index = params.tuner_index;
		if (doioctl(fd, VIDIOC_G_TUNER, tuner) == 0) {
			fac = (tuner->capability & V4L2_TUNER_CAP_LOW) ? 16000 : 16;
			vf->type = tuner->type;
		}
		vf->tuner = params.tuner_index;
		if (doioctl(fd, VIDIOC_G_FREQUENCY, vf) == 0)
			printf("Frequency for tuner %d: %d (%f MHz)\n",
				   vf->tuner, vf->frequency, vf->frequency / fac);
	}

	if (params.options[OptGetTuner]) {
		struct v4l2_tuner vt;

		memset(&vt, 0, sizeof(struct v4l2_tuner));
		vt.index = params.tuner_index;
		if (doioctl(fd, VIDIOC_G_TUNER, &vt) == 0) {
			printf("Tuner %d:\n", vt.index);
			printf("\tName                 : %s\n", vt.name);
			printf("\tCapabilities         : %s\n",
				tcap2s(vt.capability).c_str());
			if (vt.capability & V4L2_TUNER_CAP_LOW)
				printf("\tFrequency range      : %.1f MHz - %.1f MHz\n",
					 vt.rangelow / 16000.0, vt.rangehigh / 16000.0);
			else
				printf("\tFrequency range      : %.1f MHz - %.1f MHz\n",
					 vt.rangelow / 16.0, vt.rangehigh / 16.0);
			printf("\tSignal strength/AFC  : %d%%/%d\n",
				(int)((vt.signal / 655.35)+0.5), vt.afc);
			printf("\tCurrent audio mode   : %s\n", audmode2s(vt.audmode));
			printf("\tAvailable subchannels: %s\n",
					rxsubchans2s(vt.rxsubchans).c_str());
		}
	}

	if (params.options[OptListFreqBands]) {
		struct v4l2_frequency_band band;

		memset(&band, 0, sizeof(band));
		band.tuner = params.tuner_index;
		band.type = V4L2_TUNER_RADIO;
		band.index = 0;
		printf("ioctl: VIDIOC_ENUM_FREQ_BANDS\n");
		while (test_ioctl(fd, VIDIOC_ENUM_FREQ_BANDS, &band) >= 0) {
			if (band.index)
				printf("\n");
			printf("\tIndex          : %d\n", band.index);
			printf("\tModulation     : %s\n", modulation2s(band.modulation).c_str());
			printf("\tCapability     : %s\n", tcap2s(band.capability).c_str());
			if (band.capability & V4L2_TUNER_CAP_LOW)
				printf("\tFrequency Range: %.3f MHz - %.3f MHz\n",
				     band.rangelow / 16000.0, band.rangehigh / 16000.0);
			else
				printf("\tFrequency Range: %.3f MHz - %.3f MHz\n",
				     band.rangelow / 16.0, band.rangehigh / 16.0);
			band.index++;
		}
	}
}

int main(int argc, char **argv)
{
	int fd = -1;

	/* command args */
	struct v4l2_tuner tuner;	/* set_freq/get_freq */
	struct v4l2_capability vcap;	/* list_cap */
	struct v4l2_frequency vf;	/* get_freq/set_freq */

	memset(&tuner, 0, sizeof(tuner));
	memset(&vcap, 0, sizeof(vcap));
	memset(&vf, 0, sizeof(vf));
	strcpy(params.fd_name, "/dev/radio0");

	/* define locale for unicode support */
	if (!setlocale(LC_CTYPE, "")) {
		fprintf(stderr, "Can't set the specified locale!\n");
		return 1;
	}
	/* register signal handler for interrupt signal, to exit gracefully */
	signal(SIGINT, signal_handler_interrupt);

	/* try to parse the command line */
	parse_cl(argc, argv);
	if (params.options[OptHelp]) {
		usage();
		exit(0);
	}

	/* File Mode: disables all other features, except for RDS decoding */
	if (params.filemode_active) {
		if ((fd = open(params.fd_name, O_RDONLY|O_NONBLOCK)) < 0){
			perror("error opening file");
			exit(1);
		}
		read_rds_from_fd(fd);
		test_close(fd);
		exit(0);
	}

	/* Device Mode: open the radio device as read-only and non-blocking */
	if (!params.options[OptSetDevice]) {
		/* check the system for RDS capable devices */
		dev_vec devices = list_devices();
		if (devices.size() == 0) {
			fprintf(stderr, "No RDS-capable device found\n");
			exit(1);
		}
		strncpy(params.fd_name, devices[0].c_str(), sizeof(params.fd_name));
		params.fd_name[sizeof(params.fd_name) - 1] = '\0';
		printf("Using device: %s\n", params.fd_name);
	}
	if ((fd = test_open(params.fd_name, O_RDONLY | O_NONBLOCK)) < 0) {
		fprintf(stderr, "Failed to open %s: %s\n", params.fd_name,
			strerror(errno));
		exit(1);
	}
	doioctl(fd, VIDIOC_QUERYCAP, &vcap);

	/* Info options */
	if (params.options[OptGetDriverInfo])
		print_driver_info(&vcap);
	/* Set options */
	set_options(fd, vcap.capabilities, &vf, &tuner);
	/* Get options */
	get_options(fd, vcap.capabilities, &vf, &tuner);
	/* RDS decoding */
	if (params.options[OptReadRds])
		read_rds_from_fd(fd);

	test_close(fd);
	exit(app_result);
}
