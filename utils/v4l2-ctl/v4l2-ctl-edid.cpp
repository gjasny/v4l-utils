#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include <sys/ioctl.h>

#include "v4l2-ctl.h"

#include <linux/v4l2-subdev.h>

enum format {
	HEX,
	RAW,
	CARRAY
};

void edid_usage(void)
{
	printf("\nEDID options:\n"
	       "  --set-edid=pad=<pad>,[edid=<type>|file=<file>]\n"
	       "                     <pad> is the input or output index for which to set the EDID.\n"
	       "                     <type> can be 'hdmi', 'dvid' or 'vga'. A predefined EDID suitable\n"
	       "                     for that connector type will be set. It has a 1920x1080p60 native resolution.\n"
	       "                     If <file> is '-', then the data is read from stdin, otherwise it is\n"
	       "                     read from the given file. The file format must be in hex as in get-edid.\n"
	       "                     The 'edid' or 'file' arguments are mutually exclusive. One of the two\n"
	       "                     must be specified.\n"
	       "  --clear-edid=<pad>\n"
	       "                     <pad> is the input or output index for which to clear the EDID.\n"
	       "  --get-edid=pad=<pad>,startblock=<startblock>,blocks=<blocks>,format=<fmt>,file=<file>\n"
	       "                     <pad> is the input or output index for which to get the EDID.\n"
	       "                     <startblock> is the first block number you want to read. Default 0.\n"
	       "                     <blocks> is the number of blocks you want to read. Default is\n"
	       "                     all blocks.\n"
	       "                     <fmt> is one of:\n"
	       "                     hex:    hex numbers in ascii text\n"
	       "                     raw:    can be piped directly into the edid-decode tool\n"
	       "                     carray: c-program struct\n"
	       "                     If <file> is '-' or not the 'file' argument is not supplied, then the data\n"
	       "                     is written to stdout.\n"
	       "  --fix-edid-checksums\n"
	       "                     If specified then any checksum errors will be fixed silently.\n"
	       );
}

static void read_edid_file(FILE *f, struct v4l2_edid *e)
{
	char value[3] = { 0 };
	unsigned blocks = 1;
	unsigned i = 0;
	int c;

	fseek(f, SEEK_SET, 0);
	e->edid = (unsigned char *)malloc(blocks * 128);

	while ((c = fgetc(f)) != EOF) {
		if (!isxdigit(c))
			continue;
		if (i & 0x01) {
			value[1] = c;
			if (i / 2 > blocks * 128) {
				blocks++;
				if (blocks > 256) {
					fprintf(stderr, "edid file error: too long\n");
					free(e->edid);
					e->edid = NULL;
					exit(1);
				}
				e->edid = (unsigned char *)realloc(e->edid, blocks * 128);
			}
			e->edid[i / 2] = strtoul(value, 0, 16);
		} else {
			value[0] = c;
		}
		i++;
	}
	e->blocks = i / 256;
}

static unsigned char crc_calc(const unsigned char *b)
{
	unsigned char sum = 0;
	int i;

	for (i = 0; i < 127; i++)
		sum += b[i];
	return 256 - sum;
}

static bool crc_ok(const unsigned char *b)
{
	return crc_calc(b) == b[127];
}

static void fix_edid(struct v4l2_edid *e)
{
	for (unsigned b = 0; b < e->blocks; b++) {
		unsigned char *buf = e->edid + 128 * b;

		if (!crc_ok(buf))
			buf[127] = crc_calc(buf);
	}
}

static bool verify_edid(struct v4l2_edid *e)
{
	bool valid = true;

	for (unsigned b = 0; b < e->blocks; b++) {
		const unsigned char *buf = e->edid + 128 * b;

		if (!crc_ok(buf)) {
			fprintf(stderr, "Block %u has a checksum error (should be 0x%02x)\n",
					b, crc_calc(buf));
			valid = false;
		}
	}
	return valid;
}

static void hexdumpedid(FILE *f, struct v4l2_edid *e)
{
	for (unsigned b = 0; b < e->blocks; b++) {
		unsigned char *buf = e->edid + 128 * b;

		if (b)
			fprintf(f, "\n");
		for (unsigned i = 0; i < 128; i += 0x10) {
			fprintf(f, "%02x", buf[i]);
			for (unsigned j = 1; j < 0x10; j++) {
				fprintf(f, " %02x", buf[i + j]);
			}
			fprintf(f, "\n");
		}
		if (!crc_ok(buf))
			fprintf(f, "Block %u has a checksum error (should be 0x%02x)\n",
					b, crc_calc(buf));
	}
}

static void rawdumpedid(FILE *f, struct v4l2_edid *e)
{
	for (unsigned b = 0; b < e->blocks; b++) {
		unsigned char *buf = e->edid + 128 * b;

		for (unsigned i = 0; i < 128; i++)
			fprintf(f, "%c", buf[i]);
		if (!crc_ok(buf))
			fprintf(stderr, "Block %u has a checksum error (should be %02x)\n",
					b, crc_calc(buf));
	}
}

static void carraydumpedid(FILE *f, struct v4l2_edid *e)
{
	fprintf(f, "unsigned char edid[] = {\n");
	for (unsigned b = 0; b < e->blocks; b++) {
		unsigned char *buf = e->edid + 128 * b;

		if (b)
			fprintf(f, "\n");
		for (unsigned i = 0; i < 128; i += 8) {
			fprintf(f, "\t0x%02x,", buf[i]);
			for (unsigned j = 1; j < 8; j++) {
				fprintf(f, " 0x%02x,", buf[i + j]);
			}
			fprintf(f, "\n");
		}
		if (!crc_ok(buf))
			fprintf(f, "\t/* Block %u has a checksum error (should be 0x%02x) */\n",
					b, crc_calc(buf));
	}
	fprintf(f, "};\n");
}

static void printedid(FILE *f, struct v4l2_edid *e, enum format gf)
{
	switch (gf) {
	default:
	case HEX:
		hexdumpedid(f, e);
		break;
	case RAW:
		rawdumpedid(f, e);
		break;
	case CARRAY:
		carraydumpedid(f, e);
		break;
	}
}

/****************** EDIDs *****************************/
static uint8_t dvid_edid[128] = {
	0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00,
	0x63, 0x3a, 0xaa, 0x55, 0x00, 0x00, 0x00, 0x00,
	0x0a, 0x18, 0x01, 0x03, 0x80, 0x10, 0x09, 0x78,
	0x0e, 0x00, 0xb2, 0xa0, 0x57, 0x49, 0x9b, 0x26,
	0x10, 0x48, 0x4f, 0x2f, 0xcf, 0x00, 0x31, 0x59,
	0x45, 0x59, 0x81, 0x80, 0x81, 0x40, 0x90, 0x40,
	0x95, 0x00, 0xa9, 0x40, 0xb3, 0x00, 0x02, 0x3a,
	0x80, 0x18, 0x71, 0x38, 0x2d, 0x40, 0x58, 0x2c,
	0x46, 0x00, 0x10, 0x09, 0x00, 0x00, 0x00, 0x1e,
	0x00, 0x00, 0x00, 0xfd, 0x00, 0x18, 0x55, 0x18,
	0x5e, 0x11, 0x00, 0x0a, 0x20, 0x20, 0x20, 0x20,
	0x20, 0x20, 0x00, 0x00, 0x00, 0xfc, 0x00,  'v',
	'4',   'l',  '2',  '-',  'd',  'v',  'i',  'd',
	0x0a, 0x0a, 0x0a, 0x0a, 0x00, 0x00, 0x00, 0x10,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xec
};
static uint8_t vga_edid[128] = {
	0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00,
	0x63, 0x3a, 0xaa, 0x55, 0x00, 0x00, 0x00, 0x00,
	0x0a, 0x18, 0x01, 0x03, 0x08, 0x10, 0x09, 0x78,
	0x0a, 0x00, 0xb2, 0xa0, 0x57, 0x49, 0x9b, 0x26,
	0x10, 0x48, 0x4f, 0x2f, 0xcf, 0x00, 0x31, 0x59,
	0x45, 0x59, 0x61, 0x59, 0x81, 0x40, 0x81, 0x80,
	0x95, 0x00, 0xa9, 0x40, 0xb3, 0x00, 0x02, 0x3a,
	0x80, 0x18, 0x71, 0x38, 0x2d, 0x40, 0x58, 0x2c,
	0x45, 0x00, 0x10, 0x09, 0x00, 0x00, 0x00, 0x1e,
	0x00, 0x00, 0x00, 0xfd, 0x00, 0x31, 0x55, 0x18,
	0x5e, 0x11, 0x00, 0x0a, 0x20, 0x20, 0x20, 0x20,
	0x20, 0x20, 0x00, 0x00, 0x00, 0xfc, 0x00,  'v',
	'4',   'l',  '2',  '-',  'v',  'g',  'a', 0x0a,
	0x0a, 0x0a, 0x0a, 0x0a, 0x00, 0x00, 0x00, 0x10,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc5
};
static uint8_t hdmi_edid[256] = {
	0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00,
	0x63, 0x3a, 0xaa, 0x55, 0x00, 0x00, 0x00, 0x00,
	0x0a, 0x18, 0x01, 0x03, 0x80, 0x10, 0x09, 0x78,
	0x0e, 0x00, 0xb2, 0xa0, 0x57, 0x49, 0x9b, 0x26,
	0x10, 0x48, 0x4f, 0x2f, 0xcf, 0x00, 0x31, 0x59,
	0x45, 0x59, 0x81, 0x80, 0x81, 0x40, 0x90, 0x40,
	0x95, 0x00, 0xa9, 0x40, 0xb3, 0x00, 0x02, 0x3a,
	0x80, 0x18, 0x71, 0x38, 0x2d, 0x40, 0x58, 0x2c,
	0x46, 0x00, 0x10, 0x09, 0x00, 0x00, 0x00, 0x1e,
	0x00, 0x00, 0x00, 0xfd, 0x00, 0x18, 0x55, 0x18,
	0x5e, 0x11, 0x00, 0x0a, 0x20, 0x20, 0x20, 0x20,
	0x20, 0x20, 0x00, 0x00, 0x00, 0xfc, 0x00,  'v',
	'4',   'l',  '2',  '-',  'h',  'd',  'm',  'i',
	0x0a, 0x0a, 0x0a, 0x0a, 0x00, 0x00, 0x00, 0x10,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0xf0,

	0x02, 0x03, 0x1a, 0xc0, 0x48, 0xa2, 0x10, 0x04,
	0x02, 0x01, 0x21, 0x14, 0x13, 0x23, 0x09, 0x07,
	0x07, 0x65, 0x03, 0x0c, 0x00, 0x10, 0x00, 0xe2,
	0x00, 0xea, 0x01, 0x1d, 0x00, 0x80, 0x51, 0xd0,
	0x1c, 0x20, 0x40, 0x80, 0x35, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x1e, 0x8c, 0x0a, 0xd0, 0x8a,
	0x20, 0xe0, 0x2d, 0x10, 0x10, 0x3e, 0x96, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x17
};
/******************************************************/


static struct v4l2_edid sedid;
static char *file_in;

static struct v4l2_edid gedid;
static char *file_out;
static enum format gformat;
static unsigned clear_pad;

void edid_cmd(int ch, char *optarg)
{
	char *value, *subs;

	switch (ch) {
	case OptSetEdid:
		memset(&sedid, 0, sizeof(sedid));
		file_in = NULL;
		if (!optarg)
			break;
		subs = optarg;
		while (*subs != '\0') {
			static const char *const subopts[] = {
				"pad",
				"edid",
				"file",
				NULL
			};
			switch (parse_subopt(&subs, subopts, &value)) {
			case 0:
				sedid.pad = strtoul(value, 0, 0);
				break;
			case 1:
				if (!strcmp(value, "dvid")) {
					sedid.edid = dvid_edid;
					sedid.blocks = sizeof(dvid_edid) / 128;
				} else if (!strcmp(value, "vga")) {
					sedid.edid = vga_edid;
					sedid.blocks = sizeof(vga_edid) / 128;
				} else if (!strcmp(value, "hdmi")) {
					sedid.edid = hdmi_edid;
					sedid.blocks = sizeof(hdmi_edid) / 128;
				} else {
					edid_usage();
					exit(1);
				}
				if (file_in) {
					fprintf(stderr, "The edid and file options can't be used together.\n");
					exit(1);
				}
				break;
			case 2:
				if (value) {
					file_in = value;
					if (sedid.edid) {
						fprintf(stderr, "The edid and file options can't be used together.\n");
						exit(1);
					}
				}
				break;
			default:
				edid_usage();
				exit(1);
			}
		}
		break;

	case OptClearEdid:
		if (optarg)
			clear_pad = strtoul(optarg, 0, 0);
		break;

	case OptGetEdid:
		memset(&gedid, 0, sizeof(gedid));
		gedid.blocks = 256; /* default all blocks */
		gformat = HEX; /* default hex output */
		file_out = NULL;
		if (!optarg)
			break;
		subs = optarg;
		while (*subs != '\0') {
			static const char *const subopts[] = {
				"pad",
				"startblock",
				"blocks",
				"format",
				"file",
				NULL
			};
			switch (parse_subopt(&subs, subopts, &value)) {
			case 0:
				gedid.pad = strtoul(value, 0, 0);
				break;
			case 1:
				gedid.start_block = strtoul(value, 0, 0);
				if (gedid.start_block > 255) {
					fprintf(stderr, "startblock %d too large, max 255\n", gedid.start_block);
					exit(1);
				}
				break;
			case 2:
				gedid.blocks = strtoul(value, 0, 0);
				break;
			case 3:
				if (!strcmp(value, "hex")) {
					gformat = HEX;
				} else if (!strcmp(value, "raw")) {
					gformat = RAW;
				} else if (!strcmp(value, "carray")) {
					gformat = CARRAY;
				} else {
					edid_usage();
					exit(1);
				}
				break;
			case 4:
				if (value)
					file_out = value;
				break;
			default:
				edid_usage();
				exit(1);
			}
		}
		if (gedid.start_block + gedid.blocks > 256)
			gedid.blocks = 256 - gedid.start_block;
	}
}

void edid_set(int fd)
{
	if (options[OptClearEdid]) {
		struct v4l2_edid edid;

		memset(&edid, 0, sizeof(edid));
		edid.pad = clear_pad;
		doioctl(fd, VIDIOC_S_EDID, &sedid);
	}

	if (options[OptSetEdid]) {
		FILE *fin = NULL;

		if (file_in) {
			if (!strcmp(file_in, "-"))
				fin = stdin;
			else
				fin = fopen(file_in, "r");
			if (!fin) {
				fprintf(stderr, "Failed to open %s: %s\n", file_in,
						strerror(errno));
				exit(1);
			}
		}
		if (fin) {
			read_edid_file(fin, &sedid);
			if (sedid.blocks == 0) {
				fprintf(stderr, "%s contained an empty EDID, ignoring.\n",
						file_in ? file_in : "stdin");
				exit(1);
			}
		}
		if (options[OptFixEdidChecksums])
			fix_edid(&sedid);
		if (verify_edid(&sedid))
			doioctl(fd, VIDIOC_S_EDID, &sedid);
		else
			fprintf(stderr, "EDID not set due to checksum errors\n");
		if (fin) {
			if (sedid.edid) {
				free(sedid.edid);
				sedid.edid = NULL;
			}
			if (fin != stdin)
				fclose(fin);
		}
	}
}

void edid_get(int fd)
{
	if (options[OptGetEdid]) {
		FILE *fout = stdout;

		if (file_out) {
			if (!strcmp(file_out, "-"))
				fout = stdout;
			else
				fout = fopen(file_out, "w+");
			if (!fout) {
				fprintf(stderr, "Failed to open %s: %s\n", file_out,
						strerror(errno));
				exit(1);
			}
		}
		gedid.edid = (unsigned char *)malloc(gedid.blocks * 128);
		if (doioctl(fd, VIDIOC_G_EDID, &gedid) == 0) {
			if (options[OptFixEdidChecksums])
				fix_edid(&gedid);
			printedid(fout, &gedid, gformat);
		}
		if (file_out && fout != stdout)
			fclose(fout);
		free(gedid.edid);
	}
}
