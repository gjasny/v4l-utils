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

/*
 * The 24-bit IEEE Registration Identifier for the HDMI-LLC Vendor
 * Specific Data Block.
 */
#define HDMI_VSDB_EXT_TAG	0x000c03
/*
 * The 24-bit IEEE Registration Identifier for the HDMI-Forum Vendor
 * Specific Data Block.
 */
#define HF_VSDB_EXT_TAG		0xc45dd8

#define VID_CAP_EXT_TAG		0
#define COLORIMETRY_EXT_TAG	5
#define HDR_MD_EXT_TAG		6

#define VSDB_TAG		3
#define SPEAKER_TAG		4
#define EXTENDED_TAG		7

enum format {
	HEX,
	RAW,
	CARRAY
};

static struct v4l2_edid sedid;
static char *file_in;

static struct v4l2_edid gedid;
static struct v4l2_edid info_edid;
static char *file_out;
static enum format gformat;
static enum format sformat;
static unsigned clear_pad;
static long phys_addr = -1;

static __u8 toggle_cea861_hdr_flags;
#define CEA861_HDR_UNDERSCAN	(1 << 6)
#define CEA861_HDR_AUDIO	(1 << 6)
#define CEA861_HDR_YCBCR444	(1 << 5)
#define CEA861_HDR_YCBCR422	(1 << 4)

static __u8 toggle_speaker1_flags;
#define SPEAKER1_FLFR		(1 << 0)
#define SPEAKER1_LFE		(1 << 1)
#define SPEAKER1_FC		(1 << 2)
#define SPEAKER1_RLRR		(1 << 3)
#define SPEAKER1_RC		(1 << 4)
#define SPEAKER1_FLCFRC		(1 << 5)
#define SPEAKER1_RLCRRC		(1 << 6)
#define SPEAKER1_FLWFRW		(1 << 7)

static __u8 toggle_speaker2_flags;
#define SPEAKER2_FLHFRH		(1 << 0)
#define SPEAKER2_TC		(1 << 1)
#define SPEAKER2_FCH		(1 << 2)

static __u8 toggle_hdmi_vsdb_dc_flags;
#define HDMI_VSDB_Y444_BIT	(1 << 3)
#define HDMI_VSDB_30_BIT	(1 << 4)
#define HDMI_VSDB_36_BIT	(1 << 5)
#define HDMI_VSDB_48_BIT	(1 << 6)
static __u8 toggle_hdmi_vsdb_cnc_flags;
#define HDMI_VSDB_GRAPHICS	(1 << 0)
#define HDMI_VSDB_PHOTO		(1 << 1)
#define HDMI_VSDB_CINEMA	(1 << 2)
#define HDMI_VSDB_GAME		(1 << 3)
#define HDMI_VSDB_I_LATENCY	(1 << 6)
#define HDMI_VSDB_LATENCY	(1 << 7)

static __u8 toggle_hf_vsdb_flags;
#define HF_VSDB_SCSD_PRESENT	(1 << 7)

static int mod_s_pt = -1;
static int mod_s_it = -1;
static int mod_s_ce = -1;
static __u8 toggle_vid_cap_flags;
#define VID_CAP_QS		(1 << 6)
#define VID_CAP_QY		(1 << 7)

static __u8 toggle_colorimetry_flags;
#define COLORIMETRY_XVYCC601		(1 << 0)
#define COLORIMETRY_XVYCC709		(1 << 1)
#define COLORIMETRY_SYCC		(1 << 2)
#define COLORIMETRY_ADOBEYCC		(1 << 3)
#define COLORIMETRY_ADOBERGB		(1 << 4)
#define COLORIMETRY_BT2020CYCC		(1 << 5)
#define COLORIMETRY_BT2020YCC		(1 << 6)
#define COLORIMETRY_BT2020RGB		(1 << 7)

static __u8 toggle_hdr_md_flags;
#define HDR_MD_SDR		(1 << 0)
#define HDR_MD_HDR		(1 << 1)
#define HDR_MD_SMPTE_2084	(1 << 2)

void edid_usage(void)
{
	printf("\nEDID options:\n"
	       "  --set-edid=pad=<pad>[,type=<type>|file=<file>][,format=<fmt>][modifiers]\n"
	       "                     <pad> is the input or output index for which to set the EDID.\n"
	       "                     <type> can be 'hdmi', 'hdmi-4k-170mhz', 'hdmi-4k-300mhz', 'hdmi-4k-600mhz',\n"
	       "                     'dvid' or 'vga'. A predefined EDID suitable for that connector type will be\n"
	       "                     set. It has a 1920x1080p60 native resolution for the non-4k variants and a\n"
	       "                     3840x2160 resolution for the 4k variants (4kp30 YCbCr 4:2:0 for 170mhz, 4kp30\n"
	       "                     for 300 mhz and 4kp60 for 600 mhz).\n"
	       "                     If <file> is '-', then the data is read from stdin, otherwise it is\n"
	       "                     read from the given file. The file format must be in hex as in get-edid.\n"
	       "                     The 'type' or 'file' arguments are mutually exclusive. One of the two\n"
	       "                     must be specified.\n"
	       "                     <fmt> is one of:\n"
	       "                     hex:    hex numbers in ascii text (default)\n"
	       "                     raw:    raw binary EDID content\n"
	       "\n"
	       "                     [modifiers] is a comma-separate list of EDID modifiers:\n"
	       "\n"
	       "                     CEA-861 Header modifiers:\n"
	       "                     underscan: toggle the underscan bit.\n"
	       "                     audio: toggle the audio bit.\n"
	       "                     ycbcr444: toggle the YCbCr 4:4:4 bit.\n"
	       "                     ycbcr422: toggle the YCbCr 4:2:2 bit.\n"
	       "\n"
	       "                     Speaker Allocation Data Block modifiers:\n"
	       "                     fl-fr: Front Left and Front Right.\n"
	       "                     lfe: Low Frequence Effects.\n"
	       "                     fc: Front Center.\n"
	       "                     rl-rr: Rear Left and Rear Right.\n"
	       "                     rc: Rear Center.\n"
	       "                     flc-frc: Front Left Center and Front Right Center.\n"
	       "                     rlc-rrc: Rear Left Center and Rear Right Center.\n"
	       "                     flw-frw: Front Left Wide and Front Right Wide.\n"
	       "                     flh-frh: Front Left High and Front Right High.\n"
	       "                     tc: Top Center.\n"
	       "                     fch: Front Center High.\n"
	       "\n"
	       "                     HDMI Vendor-Specific Data Block modifiers:\n"
	       "                     pa=<pa>: change the physical address.\n"
	       "                     y444: toggle the YCbCr 4:4:4 Deep Color bit.\n"
	       "                     30-bit: toggle the 30 bits/pixel bit.\n"
	       "                     36-bit: toggle the 36 bits/pixel bit.\n"
	       "                     48-bit: toggle the 48 bits/pixel bit.\n"
	       "                     graphics: toggle the Graphics Content Type bit.\n"
	       "                     photo: toggle the Photo Content Type bit.\n"
	       "                     cinema: toggle the Cinema Content Type bit.\n"
	       "                     game: toggle the Game Content Type bit.\n"
	       "\n"
	       "                     HDMI Forum Vendor-Specific Data Block modifiers:\n"
	       "                     scdc: toggle the SCDC Present bit.\n"
	       "\n"
	       "                     CEA-861 Video Capability Descriptor modifiers:\n"
	       "                     qy: toggle the QY YCC Quantization Range bit.\n"
	       "                     qs: toggle the QS RGB Quantization Range bit.\n"
	       "                     s-pt=<0-3>: set the PT Preferred Format Over/underscan bits.\n"
	       "                     s-it=<0-3>: set the IT Over/underscan bits.\n"
	       "                     s-ce=<0-3>: set the CE Over/underscan bits.\n"
	       "\n"
	       "                     CEA-861 Colorimetry Data Block modifiers:\n"
	       "                     xvycc-601: toggle the xvYCC 601 bit.\n"
	       "                     xvycc-709: toggle the xvYCC 709 bit.\n"
	       "                     sycc: toggle the sYCC 601 bit.\n"
	       "                     adobe-ycc: toggle the Adobe YCC 601 bit.\n"
	       "                     adobe-rgb: toggle the Adobe RGB bit.\n"
	       "                     bt2020-rgb: toggle the BT2020 RGB bit.\n"
	       "                     bt2020-ycc: toggle the BT2020 YCC bit.\n"
	       "                     bt2020-cycc: toggle the BT2020 cYCC bit.\n"
	       "\n"
	       "                     CEA-861 HDR Static Metadata Data Block modifiers:\n"
	       "                     sdr: toggle the Traditional gamma SDR bit.\n"
	       "                     hdr: toggle the Traditional gamma HDR bit.\n"
	       "                     smpte2084: toggle the SMPTE ST 2084 bit.\n"
	       "  --clear-edid=<pad>\n"
	       "                     <pad> is the input or output index for which to clear the EDID.\n"
	       "  --info-edid=<pad>  print the current EDID's modifiers\n"
	       "                     <pad> is the input or output index for which to get the EDID.\n"
	       "  --get-edid=pad=<pad>,startblock=<startblock>,blocks=<blocks>,format=<fmt>,file=<file>\n"
	       "                     <pad> is the input or output index for which to get the EDID.\n"
	       "                     <startblock> is the first block number you want to read. Default 0.\n"
	       "                     <blocks> is the number of blocks you want to read. Default is\n"
	       "                     all blocks.\n"
	       "                     <fmt> is one of:\n"
	       "                     hex:    hex numbers in ascii text (default)\n"
	       "                     raw:    can be piped directly into the edid-decode tool\n"
	       "                     carray: c-program struct\n"
	       "                     If <file> is '-' or not the 'file' argument is not supplied, then the data\n"
	       "                     is written to stdout.\n"
	       "  --fix-edid-checksums\n"
	       "                     If specified then any checksum errors will be fixed silently.\n"
	       );
}

static void edid_add_block(struct v4l2_edid *e)
{
	e->blocks++;
	if (e->blocks > 256) {
		fprintf(stderr, "edid file error: too long\n");
		free(e->edid);
		e->edid = NULL;
		exit(1);
	}
	e->edid = (unsigned char *)realloc(e->edid, e->blocks * 128);
}

static void read_edid_file(FILE *f, struct v4l2_edid *e)
{
	char value[3] = { 0 };
	unsigned i = 0;
	int c;

	fseek(f, SEEK_SET, 0);
	e->edid = NULL;
	e->blocks = 0;

	while ((c = fgetc(f)) != EOF) {
		if (sformat == RAW) {
			if (i % 256 == 0)
				edid_add_block(e);
			e->edid[i / 2] = c;
			i += 2;
			continue;
		}
		/* Handle '0x' prefix */
		if ((i & 1) && value[0] == '0' && (c == 'x' || c == 'X'))
			i--;
		if (!isxdigit(c))
			continue;
		if (i & 0x01) {
			value[1] = c;
			if (i % 256 == 1)
				edid_add_block(e);
			e->edid[i / 2] = strtoul(value, 0, 16);
		} else {
			value[0] = c;
		}
		i++;
	}
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

static int get_edid_tag_location(const unsigned char *edid, unsigned size,
				 unsigned char want_tag, __u32 ext_tag)
{
	unsigned char d;

	if (size < 256)
		return -1;

	if (edid[0x7e] != 1 || edid[0x80] != 0x02 || edid[0x81] != 0x03)
		return -1;

	/* search tag */
	d = edid[0x82] & 0x7f;
	if (d <= 4)
		return -1;

	int i = 0x84;
	int end = 0x80 + d;

	do {
		unsigned char tag = edid[i] >> 5;
		unsigned char len = edid[i] & 0x1f;

		if (tag != want_tag || i + len > end) {
			i += len + 1;
			continue;
		}

		/*
		 * Tag 3 (Vendor-Specific Data Block) has
		 * a 24 bit IEEE identifier.
		 */
		if (tag == VSDB_TAG && len >= 3 &&
		    edid[i + 1] == (ext_tag & 0xff) &&
		    edid[i + 2] == ((ext_tag >> 8) & 0xff) &&
		    edid[i + 3] == ((ext_tag >> 16) & 0xff))
			return i;
		/*
		 * Tag 7 has an extended tag, others (0-2, 4-6)
		 * have no identifiers.
		 */
		if ((tag < EXTENDED_TAG && tag != VSDB_TAG) ||
		    (tag == EXTENDED_TAG && len >= 1 && edid[i + 1] == ext_tag))
			return i;
		i += len + 1;
	} while (i < end);
	return -1;
}

static int get_edid_cea861_hdr_location(const unsigned char *edid, unsigned size)
{
	if (size < 256)
		return -1;

	if (edid[0x7e] != 1 || edid[0x80] != 0x02 || edid[0x81] != 0x03)
		return -1;

	return 0x83;
}

static int get_edid_spa_location(const unsigned char *edid, unsigned size)
{
	int loc = get_edid_tag_location(edid, size, VSDB_TAG, HDMI_VSDB_EXT_TAG);

	if (loc < 0)
		return loc;

	return (edid[loc] & 0x1f) >= 5 ? loc + 4 : -1;
}

static int get_edid_hdmi_vsdb_location(const unsigned char *edid, unsigned size)
{
	int loc = get_edid_tag_location(edid, size, VSDB_TAG, HDMI_VSDB_EXT_TAG);

	if (loc < 0)
		return loc;

	return (edid[loc] & 0x1f) >= 5 ? loc : -1;
}

static int get_edid_hf_vsdb_location(const unsigned char *edid, unsigned size)
{
	int loc = get_edid_tag_location(edid, size, VSDB_TAG, HF_VSDB_EXT_TAG);

	if (loc < 0)
		return loc;

	return (edid[loc] & 0x1f) >= 6 ? loc + 5 : -1;
}

static int get_edid_speaker_location(const unsigned char *edid, unsigned size)
{
	int loc = get_edid_tag_location(edid, size, SPEAKER_TAG, 0);

	if (loc < 0)
		return loc;

	return (edid[loc] & 0x1f) >= 3 ? loc + 1 : -1;
}

static int get_edid_vid_cap_location(const unsigned char *edid, unsigned size)
{
	int loc = get_edid_tag_location(edid, size, EXTENDED_TAG, VID_CAP_EXT_TAG);

	if (loc < 0)
		return loc;

	return (edid[loc] & 0x1f) >= 2 ? loc + 2 : -1;
}

static int get_edid_colorimetry_location(const unsigned char *edid, unsigned size)
{
	int loc = get_edid_tag_location(edid, size, EXTENDED_TAG, COLORIMETRY_EXT_TAG);

	if (loc < 0)
		return loc;

	return (edid[loc] & 0x1f) >= 3 ? loc + 2 : -1;
}

static int get_edid_hdr_md_location(const unsigned char *edid, unsigned size)
{
	int loc = get_edid_tag_location(edid, size, EXTENDED_TAG, HDR_MD_EXT_TAG);

	if (loc < 0)
		return loc;

	return (edid[loc] & 0x1f) >= 3 ? loc + 2 : -1;
}

static void set_edid_phys_addr(unsigned char *edid, unsigned size, unsigned short phys_addr)
{
	int loc = get_edid_spa_location(edid, size);
	unsigned char sum = 0;
	int i;

	if (loc < 0)
		return;
	edid[loc] = phys_addr >> 8;
	edid[loc + 1] = phys_addr & 0xff;
	loc &= ~0x7f;

	for (i = loc; i < loc + 127; i++)
		sum += edid[i];
	edid[i] = 256 - sum;
}

static unsigned short get_edid_phys_addr(const unsigned char *edid, unsigned size)
{
	int loc = get_edid_spa_location(edid, size);

	if (loc < 0)
		return 0xffff;
	return (edid[loc] << 8) | edid[loc + 1];
}

static void print_edid_mods(const struct v4l2_edid *e)
{
	unsigned short pa = get_edid_phys_addr(e->edid, e->blocks * 128);
	int loc;

	loc = get_edid_cea861_hdr_location(e->edid, e->blocks * 128);
	if (loc >= 0) {
		__u8 v = e->edid[loc];

		printf("\nCEA-861 Header\n");
		printf("  IT Formats Underscanned: %s\n", (v & CEA861_HDR_UNDERSCAN) ? "yes" : "no");
		printf("  Audio:                   %s\n", (v & CEA861_HDR_AUDIO) ? "yes" : "no");
		printf("  YCbCr 4:4:4:             %s\n", (v & CEA861_HDR_YCBCR444) ? "yes" : "no");
		printf("  YCbCr 4:2:2:             %s\n", (v & CEA861_HDR_YCBCR422) ? "yes" : "no");
	}
	loc = get_edid_speaker_location(e->edid, e->blocks * 128);
	if (loc >= 0) {
		__u8 v = e->edid[loc];

		printf("\nSpeaker Allocation Data Block\n");
		printf("  FL/FR:                   %s\n", (v & SPEAKER1_FLFR) ? "yes" : "no");
		printf("  LFE:                     %s\n", (v & SPEAKER1_LFE) ? "yes" : "no");
		printf("  FC:                      %s\n", (v & SPEAKER1_FC) ? "yes" : "no");
		printf("  RL/RR:                   %s\n", (v & SPEAKER1_RLRR) ? "yes" : "no");
		printf("  RC:                      %s\n", (v & SPEAKER1_RC) ? "yes" : "no");
		printf("  FLC/FRC:                 %s\n", (v & SPEAKER1_FLCFRC) ? "yes" : "no");
		printf("  RLC/RRC:                 %s\n", (v & SPEAKER1_RLCRRC) ? "yes" : "no");
		printf("  FLW/FRW:                 %s\n", (v & SPEAKER1_FLWFRW) ? "yes" : "no");

		v = e->edid[loc + 1];
		printf("  FLH/FRH:                 %s\n", (v & SPEAKER2_FLHFRH) ? "yes" : "no");
		printf("  TC:                      %s\n", (v & SPEAKER2_TC) ? "yes" : "no");
		printf("  FCH:                     %s\n", (v & SPEAKER2_FCH) ? "yes" : "no");
	}
	loc = get_edid_hdmi_vsdb_location(e->edid, e->blocks * 128);
	if (loc >= 0) {
		__u8 len = e->edid[loc] & 0x1f;
		__u8 v = len >= 7 ? e->edid[loc + 7] : 0;

		printf("\nHDMI Vendor-Specific Data Block\n");
		if (v)
			printf("  Max TMDS Clock:          %u MHz\n", v * 5); 
		printf("  Physical Address:        %x.%x.%x.%x\n",
		       pa >> 12, (pa >> 8) & 0xf, (pa >> 4) & 0xf, pa & 0xf);
		if (len >= 6) {
			v = e->edid[loc + 6];
			printf("  YCbCr 4:4:4 Deep Color:  %s\n", (v & HDMI_VSDB_Y444_BIT) ? "yes" : "no");
			printf("  30-bit:                  %s\n", (v & HDMI_VSDB_30_BIT) ? "yes" : "no");
			printf("  36-bit:                  %s\n", (v & HDMI_VSDB_36_BIT) ? "yes" : "no");
			printf("  48-bit:                  %s\n", (v & HDMI_VSDB_48_BIT) ? "yes" : "no");
		}
		if (len >= 8) {
			v = e->edid[loc + 8];
			printf("  Graphics:                %s\n", (v & HDMI_VSDB_GRAPHICS) ? "yes" : "no");
			printf("  Photo:                   %s\n", (v & HDMI_VSDB_PHOTO) ? "yes" : "no");
			printf("  Cinema:                  %s\n", (v & HDMI_VSDB_CINEMA) ? "yes" : "no");
			printf("  Game:                    %s\n", (v & HDMI_VSDB_GAME) ? "yes" : "no");
			if ((v & HDMI_VSDB_LATENCY) && len >= 10) {
				__u8 lat = e->edid[loc + 9];
				if (lat == 255)
					printf("  Video Latency:           unsupported\n");
				else if (lat > 0)
					printf("  Video Latency:           %u.%ums\n",
					       (lat - 1) / 2, ((lat - 1) & 1) ? 0 : 5);
				lat = e->edid[loc + 10];
				if (lat == 255)
					printf("  Audio Latency:           unsupported\n");
				else if (lat > 0)
					printf("  Audio Latency:           %u.%ums\n",
					       (lat - 1) / 2, ((lat - 1) & 1) ? 0 : 5);
			}
			if ((v & HDMI_VSDB_I_LATENCY) && len >= 12) {
				__u8 lat = e->edid[loc + 11];
				if (lat == 255)
					printf("  IL Video Latency:        unsupported\n");
				else if (lat > 0)
					printf("  IL Video Latency:        %u.%ums\n",
					       (lat - 1) / 2, ((lat - 1) & 1) ? 0 : 5);
				lat = e->edid[loc + 12];
				if (lat == 255)
					printf("  IL Audio Latency:        unsupported\n");
				else if (lat > 0)
					printf("  IL Audio Latency:        %u.%ums\n",
					       (lat - 1) / 2, ((lat - 1) & 1) ? 0 : 5);
			}
		}
	}
	loc = get_edid_hf_vsdb_location(e->edid, e->blocks * 128);
	if (loc >= 0) {
		__u8 v = e->edid[loc];

		printf("\nHDMI Forum Vendor-Specific Data Block\n");
		if (v)
			printf("  Max TMDS Character Rate: %u MHz\n", v * 5); 
		v = e->edid[loc + 1];
		printf("  SCDC Present:            %s\n", (v & HF_VSDB_SCSD_PRESENT) ? "yes" : "no");
	}
	loc = get_edid_vid_cap_location(e->edid, e->blocks * 128);
	if (loc >= 0) {
		static const char *pt_scan[] = {
			"No Data",
			"Always Overscanned",
			"Always Underscanned",
			"Supports both over- and underscan"
		};
		static const char *it_scan[] = {
			"IT Formats not supported",
			"Always Overscanned",
			"Always Underscanned",
			"Supports both over- and underscan"
		};
		static const char *ce_scan[] = {
			"CE Formats not supported",
			"Always Overscanned",
			"Always Underscanned",
			"Supports both over- and underscan"
		};
		__u8 v = e->edid[loc];

		printf("\nCEA-861 Video Capability Descriptor\n");
		printf("  RGB Quantization Range:  %s\n", (v & VID_CAP_QS) ? "yes" : "no");
		printf("  YCC Quantization Range:  %s\n", (v & VID_CAP_QY) ? "yes" : "no");
		printf("  PT:                      %s\n", pt_scan[(v >> 4) & 3]);
		printf("  IT:                      %s\n", it_scan[(v >> 2) & 3]);
		printf("  CE:                      %s\n", ce_scan[(v >> 0) & 3]);
	}
	loc = get_edid_colorimetry_location(e->edid, e->blocks * 128);
	if (loc >= 0) {
		__u8 v = e->edid[loc];

		printf("\nCEA-861 Colorimetry Data Block\n");
		printf("  xvYCC 601:               %s\n", (v & COLORIMETRY_XVYCC601) ? "yes" : "no");
		printf("  xvYCC 709:               %s\n", (v & COLORIMETRY_XVYCC709) ? "yes" : "no");
		printf("  sYCC:                    %s\n", (v & COLORIMETRY_SYCC) ? "yes" : "no");
		printf("  AdobeRGB:                %s\n", (v & COLORIMETRY_ADOBERGB) ? "yes" : "no");
		printf("  AdobeYCC:                %s\n", (v & COLORIMETRY_ADOBEYCC) ? "yes" : "no");
		printf("  BT.2020 RGB:             %s\n", (v & COLORIMETRY_BT2020RGB) ? "yes" : "no");
		printf("  BT.2020 YCC:             %s\n", (v & COLORIMETRY_BT2020YCC) ? "yes" : "no");
		printf("  BT.2020 cYCC:            %s\n", (v & COLORIMETRY_BT2020CYCC) ? "yes" : "no");
	}
	loc = get_edid_hdr_md_location(e->edid, e->blocks * 128);
	if (loc >= 0) {
		__u8 v = e->edid[loc];

		printf("\nCEA-861 HDR Static Metadata Data Block\n");
		printf("  SDR (Traditional Gamma): %s\n", (v & HDR_MD_SDR) ? "yes" : "no");
		printf("  HDR (Traditional Gamma): %s\n", (v & HDR_MD_HDR) ? "yes" : "no");
		printf("  SMPTE 2084:              %s\n", (v & HDR_MD_SMPTE_2084) ? "yes" : "no");
	}
}

static unsigned short parse_phys_addr(const char *value)
{
	unsigned p1, p2, p3, p4;

	if (!strchr(value, '.'))
		return strtoul(value, NULL, 0);
	if (sscanf(value, "%x.%x.%x.%x", &p1, &p2, &p3, &p4) != 4) {
		fprintf(stderr, "Expected a physical address of the form x.x.x.x\n");
		return 0xffff;
	}
	if (p1 > 0xf || p2 > 0xf || p3 > 0xf || p4 > 0xf) {
		fprintf(stderr, "Physical address components should never be larger than 0xf\n");
		return 0xffff;
	}
	return (p1 << 12) | (p2 << 8) | (p3 << 4) | p4;
}

/****************** EDIDs *****************************/
static uint8_t vga_edid[128] = {
	0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00,
	0x31, 0xd8, 0x34, 0x12, 0x00, 0x00, 0x00, 0x00,
	0x22, 0x1a, 0x01, 0x04, 0x08, 0x30, 0x1e, 0x78,
	0x0f, 0xee, 0x91, 0xa3, 0x54, 0x4c, 0x99, 0x26,
	0x0f, 0x50, 0x54, 0x2f, 0xcf, 0x00, 0x31, 0x59,
	0x45, 0x59, 0x61, 0x59, 0x81, 0x40, 0x81, 0x80,
	0x95, 0x00, 0xa9, 0x40, 0xb3, 0x00, 0x28, 0x3c,
	0x80, 0xa0, 0x70, 0xb0, 0x23, 0x40, 0x30, 0x20,
	0x36, 0x00, 0xe0, 0x2c, 0x11, 0x00, 0x00, 0x1a,
	0x00, 0x00, 0x00, 0xfd, 0x00, 0x31, 0x55, 0x18,
	0x5e, 0x11, 0x04, 0x12, 0x00, 0xf0, 0xf8, 0x58,
	0xf0, 0x3c, 0x00, 0x00, 0x00, 0xfc, 0x00, 0x76,
	0x67, 0x61, 0x0a, 0x20, 0x20, 0x20, 0x20, 0x20,
	0x20, 0x20, 0x20, 0x20, 0x00, 0x00, 0x00, 0x10,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xce,
};

static uint8_t dvid_edid[128] = {
	0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00,
	0x31, 0xd8, 0x34, 0x12, 0x00, 0x00, 0x00, 0x00,
	0x22, 0x1a, 0x01, 0x04, 0xa1, 0x30, 0x1e, 0x78,
	0x07, 0xee, 0x91, 0xa3, 0x54, 0x4c, 0x99, 0x26,
	0x0f, 0x50, 0x54, 0x2f, 0xcf, 0x00, 0x31, 0x59,
	0x45, 0x59, 0x81, 0x80, 0x81, 0x40, 0x90, 0x40,
	0x95, 0x00, 0xa9, 0x40, 0xb3, 0x00, 0x28, 0x3c,
	0x80, 0xa0, 0x70, 0xb0, 0x23, 0x40, 0x30, 0x20,
	0x36, 0x00, 0xe0, 0x2c, 0x11, 0x00, 0x00, 0x1a,
	0x00, 0x00, 0x00, 0xfd, 0x00, 0x18, 0x55, 0x18,
	0x5e, 0x11, 0x04, 0x12, 0x00, 0xf0, 0xf8, 0x58,
	0xf0, 0x3c, 0x00, 0x00, 0x00, 0xfc, 0x00, 0x64,
	0x76, 0x69, 0x2d, 0x64, 0x0a, 0x20, 0x20, 0x20,
	0x20, 0x20, 0x20, 0x20, 0x00, 0x00, 0x00, 0x10,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xea,
};

static uint8_t hdmi_edid[256] = {
	0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00,
	0x31, 0xd8, 0x34, 0x12, 0x00, 0x00, 0x00, 0x00,
	0x22, 0x1a, 0x01, 0x03, 0x80, 0x30, 0x1b, 0x78,
	0x0f, 0xee, 0x91, 0xa3, 0x54, 0x4c, 0x99, 0x26,
	0x0f, 0x50, 0x54, 0x2f, 0xcf, 0x00, 0x31, 0x59,
	0x45, 0x59, 0x81, 0x80, 0x81, 0x40, 0x90, 0x40,
	0x95, 0x00, 0xa9, 0x40, 0xb3, 0x00, 0x02, 0x3a,
	0x80, 0x18, 0x71, 0x38, 0x2d, 0x40, 0x58, 0x2c,
	0x45, 0x00, 0xe0, 0x0e, 0x11, 0x00, 0x00, 0x1e,
	0x00, 0x00, 0x00, 0xfd, 0x00, 0x18, 0x55, 0x18,
	0x5e, 0x11, 0x00, 0x0a, 0x20, 0x20, 0x20, 0x20,
	0x20, 0x20, 0x00, 0x00, 0x00, 0xfc, 0x00, 0x68,
	0x64, 0x6d, 0x69, 0x0a, 0x20, 0x20, 0x20, 0x20,
	0x20, 0x20, 0x20, 0x20, 0x00, 0x00, 0x00, 0x10,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0xc4,

	0x02, 0x03, 0x2d, 0xf0, 0x4c, 0x10, 0x1f, 0x04,
	0x13, 0x22, 0x21, 0x20, 0x05, 0x14, 0x02, 0x11,
	0x01, 0x23, 0x09, 0x07, 0x07, 0x83, 0x01, 0x00,
	0x00, 0x68, 0x03, 0x0c, 0x00, 0x10, 0x00, 0x00,
	0x22, 0x01, 0xe2, 0x00, 0xea, 0xe3, 0x05, 0x00,
	0x00, 0xe3, 0x06, 0x01, 0x00, 0x1a, 0x36, 0x80,
	0xa0, 0x70, 0x38, 0x1f, 0x40, 0x30, 0x20, 0x35,
	0x00, 0xe0, 0x0e, 0x11, 0x00, 0x00, 0x1a, 0x1a,
	0x1d, 0x00, 0x80, 0x51, 0xd0, 0x1c, 0x20, 0x40,
	0x80, 0x35, 0x00, 0xe0, 0x0e, 0x11, 0x00, 0x00,
	0x1c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7d,
};

static uint8_t hdmi_edid_4k_170[256] = {
	0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00,
	0x31, 0xd8, 0x34, 0x12, 0x00, 0x00, 0x00, 0x00,
	0x22, 0x1a, 0x01, 0x03, 0x80, 0x60, 0x36, 0x78,
	0x0f, 0xee, 0x91, 0xa3, 0x54, 0x4c, 0x99, 0x26,
	0x0f, 0x50, 0x54, 0x2f, 0xcf, 0x00, 0x31, 0x59,
	0x45, 0x59, 0x81, 0x80, 0x81, 0x40, 0x90, 0x40,
	0x95, 0x00, 0xa9, 0x40, 0xb3, 0x00, 0x02, 0x3a,
	0x80, 0x18, 0x71, 0x38, 0x2d, 0x40, 0x58, 0x2c,
	0x46, 0x00, 0xc0, 0x1c, 0x32, 0x00, 0x00, 0x1e,
	0x00, 0x00, 0x00, 0xfd, 0x00, 0x18, 0x55, 0x18,
	0x5e, 0x11, 0x00, 0x0a, 0x20, 0x20, 0x20, 0x20,
	0x20, 0x20, 0x00, 0x00, 0x00, 0xfc, 0x00, 0x68,
	0x64, 0x6d, 0x69, 0x2d, 0x34, 0x6b, 0x2d, 0x31,
	0x37, 0x30, 0x0a, 0x20, 0x00, 0x00, 0x00, 0x10,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0xb8,

	0x02, 0x03, 0x32, 0xf0, 0x4c, 0x10, 0x1f, 0x04,
	0x13, 0x22, 0x21, 0x20, 0x05, 0x14, 0x02, 0x11,
	0x01, 0x23, 0x09, 0x07, 0x07, 0x83, 0x01, 0x00,
	0x00, 0x68, 0x03, 0x0c, 0x00, 0x10, 0x00, 0x00,
	0x22, 0x01, 0xe2, 0x00, 0xea, 0xe4, 0x0e, 0x5f,
	0x5e, 0x5d, 0xe3, 0x05, 0x00, 0x00, 0xe3, 0x06,
	0x01, 0x00, 0x1a, 0x36, 0x80, 0xa0, 0x70, 0x38,
	0x1f, 0x40, 0x30, 0x20, 0x35, 0x00, 0xc0, 0x1c,
	0x32, 0x00, 0x00, 0x1a, 0x1a, 0x1d, 0x00, 0x80,
	0x51, 0xd0, 0x1c, 0x20, 0x40, 0x80, 0x35, 0x00,
	0xc0, 0x1c, 0x32, 0x00, 0x00, 0x1c, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x4e,
};

static uint8_t hdmi_edid_4k_300[256] = {
	0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00,
	0x31, 0xd8, 0x34, 0x12, 0x00, 0x00, 0x00, 0x00,
	0x22, 0x1a, 0x01, 0x03, 0x80, 0x60, 0x36, 0x78,
	0x0f, 0xee, 0x91, 0xa3, 0x54, 0x4c, 0x99, 0x26,
	0x0f, 0x50, 0x54, 0x2f, 0xcf, 0x00, 0x31, 0x59,
	0x45, 0x59, 0x81, 0x80, 0x81, 0x40, 0x90, 0x40,
	0x95, 0x00, 0xa9, 0x40, 0xb3, 0x00, 0x04, 0x74,
	0x00, 0x30, 0xf2, 0x70, 0x5a, 0x80, 0xb0, 0x58,
	0x8a, 0x00, 0xc0, 0x1c, 0x32, 0x00, 0x00, 0x1e,
	0x00, 0x00, 0x00, 0xfd, 0x00, 0x18, 0x55, 0x18,
	0x87, 0x1e, 0x00, 0x0a, 0x20, 0x20, 0x20, 0x20,
	0x20, 0x20, 0x00, 0x00, 0x00, 0xfc, 0x00, 0x68,
	0x64, 0x6d, 0x69, 0x2d, 0x34, 0x6b, 0x2d, 0x33,
	0x30, 0x30, 0x0a, 0x20, 0x00, 0x00, 0x00, 0x10,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0xc5,

	0x02, 0x03, 0x3d, 0xf0, 0x4f, 0x5f, 0x5e, 0x5d,
	0x10, 0x1f, 0x04, 0x13, 0x22, 0x21, 0x20, 0x05,
	0x14, 0x02, 0x11, 0x01, 0x23, 0x09, 0x07, 0x07,
	0x83, 0x01, 0x00, 0x00, 0x6d, 0x03, 0x0c, 0x00,
	0x10, 0x00, 0x00, 0x3c, 0x21, 0x00, 0x60, 0x01,
	0x02, 0x03, 0x67, 0xd8, 0x5d, 0xc4, 0x01, 0x00,
	0x00, 0x00, 0xe2, 0x00, 0xea, 0xe3, 0x05, 0x00,
	0x00, 0xe3, 0x06, 0x01, 0x00, 0xa3, 0x66, 0x00,
	0xa0, 0xf0, 0x70, 0x1f, 0x80, 0x30, 0x20, 0x35,
	0x00, 0xc0, 0x1c, 0x32, 0x00, 0x00, 0x1e, 0x1a,
	0x36, 0x80, 0xa0, 0x70, 0x38, 0x1f, 0x40, 0x30,
	0x20, 0x35, 0x00, 0xc0, 0x1c, 0x32, 0x00, 0x00,
	0x1a, 0x1a, 0x1d, 0x00, 0x80, 0x51, 0xd0, 0x1c,
	0x20, 0x40, 0x80, 0x35, 0x00, 0xc0, 0x1c, 0x32,
	0x00, 0x00, 0x1c, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xd3,
};

static uint8_t hdmi_edid_4k_600[256] = {
	0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00,
	0x31, 0xd8, 0x34, 0x12, 0x00, 0x00, 0x00, 0x00,
	0x22, 0x1a, 0x01, 0x03, 0x80, 0x60, 0x36, 0x78,
	0x0f, 0xee, 0x91, 0xa3, 0x54, 0x4c, 0x99, 0x26,
	0x0f, 0x50, 0x54, 0x2f, 0xcf, 0x00, 0x31, 0x59,
	0x45, 0x59, 0x81, 0x80, 0x81, 0x40, 0x90, 0x40,
	0x95, 0x00, 0xa9, 0x40, 0xb3, 0x00, 0x08, 0xe8,
	0x00, 0x30, 0xf2, 0x70, 0x5a, 0x80, 0xb0, 0x58,
	0x8a, 0x00, 0xc0, 0x1c, 0x32, 0x00, 0x00, 0x1e,
	0x00, 0x00, 0x00, 0xfd, 0x00, 0x18, 0x55, 0x18,
	0x87, 0x3c, 0x00, 0x0a, 0x20, 0x20, 0x20, 0x20,
	0x20, 0x20, 0x00, 0x00, 0x00, 0xfc, 0x00, 0x68,
	0x64, 0x6d, 0x69, 0x2d, 0x34, 0x6b, 0x2d, 0x36,
	0x30, 0x30, 0x0a, 0x20, 0x00, 0x00, 0x00, 0x10,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x2c,

	0x02, 0x03, 0x3f, 0xf0, 0x51, 0x61, 0x60, 0x5f,
	0x5e, 0x5d, 0x10, 0x1f, 0x04, 0x13, 0x22, 0x21,
	0x20, 0x05, 0x14, 0x02, 0x11, 0x01, 0x23, 0x09,
	0x07, 0x07, 0x83, 0x01, 0x00, 0x00, 0x6d, 0x03,
	0x0c, 0x00, 0x10, 0x00, 0x00, 0x3c, 0x21, 0x00,
	0x60, 0x01, 0x02, 0x03, 0x67, 0xd8, 0x5d, 0xc4,
	0x01, 0x78, 0x00, 0x00, 0xe2, 0x00, 0xea, 0xe3,
	0x05, 0x00, 0x00, 0xe3, 0x06, 0x01, 0x00, 0x4d,
	0xd0, 0x00, 0xa0, 0xf0, 0x70, 0x3e, 0x80, 0x30,
	0x20, 0x35, 0x00, 0xc0, 0x1c, 0x32, 0x00, 0x00,
	0x1e, 0x1a, 0x36, 0x80, 0xa0, 0x70, 0x38, 0x1f,
	0x40, 0x30, 0x20, 0x35, 0x00, 0xc0, 0x1c, 0x32,
	0x00, 0x00, 0x1a, 0x1a, 0x1d, 0x00, 0x80, 0x51,
	0xd0, 0x1c, 0x20, 0x40, 0x80, 0x35, 0x00, 0xc0,
	0x1c, 0x32, 0x00, 0x00, 0x1c, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x63,
};

/******************************************************/

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
				"type",
				"edid",
				"file",
				"format",
				"pa",
				"s-pt",
				"s-it",
				"s-ce",
				"y444",
				"30-bit",
				"36-bit",
				"48-bit",
				"graphics",
				"photo",
				"cinema",
				"game",
				"scdc",
				"underscan",
				"audio",
				"ycbcr444",
				"ycbcr422",
				"qy",
				"qs",
				"xvycc-601",
				"xvycc-709",
				"sycc",
				"adobe-ycc",
				"adobe-rgb",
				"bt2020-rgb",
				"bt2020-ycc",
				"bt2020-cycc",
				"sdr",
				"hdr",
				"smpte2084",
				"fl-fr",
				"lfe",
				"fc",
				"rl-rr",
				"rc",
				"flc-frc",
				"rlc-rrc",
				"flw-frw",
				"flh-frh",
				"tc",
				"fch",
				NULL
			};

			int opt = getsubopt(&subs, (char* const*)subopts, &value);

			if (opt == -1) {
				fprintf(stderr, "Invalid suboptions specified\n");
				edid_usage();
				exit(1);
			}
			if (value == NULL && opt <= 8) {
				fprintf(stderr, "No value given to suboption <%s>\n",
					subopts[opt]);
				edid_usage();
				exit(1);
			}
			switch (opt) {
			case 0:
				sedid.pad = strtoul(value, 0, 0);
				break;
			case 1:
			case 2:	/* keep edid for compat reasons, it's the same as type */
				if (!strcmp(value, "dvid")) {
					sedid.edid = dvid_edid;
					sedid.blocks = sizeof(dvid_edid) / 128;
				} else if (!strcmp(value, "vga")) {
					sedid.edid = vga_edid;
					sedid.blocks = sizeof(vga_edid) / 128;
				} else if (!strcmp(value, "hdmi-4k-170mhz")) {
					sedid.edid = hdmi_edid_4k_170;
					sedid.blocks = sizeof(hdmi_edid) / 128;
				} else if (!strcmp(value, "hdmi-4k-300mhz")) {
					sedid.edid = hdmi_edid_4k_300;
					sedid.blocks = sizeof(hdmi_edid) / 128;
				} else if (!strcmp(value, "hdmi-4k-600mhz")) {
					sedid.edid = hdmi_edid_4k_600;
					sedid.blocks = sizeof(hdmi_edid) / 128;
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
			case 3:
				if (value) {
					file_in = value;
					if (sedid.edid) {
						fprintf(stderr, "The edid and file options can't be used together.\n");
						exit(1);
					}
				}
				break;
			case 4:
				if (!strcmp(value, "hex")) {
					sformat = HEX;
				} else if (!strcmp(value, "raw")) {
					sformat = RAW;
				} else {
					edid_usage();
					exit(1);
				}
				break;
			case 5:
				if (value)
					phys_addr = parse_phys_addr(value);
				break;
			case 6:
				mod_s_pt = strtoul(value, 0, 0) & 3;
				break;
			case 7:
				mod_s_it = strtoul(value, 0, 0) & 3;
				break;
			case 8:
				mod_s_ce = strtoul(value, 0, 0) & 3;
				break;
			case 9: toggle_hdmi_vsdb_dc_flags |= HDMI_VSDB_Y444_BIT; break;
			case 10: toggle_hdmi_vsdb_dc_flags |= HDMI_VSDB_30_BIT; break;
			case 11: toggle_hdmi_vsdb_dc_flags |= HDMI_VSDB_36_BIT; break;
			case 12: toggle_hdmi_vsdb_dc_flags |= HDMI_VSDB_48_BIT; break;
			case 13: toggle_hdmi_vsdb_cnc_flags |= HDMI_VSDB_GRAPHICS; break;
			case 14: toggle_hdmi_vsdb_cnc_flags |= HDMI_VSDB_PHOTO; break;
			case 15: toggle_hdmi_vsdb_cnc_flags |= HDMI_VSDB_CINEMA; break;
			case 16: toggle_hdmi_vsdb_cnc_flags |= HDMI_VSDB_GAME; break;
			case 17: toggle_hf_vsdb_flags |= HF_VSDB_SCSD_PRESENT; break;
			case 18: toggle_cea861_hdr_flags |= CEA861_HDR_UNDERSCAN; break;
			case 19: toggle_cea861_hdr_flags |= CEA861_HDR_AUDIO; break;
			case 20: toggle_cea861_hdr_flags |= CEA861_HDR_YCBCR444; break;
			case 21: toggle_cea861_hdr_flags |= CEA861_HDR_YCBCR422; break;
			case 22: toggle_vid_cap_flags |= VID_CAP_QY; break;
			case 23: toggle_vid_cap_flags |= VID_CAP_QS; break;
			case 24: toggle_colorimetry_flags |= COLORIMETRY_XVYCC601; break;
			case 25: toggle_colorimetry_flags |= COLORIMETRY_XVYCC709; break;
			case 26: toggle_colorimetry_flags |= COLORIMETRY_SYCC; break;
			case 27: toggle_colorimetry_flags |= COLORIMETRY_ADOBEYCC; break;
			case 28: toggle_colorimetry_flags |= COLORIMETRY_ADOBERGB; break;
			case 29: toggle_colorimetry_flags |= COLORIMETRY_BT2020RGB; break;
			case 30: toggle_colorimetry_flags |= COLORIMETRY_BT2020YCC; break;
			case 31: toggle_colorimetry_flags |= COLORIMETRY_BT2020CYCC; break;
			case 32: toggle_hdr_md_flags |= HDR_MD_SDR; break;
			case 33: toggle_hdr_md_flags |= HDR_MD_HDR; break;
			case 34: toggle_hdr_md_flags |= HDR_MD_SMPTE_2084; break;
			case 35: toggle_speaker1_flags |= SPEAKER1_FLFR; break;
			case 36: toggle_speaker1_flags |= SPEAKER1_LFE; break;
			case 37: toggle_speaker1_flags |= SPEAKER1_FC; break;
			case 38: toggle_speaker1_flags |= SPEAKER1_RLRR; break;
			case 39: toggle_speaker1_flags |= SPEAKER1_RC; break;
			case 40: toggle_speaker1_flags |= SPEAKER1_FLCFRC; break;
			case 41: toggle_speaker1_flags |= SPEAKER1_RLCRRC; break;
			case 42: toggle_speaker1_flags |= SPEAKER1_FLWFRW; break;
			case 43: toggle_speaker2_flags |= SPEAKER2_FLHFRH; break;
			case 44: toggle_speaker2_flags |= SPEAKER2_TC; break;
			case 45: toggle_speaker2_flags |= SPEAKER2_FCH; break;
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
		break;

	case OptInfoEdid:
		memset(&info_edid, 0, sizeof(info_edid));
		if (optarg)
			info_edid.pad = strtoul(optarg, 0, 0);
		break;
	}
}

void edid_set(int fd)
{
	int loc;

	if (options[OptClearEdid]) {
		struct v4l2_edid edid;

		memset(&edid, 0, sizeof(edid));
		edid.pad = clear_pad;
		doioctl(fd, VIDIOC_S_EDID, &edid);
	}

	if (options[OptSetEdid]) {
		FILE *fin = NULL;
		bool must_fix_edid = options[OptFixEdidChecksums];

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
		if (toggle_cea861_hdr_flags || phys_addr >= 0) {
			loc = get_edid_cea861_hdr_location(sedid.edid, sedid.blocks * 128);
			if (loc >= 0) {
				sedid.edid[loc] ^= toggle_cea861_hdr_flags;
				if (phys_addr >= 0)
					set_edid_phys_addr(sedid.edid, sedid.blocks * 128, phys_addr);
				must_fix_edid = true;
			}
		}
		if (toggle_speaker1_flags || toggle_speaker2_flags) {
			loc = get_edid_speaker_location(sedid.edid, sedid.blocks * 128);
			if (loc >= 0) {
				sedid.edid[loc] ^= toggle_speaker1_flags;
				sedid.edid[loc + 1] ^= toggle_speaker2_flags;
				must_fix_edid = true;
			}
		}
		if (toggle_hdmi_vsdb_dc_flags || toggle_hdmi_vsdb_cnc_flags) {
			loc = get_edid_hdmi_vsdb_location(sedid.edid, sedid.blocks * 128);

			if (loc >= 0) {
				__u8 len = sedid.edid[loc] & 0x1f;

				if (len >= 6) {
					sedid.edid[loc + 6] ^= toggle_hdmi_vsdb_dc_flags;
					must_fix_edid = true;
				}
				if (len >= 8) {
					sedid.edid[loc + 8] ^= toggle_hdmi_vsdb_cnc_flags;
					must_fix_edid = true;
				}
			}
		}
		if (toggle_hf_vsdb_flags) {
			loc = get_edid_hf_vsdb_location(sedid.edid, sedid.blocks * 128);
			if (loc >= 0) {
				sedid.edid[loc + 1] ^= toggle_hf_vsdb_flags;
				must_fix_edid = true;
			}
		}
		if (toggle_vid_cap_flags || mod_s_pt >= 0 ||
		    mod_s_ce >= 0 || mod_s_it >= 0) {
			loc = get_edid_vid_cap_location(sedid.edid, sedid.blocks * 128);
			if (loc >= 0) {
				sedid.edid[loc] ^= toggle_vid_cap_flags;
				if (mod_s_ce >= 0) {
					sedid.edid[loc] &= 0xfc;
					sedid.edid[loc] |= mod_s_ce << 0;
				}
				if (mod_s_it >= 0) {
					sedid.edid[loc] &= 0xf3;
					sedid.edid[loc] |= mod_s_it << 2;
				}
				if (mod_s_pt >= 0) {
					sedid.edid[loc] &= 0xcf;
					sedid.edid[loc] |= mod_s_pt << 4;
				}
				must_fix_edid = true;
			}
		}
		if (toggle_colorimetry_flags) {
			loc = get_edid_colorimetry_location(sedid.edid, sedid.blocks * 128);
			if (loc >= 0) {
				sedid.edid[loc] ^= toggle_colorimetry_flags;
				must_fix_edid = true;
			}
		}
		if (toggle_hdr_md_flags) {
			loc = get_edid_hdr_md_location(sedid.edid, sedid.blocks * 128);
			if (loc >= 0) {
				sedid.edid[loc] ^= toggle_hdr_md_flags;
				must_fix_edid = true;
			}
		}
		if (must_fix_edid)
			fix_edid(&sedid);
		print_edid_mods(&sedid);
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
	if (options[OptInfoEdid]) {
		info_edid.blocks = 2;
		info_edid.edid = (unsigned char *)malloc(info_edid.blocks * 128);
		if (doioctl(fd, VIDIOC_G_EDID, &info_edid) == 0)
			print_edid_mods(&info_edid);
		free(info_edid.edid);
	}
}
