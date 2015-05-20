/*
 *  Cobalt NOR flash utilities
 *
 *  Copyright 2015 Cisco Systems, Inc. and/or its affiliates.
 *  All rights reserved.
 *
 *  This program is free software; you may redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 *  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 *  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 *  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 *  BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 *  ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 *  CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *  SOFTWARE.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <dirent.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <stdbool.h>
#include <error.h>
#include <mtd/mtd-abi.h>

#include "s-record.h"


#define COBALT_FLASH_SIZE    (64 * 1024 * 1024)
#define SIZEOF_PAGE          0xc00000
#define PAGE0_OFFSET         0x020000
#define PAGE1_OFFSET         0xc20000
#define DEFAULT_READ_SIZE    256
#define DEFAULT_READ_OFFSET  PAGE1_OFFSET


static void usage(void)
{
	printf("Usage:\n");
	printf("Options:\n");
	printf("  -d, --device=<dev> Use device <dev> as the video device.\n");
	printf("                     If <dev> starts with a digit, then /dev/video<dev> is used.\n");
	printf("  -s, --scan         Scan for mtd devices\n");
	printf("  -i, --info         Get flash information\n");
	printf("  -f, --factory      Allow write of factory image\n");
	printf("  -w, --write        Write *.flash file to flash\n");
	printf("  -m, --multiple     Use multiple byte write algorithm. 2-64, default 64 bytes\n");
	printf("  -r, --read         Read from flash to file. Default %d bytes\n",
	       DEFAULT_READ_SIZE);
	printf("  -c, --count        Bytes to read from flash. 1-%d\n",
	       DEFAULT_READ_SIZE);
	printf("  -o, --offset       Read offset into flash. Default offset = page1\n");
	printf("  -h, --help         Print this message\n");
}

static const char short_options[] = "d:w:m:r:c:o:sifh";
static const struct option long_options[] = {
	{ "device",  required_argument, NULL, 'd' },
	{ "scan",    no_argument,       NULL, 's' },
	{ "info",    no_argument,       NULL, 'i' },
	{ "factory", no_argument,       NULL, 'f' },
	{ "write",   required_argument, NULL, 'w' },
	{ "multiple",required_argument, NULL, 'm' },
	{ "read",    required_argument, NULL, 'r' },
	{ "count",   required_argument, NULL, 'c' },
	{ "offset",  required_argument, NULL, 'o' },
	{ "help",    no_argument,       NULL, 'h' },
	{ 0, 0, 0, 0 }
};

static char *get_flash_type(u_char type)
{
	switch (type) {
	case MTD_ABSENT:
		return "MTD_ABSENT";
	case MTD_RAM:
		return "MTD_RAM";
	case MTD_ROM:
		return "MTD_ROM";
	case MTD_NORFLASH:
		return "NOR flash";
	case MTD_NANDFLASH:
		return "NAND flash";
	case MTD_DATAFLASH:
		return "MTD dataflash";
	case MTD_UBIVOLUME:
		return "MTD_UBIVOLUME";
	default:
		break;
	}

	return "Unknown!";
}

static int get_flash_info(char *device, struct mtd_info_user *iu, struct stat *fs)
{
	int fd;

	if ((fd = open(device, O_RDONLY)) < 0) {
		printf("Can't open %s\n", device);
		return -1;
	}

	fstat(fd, fs);

	if (ioctl(fd, MEMGETINFO, iu) < 0) {
		error(0, errno, "ioclt MEMGETINFO on device %s", device);
		close(fd);
		return -1;
	}

	close(fd);
	return 0;
}

static int show_flash_info(char *device)
{
	struct mtd_info_user iu;
	struct stat fs;

	if (get_flash_info(device, &iu, &fs))
		return -1;

	printf("MTD device %s:\n", device);
	printf("\tdevice number:\t%d\n", (int)fs.st_rdev);
	printf("\tserial number:\t%d\n", (int)fs.st_ino);
	printf("\toptimal blocksize:\t%d\n", (int)fs.st_blksize);
	printf("\tfile mode:\t\t0x%x\n", fs.st_mode);
	printf("\ttype:\t\t%s\n",get_flash_type(iu.type));
	printf("\tflags:\t\t%s\n", iu.flags & MTD_WRITEABLE ? "writeable" : "read only");
	printf("\t\t\t%s\n",
	       iu.flags & MTD_NO_ERASE ? "no erase is necessary" : "need erase");
	printf("\t\t\t%s %s\n",
	       iu.flags & MTD_POWERUP_LOCK ? "always" : "may be", "locked after reset");
	printf("\tsize:\t\t%u MB\n", iu.size / 1024 / 1024);
	printf("\terasesize\t%u kB\n", iu.erasesize / 1024);
	printf("\twritesize\t%u byte(s)\n", iu.writesize);
	printf("\toobsize\t\t0x%x\n", iu.oobsize);
	return 0;
}

static int get_mtd_device(char *path, char *device, int slen)
{
	struct dirent *entry;
	struct stat info;
	DIR *dir;
	int found = -1;

	if (stat(path, &info)) {
		printf("Couldn't find %s\n", path);
		return -1;
	}

	if (!S_ISDIR(info.st_mode)) {
		printf("%s isn't a directory\n", path);
		return -1;
	}

	dir = opendir(path);
	if (dir) {
		while ((entry = readdir(dir)) != NULL) {
			if (strstr(entry->d_name, "mtd") && !strstr(entry->d_name, "ro")) {
				snprintf(device, slen, "/dev/%s", entry->d_name);
				found = 0;
			}
		}
		closedir(dir);
	}
	return found;
}

static int scan_for_mtd_devices(void)
{
	const char *cobalt_sysfs = "/sys/bus/pci/drivers/cobalt/";
	struct dirent *cobalt_entry;
	DIR *cobalt_dir;

	cobalt_dir = opendir(cobalt_sysfs);
	if (cobalt_dir == NULL)
		return 0;
	while ((cobalt_entry = readdir(cobalt_dir)) != NULL) {
		struct dirent *video_entry;
		DIR *video_dir;
		char path[256];
		char mtd_device[20];

		if (!strstr(cobalt_entry->d_name, ":"))
			continue;

		printf("Found Cobalt card with PCI address: %s\n", cobalt_entry->d_name);
		printf("Video4Linux devices: ");
		sprintf(path, "%s%s/video4linux/", cobalt_sysfs, cobalt_entry->d_name);
		video_dir = opendir(path);
		if (video_dir) {
			while ((video_entry = readdir(video_dir)) != NULL) {
				if (strstr(video_entry->d_name, "video"))
					printf("%s ", video_entry->d_name);
			}
			closedir(video_dir);
		}
		printf("\n");

		sprintf(path, "%s%s/mtd/", cobalt_sysfs, cobalt_entry->d_name);
		if (get_mtd_device(path, mtd_device, 20) == 0) {
			show_flash_info(mtd_device);
		}
		printf("\n");
	}

	closedir(cobalt_dir);
	return 0;
}

static void show_performance(struct timeval *start, struct timeval *end, int size)
{
	double ssec = start->tv_sec + start->tv_usec / 1000000.0;
	double esec = end->tv_sec + end->tv_usec / 1000000.0;
	double bytes = (double)size;
	double bsec = bytes / (esec - ssec);

	printf("Time used: %.2lf seconds ==> ", esec - ssec);
	if (bsec / 1000000 > 1.0)
		printf("%.3lf MB/s\n", bsec/1000000);
	else if (bsec / 1000 > 1.0)
		printf("%.3lf kB/s\n", bsec/1000);
	else
		printf("%.3lf B/s\n", bsec);
}

static int write_to_file(char *filename, char *buffer, int bytes)
{
	int fd;
	int ret;

	if ((fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644)) < 0) {
		printf("Can't open %s\n", filename);
		return -1;
	}

	printf("Write content to: %s\n", filename);
	ret = write(fd, buffer, bytes);
	if (ret != bytes)
		printf("write() returned %d instead of %d\n", ret, bytes);

	chmod(filename, DEFFILEMODE);
	close(fd);

	return ret != bytes;
}

static int read_flash(char *device, char *filename, off_t offset, int length)
{
	struct mtd_info_user iu;
	struct stat fs;
	char *buffer = NULL;
	int status = -1;
	int fd = 0;
	int i = 0;
	int left_to_read = length;
	struct timeval start, end;

	if (get_flash_info(device, &iu, &fs))
		goto exit;

	if (!(buffer = (char *)malloc(length))) {
		printf("Can't allocate %d bytes buffer\n", length);
		goto exit;
	}

	if ((fd = open(device, O_RDONLY)) < 0) {
		printf("Can't open %s\n", device);
		goto exit;
	}

	printf("Read %d bytes from offset 0x%lx\n", length, offset);
	gettimeofday(&start, NULL);
	lseek(fd, offset, SEEK_SET);
	for (i = 0;  left_to_read > 0;  i +=fs.st_blksize) {
		int bytes = left_to_read < fs.st_blksize ? left_to_read : (int)fs.st_blksize;
		int ret;

		ret = read(fd, &buffer[i], bytes);
		if (ret != bytes) {
			printf("read() returned %d instead of %d\n", ret, bytes);
			status = -1;
			goto exit;
		}
		left_to_read -= bytes;
	}

	gettimeofday(&end, NULL);
	show_performance(&start, &end, length);

	status = write_to_file(filename, buffer, length);

exit:
	if (fd > 0)
		close(fd);
	if (buffer)
		free(buffer);

	return status;
}

static int block_in_use(unsigned char *binary, unsigned int block, struct mtd_info_user *mi)
{
	int i = block * mi->erasesize;

	for (;  i < block * mi->erasesize + mi->erasesize;  i++) {
		if (binary[i] != 0xff)
			return 1;
	}

	return 0;
}

static int get_srecord_file(char *filename, char **srecord)
{
	struct stat fs;
	char *buffer;
	int fd = 0;
	int length = 0;

	if ((fd = open(filename, O_RDONLY)) < 0) {
		printf("Can't open %s\n", filename);
		return -1;
	}

	fstat(fd, &fs);
	if (!(buffer = malloc(fs.st_size))) {
		printf("Can't allocate %lu bytes buffer\n", fs.st_size);
		goto exit;
	}

	printf("Read %lu bytes from: %s\n", fs.st_size, filename);
	length = read(fd, buffer, fs.st_size);
	if (length != fs.st_size) {
		printf("Not able to read the whole file! (%d/%lu)\n", length, fs.st_size);
		length = 0;
	}

exit:
	if (fd > 0)
		close(fd);
	*srecord = buffer;
	return length;
}

//#define TEST

static int write_flash(char *device, char *filename, unsigned int bytes_pr_write, bool allow_factory_write)
{
	struct erase_info_user ei;
	struct mtd_info_user mi;
	struct stat fs;
	struct timeval start, end;
	unsigned char *binary = NULL;
	unsigned char *verify = NULL;
	char *srecord = NULL;
	int srecord_size;
	int binary_size;
	int num_of_erase_blocks;
	int erase_block_cnt;
	int write_block_cnt;
	unsigned int block;
	int fd = 0;
	int status = -1;
	int first_continue_block = -1;
	unsigned min_offset = allow_factory_write ? PAGE0_OFFSET : PAGE1_OFFSET;
	unsigned max_offset = min_offset + SIZEOF_PAGE - 1;

	if (bytes_pr_write > 64)
		bytes_pr_write = 64;
	if (bytes_pr_write < 2)
		bytes_pr_write = 2;

	if (get_flash_info(device, &mi, &fs))
		goto exit;

	if (!(verify = malloc(bytes_pr_write))) {
		printf("Can't allocate %u bytes buffer\n", bytes_pr_write);
		goto exit;
	}

	if (!(binary = malloc(COBALT_FLASH_SIZE))) {
		printf("Can't allocate %d bytes buffer\n", COBALT_FLASH_SIZE);
		goto exit;
	}

	memset(binary, 0xffffffff, COBALT_FLASH_SIZE);
	num_of_erase_blocks = mi.size / mi.erasesize;

	if ((srecord_size = get_srecord_file(filename, &srecord)) <= 0)
		goto exit;

	if ((binary_size = s_record_to_bin(srecord, binary, srecord_size, mi.size)) <= 0)
		goto exit;

	for (erase_block_cnt = 0, block = 0; block < num_of_erase_blocks;  block++) {
		if (block_in_use(binary, block, &mi))
			erase_block_cnt++;

		if ((first_continue_block < 0) && block_in_use(binary, block, &mi))
			first_continue_block = block;

		if ((first_continue_block >= 0) && !block_in_use(binary, block, &mi)) {
			printf("Flash block %u..%u with offset 0x%x..0x%x will be erased/written\n",
			       first_continue_block, block - 1,
			       first_continue_block * mi.erasesize, block * mi.erasesize - 1);
			first_continue_block = -1;
		}
	}

	if ((fd = open(device, O_RDWR)) < 0) {
		printf("Can't open %s\n", device);
		goto exit;
	}

	printf("Erase %u blocks: #", erase_block_cnt);
	gettimeofday(&start, NULL);
	ei.length = mi.erasesize;
	for (erase_block_cnt = 0, block = 0;  block < num_of_erase_blocks;  block++) {
		if (!block_in_use(binary, block, &mi))
			continue;

		ei.start = (off_t)(block * mi.erasesize);
		if (ei.start < min_offset) {
			printf("\nAttempt to erase (0x%x) below offset 0x%x\n",
					ei.start, min_offset);
				goto exit;
		}
		if (ei.start + mi.erasesize > max_offset) {
			printf("\nAttempt to erase (0x%x) above offset 0x%x\n",
					ei.start + mi.erasesize, max_offset);
				goto exit;
		}

		erase_block_cnt++;
		printf("%03u\b\b\b", erase_block_cnt);
		fflush(stdout);

		if (ioctl(fd, MEMERASE, &ei)) {
			printf("Erase of block %03u failed!\n", erase_block_cnt);
			goto exit;
		}

#if defined(TEST)
		if (erase_block_cnt >= 1)
			break;
#endif
	}

	gettimeofday(&end, NULL);
	printf("\b\b\b      \n");
	show_performance(&start, &end, erase_block_cnt * mi.erasesize);

	printf("Write/verify %d bytes to flash: #", binary_size);
	fflush(stdout);
	gettimeofday(&start, NULL);

	for (write_block_cnt = 0, block = 0;  block < num_of_erase_blocks;  block++) {
		unsigned int offset = block * mi.erasesize;

		if (!block_in_use(binary, block, &mi))
			continue;

		write_block_cnt++;
		for (; offset < ((block + 1) * mi.erasesize);  offset += bytes_pr_write) {
			lseek(fd, offset, SEEK_SET);
			write(fd, &binary[offset], bytes_pr_write);
#if !defined(TEST)
			lseek(fd, offset, SEEK_SET);
			read(fd, verify, bytes_pr_write);
			if (memcmp(verify, &binary[offset], bytes_pr_write)) {
				unsigned int i;

				printf("\nFlash verification failed!\n");
				for (i = 0;  i < bytes_pr_write;  i += 2) {
					printf("Offset 0x%x is 0x%02x%02x,"
					       "should be 0x%02x%02x\n",
					       offset + i, verify[i], verify[i+1],
					       binary[offset+i], binary[offset+i+1]);
				}
				goto exit;
			}
#endif
		}

		printf("%03u\b\b\b", write_block_cnt);
		fflush(stdout);

#if defined(TEST)
		if (write_block_cnt >= 1)
			break;
#endif
	}

	gettimeofday(&end, NULL);
	printf("\b\b\b      \n");
	printf("\nFlash write/verification was successful!\n");
	show_performance(&start, &end, write_block_cnt * mi.erasesize);
	status = 0;

exit:
	if (fd > 0)
		close(fd);
	if (binary)
		free(binary);
	if (srecord)
		free(srecord);

	return status;
}

int main(int argc, char **argv)
{
	char *device = NULL;
	char *filename = NULL;
	bool do_write = false;
	bool do_read = false;
	bool do_show_info = false;
	bool allow_factory_write = false;
	int read_size = DEFAULT_READ_SIZE;
	int read_offset = DEFAULT_READ_OFFSET;
	char path[256];
	char mtd_device[20];
	unsigned int bytes_pr_write = 64;

	printf("\n");
	if (argc == 1) {
		usage();
		exit(EXIT_SUCCESS);
	}

	for (;;) {
		int idx;
		int c;

		c = getopt_long(argc, argv, short_options, long_options, &idx);
		if (-1 == c)
			break;

		switch (c) {
		case 0: /* getopt_long() flag */
			break;
		case 'd':
			device = optarg;
			if (device[0] >= '0' && device[0] <= '9' && strlen(device) <= 3) {
				static char newdev[20];

				sprintf(newdev, "video%s", device);
				device = newdev;
			}
			break;
		case 's':
			scan_for_mtd_devices();
			exit(EXIT_SUCCESS);
		case 'i':
			do_show_info = true;
			break;
		case 'f':
			allow_factory_write = true;
			break;
		case 'w':
			filename = optarg;
			do_write = true;
			break;
		case 'm':
			bytes_pr_write = strtoul(optarg, NULL, 10);
			break;
		case 'r':
			filename = optarg;
			do_read = true;
			break;
		case 'c':
			read_size = strtoul(optarg, NULL, 10);
			if (read_size > COBALT_FLASH_SIZE || read_size < 1)
				goto input_err;
			break;
		case 'o':
			read_offset = strtoul(optarg, NULL, 16);
			if (read_offset > COBALT_FLASH_SIZE)
				goto input_err;
			break;
		case 'h':
			usage();
			exit(EXIT_SUCCESS);
		default:
			goto input_err;
		}
	}

	if (!device)
		goto input_err;

	if (!memcmp("/dev/", device, 5))
		device += 5;

	sprintf(path, "/sys/class/video4linux/%s/device/mtd/", device);
	if (get_mtd_device(path, mtd_device, 20))
		goto input_err;

	if (do_show_info) {
		if (show_flash_info(mtd_device))
			goto input_err;
		else
			exit(EXIT_SUCCESS);
	}

	if (do_read) {
		if (read_offset +read_size > COBALT_FLASH_SIZE)
			goto input_err;

		if (!read_flash(mtd_device, filename, read_offset, read_size))
			exit(EXIT_SUCCESS);

	} else if (do_write) {
		if (!write_flash(mtd_device, filename, bytes_pr_write, allow_factory_write))
			exit(EXIT_SUCCESS);
	}

input_err:
	usage();
	exit(EXIT_FAILURE);
}
