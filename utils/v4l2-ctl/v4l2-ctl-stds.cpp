#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <dirent.h>
#include <math.h>
#include <config.h>

#include <linux/videodev2.h>
#include <libv4l2.h>
#include <string>

#include "v4l2-ctl.h"

static v4l2_std_id standard;		/* get_std/set_std */
static struct v4l2_dv_timings dv_timings; /* set_dv_bt_timings/get_dv_timings/query_dv_timings */
static bool query_and_set_dv_timings = false;
static int enum_and_set_dv_timings = -1;
	
void stds_usage(void)
{
	printf("\nStandards/Timings options:\n"
	       "  --list-standards   display supported video standards [VIDIOC_ENUMSTD]\n"
	       "  -S, --get-standard\n"
	       "                     query the video standard [VIDIOC_G_STD]\n"
	       "  -s, --set-standard=<num>\n"
	       "                     set the video standard to <num> [VIDIOC_S_STD]\n"
	       "                     <num> a numerical v4l2_std value, or one of:\n"
	       "                     pal or pal-X (X = B/G/H/N/Nc/I/D/K/M/60) (V4L2_STD_PAL)\n"
	       "                     ntsc or ntsc-X (X = M/J/K) (V4L2_STD_NTSC)\n"
	       "                     secam or secam-X (X = B/G/H/D/K/L/Lc) (V4L2_STD_SECAM)\n"
	       "  --get-detected-standard\n"
	       "                     display detected input video standard [VIDIOC_QUERYSTD]\n"
	       "  --list-dv-timings  list supp. standard dv timings [VIDIOC_ENUM_DV_TIMINGS]\n"
	       "  --set-dv-bt-timings\n"
	       "                     query: use the output of VIDIOC_QUERY_DV_TIMINGS\n"
	       "                     index=<index>: use the index as provided by --list-dv-timings\n"
	       "                     or give a fully specified timings:\n"
	       "                     width=<width>,height=<height>,interlaced=<0/1>,\n"
	       "                     polarities=<polarities mask>,pixelclock=<pixelclock Hz>,\n"
	       "                     hfp=<horizontal front porch>,hs=<horizontal sync>,\n"
	       "                     hbp=<horizontal back porch>,vfp=<vertical front porch>,\n"
	       "                     vs=<vertical sync>,vbp=<vertical back porch>,\n"
	       "                     il_vfp=<vertical front porch for bottom field>,\n"
	       "                     il_vs=<vertical sync for bottom field>,\n"
	       "                     il_vbp=<vertical back porch for bottom field>,\n"
	       "                     set the digital video timings according to the BT 656/1120\n"
	       "                     standard [VIDIOC_S_DV_TIMINGS]\n"
	       "  --get-dv-timings   get the digital video timings in use [VIDIOC_G_DV_TIMINGS]\n"
	       "  --query-dv-timings query the detected dv timings [VIDIOC_QUERY_DV_TIMINGS]\n"
	       "  --get-dv-timings-cap\n"
	       "                     get the dv timings capabilities [VIDIOC_DV_TIMINGS_CAP]\n"
	       );
}

static v4l2_std_id parse_pal(const char *pal)
{
	if (pal[0] == '-') {
		switch (tolower(pal[1])) {
			case '6':
				return V4L2_STD_PAL_60;
			case 'b':
			case 'g':
				return V4L2_STD_PAL_BG;
			case 'h':
				return V4L2_STD_PAL_H;
			case 'n':
				if (tolower(pal[2]) == 'c')
					return V4L2_STD_PAL_Nc;
				return V4L2_STD_PAL_N;
			case 'i':
				return V4L2_STD_PAL_I;
			case 'd':
			case 'k':
				return V4L2_STD_PAL_DK;
			case 'm':
				return V4L2_STD_PAL_M;
		}
	}
	fprintf(stderr, "pal specifier not recognised\n");
	stds_usage();
	exit(1);
}

static v4l2_std_id parse_secam(const char *secam)
{
	if (secam[0] == '-') {
		switch (tolower(secam[1])) {
			case 'b':
			case 'g':
			case 'h':
				return V4L2_STD_SECAM_B | V4L2_STD_SECAM_G | V4L2_STD_SECAM_H;
			case 'd':
			case 'k':
				return V4L2_STD_SECAM_DK;
			case 'l':
				if (tolower(secam[2]) == 'c')
					return V4L2_STD_SECAM_LC;
				return V4L2_STD_SECAM_L;
		}
	}
	fprintf(stderr, "secam specifier not recognised\n");
	stds_usage();
	exit(1);
}

static v4l2_std_id parse_ntsc(const char *ntsc)
{
	if (ntsc[0] == '-') {
		switch (tolower(ntsc[1])) {
			case 'm':
				return V4L2_STD_NTSC_M;
			case 'j':
				return V4L2_STD_NTSC_M_JP;
			case 'k':
				return V4L2_STD_NTSC_M_KR;
		}
	}
	fprintf(stderr, "ntsc specifier not recognised\n");
	stds_usage();
	exit(1);
}

static void parse_dv_bt_timings(char *optarg, struct v4l2_dv_timings *dv_timings,
		bool &query, int &enumerate)
{
	char *value;
	char *subs = optarg;
	struct v4l2_bt_timings *bt = &dv_timings->bt;

	dv_timings->type = V4L2_DV_BT_656_1120;

	while (*subs != '\0') {
		static const char *const subopts[] = {
			"width",
			"height",
			"interlaced",
			"polarities",
			"pixelclock",
			"hfp",
			"hs",
			"hbp",
			"vfp",
			"vs",
			"vbp",
			"il_vfp",
			"il_vs",
			"il_vbp",
			"index",
			"query",
			NULL
		};

		switch (parse_subopt(&subs, subopts, &value)) {
		case 0:
			bt->width = atoi(value);
			break;
		case 1:
			bt->height = strtol(value, 0L, 0);
			break;
		case 2:
			bt->interlaced = strtol(value, 0L, 0);
			break;
		case 3:
			bt->polarities = strtol(value, 0L, 0);
			break;
		case 4:
			bt->pixelclock = strtol(value, 0L, 0);
			break;
		case 5:
			bt->hfrontporch = strtol(value, 0L, 0);
			break;
		case 6:
			bt->hsync = strtol(value, 0L, 0);
			break;
		case 7:
			bt->hbackporch = strtol(value, 0L, 0);
			break;
		case 8:
			bt->vfrontporch = strtol(value, 0L, 0);
			break;
		case 9:
			bt->vsync = strtol(value, 0L, 0);
			break;
		case 10:
			bt->vbackporch = strtol(value, 0L, 0);
			break;
		case 11:
			bt->il_vfrontporch = strtol(value, 0L, 0);
			break;
		case 12:
			bt->il_vsync = strtol(value, 0L, 0);
			break;
		case 13:
			bt->il_vbackporch = strtol(value, 0L, 0);
			break;
		case 14:
			enumerate = strtol(value, 0L, 0);
			break;
		case 15:
			query = true;
			break;
		default:
			stds_usage();
			exit(1);
		}
	}
}

static const flag_def dv_standards_def[] = {
	{ V4L2_DV_BT_STD_CEA861, "CEA-861" },
	{ V4L2_DV_BT_STD_DMT, "DMT" },
	{ V4L2_DV_BT_STD_CVT, "CVT" },
	{ V4L2_DV_BT_STD_GTF, "GTF" },
	{ 0, NULL }
};

static const flag_def dv_flags_def[] = {
	{ V4L2_DV_FL_REDUCED_BLANKING, "reduced blanking" },
	{ V4L2_DV_FL_CAN_REDUCE_FPS, "framerate can be reduced by 1/1.001" },
	{ V4L2_DV_FL_REDUCED_FPS, "framerate is reduced by 1/1.001" },
	{ V4L2_DV_FL_HALF_LINE, "half-line" },
	{ 0, NULL }
};

static void print_dv_timings(const struct v4l2_dv_timings *t)
{
	const struct v4l2_bt_timings *bt;
	double tot_width, tot_height;

	switch (t->type) {
	case V4L2_DV_BT_656_1120:
		bt = &t->bt;

		tot_height = bt->height +
			bt->vfrontporch + bt->vsync + bt->vbackporch +
			bt->il_vfrontporch + bt->il_vsync + bt->il_vbackporch;
		tot_width = bt->width +
			bt->hfrontporch + bt->hsync + bt->hbackporch;
		if (options[OptConcise]) {
			printf("\t%dx%d%c%.2f %s\n", bt->width, bt->height,
				bt->interlaced ? 'i' : 'p',
				(double)bt->pixelclock /
					(tot_width * (tot_height / (bt->interlaced ? 2 : 1))),
				flags2s(bt->flags, dv_flags_def).c_str());
			break;
		}
		printf("\tActive width: %d\n", bt->width);
		printf("\tActive height: %d\n", bt->height);
		printf("\tTotal width: %d\n",bt->width +
				bt->hfrontporch + bt->hsync + bt->hbackporch);
		printf("\tTotal height: %d\n", bt->height +
				bt->vfrontporch + bt->vsync + bt->vbackporch +
				bt->il_vfrontporch + bt->il_vsync + bt->il_vbackporch);

		printf("\tFrame format: %s\n", bt->interlaced ? "interlaced" : "progressive");
		printf("\tPolarities: %cvsync %chsync\n",
				(bt->polarities & V4L2_DV_VSYNC_POS_POL) ? '+' : '-',
				(bt->polarities & V4L2_DV_HSYNC_POS_POL) ? '+' : '-');
		printf("\tPixelclock: %lld Hz", bt->pixelclock);
		if (bt->width && bt->height) {
			if (bt->interlaced)
				printf(" (%.2f fields per second)", (double)bt->pixelclock /
					(tot_width * (tot_height / 2)));
			else
				printf(" (%.2f frames per second)", (double)bt->pixelclock /
					(tot_width * tot_height));
		}
		printf("\n");
		printf("\tHorizontal frontporch: %d\n", bt->hfrontporch);
		printf("\tHorizontal sync: %d\n", bt->hsync);
		printf("\tHorizontal backporch: %d\n", bt->hbackporch);
		if (bt->interlaced)
			printf("\tField 1:\n");
		printf("\tVertical frontporch: %d\n", bt->vfrontporch);
		printf("\tVertical sync: %d\n", bt->vsync);
		printf("\tVertical backporch: %d\n", bt->vbackporch);
		if (bt->interlaced) {
			printf("\tField 2:\n");
			printf("\tVertical frontporch: %d\n", bt->il_vfrontporch);
			printf("\tVertical sync: %d\n", bt->il_vsync);
			printf("\tVertical backporch: %d\n", bt->il_vbackporch);
		}
		printf("\tStandards: %s\n", flags2s(bt->standards, dv_standards_def).c_str());
		printf("\tFlags: %s\n", flags2s(bt->flags, dv_flags_def).c_str());
		break;
	default:
		printf("Timing type not defined\n");
		break;
	}
}

void stds_cmd(int ch, char *optarg)
{
	switch (ch) {
	case OptSetStandard:
		if (!strncasecmp(optarg, "pal", 3)) {
			if (optarg[3])
				standard = parse_pal(optarg + 3);
			else
				standard = V4L2_STD_PAL;
		}
		else if (!strncasecmp(optarg, "ntsc", 4)) {
			if (optarg[4])
				standard = parse_ntsc(optarg + 4);
			else
				standard = V4L2_STD_NTSC;
		}
		else if (!strncasecmp(optarg, "secam", 5)) {
			if (optarg[5])
				standard = parse_secam(optarg + 5);
			else
				standard = V4L2_STD_SECAM;
		}
		else if (isdigit(optarg[0])) {
			standard = strtol(optarg, 0L, 0) | (1ULL << 63);
		} else {
			fprintf(stderr, "Unknown standard '%s'\n", optarg);
			stds_usage();
			exit(1);
		}
		break;
	case OptSetDvBtTimings:
		parse_dv_bt_timings(optarg, &dv_timings,
				query_and_set_dv_timings, enum_and_set_dv_timings);
		break;
	}
}

void stds_set(int fd)
{
	if (options[OptSetStandard]) {
		if (standard & (1ULL << 63)) {
			struct v4l2_standard vs;

			vs.index = standard & 0xffff;
			if (test_ioctl(fd, VIDIOC_ENUMSTD, &vs) >= 0) {
				standard = vs.id;
			}
		}
		if (doioctl(fd, VIDIOC_S_STD, &standard) == 0)
			printf("Standard set to %08llx\n", (unsigned long long)standard);
	}

	if (options[OptSetDvBtTimings]) {
		struct v4l2_enum_dv_timings et;

		if (query_and_set_dv_timings)
			doioctl(fd, VIDIOC_QUERY_DV_TIMINGS, &dv_timings);
		if (enum_and_set_dv_timings >= 0) {
			memset(&et, 0, sizeof(et));
			et.index = enum_and_set_dv_timings;
			doioctl(fd, VIDIOC_ENUM_DV_TIMINGS, &et);
			dv_timings = et.timings;
		}
		if (doioctl(fd, VIDIOC_S_DV_TIMINGS, &dv_timings) >= 0) {
			printf("BT timings set\n");
		}
	}
}

void stds_get(int fd)
{
	if (options[OptGetStandard]) {
		if (doioctl(fd, VIDIOC_G_STD, &standard) == 0) {
			printf("Video Standard = 0x%08llx\n", (unsigned long long)standard);
			print_v4lstd((unsigned long long)standard);
		}
	}

	if (options[OptGetDvTimings]) {
		if (doioctl(fd, VIDIOC_G_DV_TIMINGS, &dv_timings) >= 0) {
			printf("DV timings:\n");
			print_dv_timings(&dv_timings);
		}
	}

	if (options[OptGetDvTimingsCap]) {
		struct v4l2_dv_timings_cap dv_timings_cap;

		if (doioctl(fd, VIDIOC_DV_TIMINGS_CAP, &dv_timings_cap) >= 0) {
			static const flag_def dv_caps_def[] = {
				{ V4L2_DV_BT_CAP_INTERLACED, "Interlaced" },
				{ V4L2_DV_BT_CAP_PROGRESSIVE, "Progressive" },
				{ V4L2_DV_BT_CAP_REDUCED_BLANKING, "Reduced Blanking" },
				{ V4L2_DV_BT_CAP_CUSTOM, "Custom Formats" },
				{ 0, NULL }
			};
			struct v4l2_bt_timings_cap *bt = &dv_timings_cap.bt;

			printf("DV timings capabilities:\n");
			if (dv_timings_cap.type != V4L2_DV_BT_656_1120)
				printf("\tUnknown type\n");
			else {
				printf("\tMinimum Width: %u\n", bt->min_width);
				printf("\tMaximum Width: %u\n", bt->max_width);
				printf("\tMinimum Height: %u\n", bt->min_height);
				printf("\tMaximum Height: %u\n", bt->max_height);
				printf("\tMinimum PClock: %llu\n", bt->min_pixelclock);
				printf("\tMaximum PClock: %llu\n", bt->max_pixelclock);
				printf("\tStandards: %s\n",
					flags2s(bt->standards, dv_standards_def).c_str());
				printf("\tCapabilities: %s\n",
					flags2s(bt->capabilities, dv_caps_def).c_str());
			}
		}
	}

        if (options[OptQueryStandard]) {
		if (doioctl(fd, VIDIOC_QUERYSTD, &standard) == 0) {
			printf("Video Standard = 0x%08llx\n", (unsigned long long)standard);
			print_v4lstd((unsigned long long)standard);
		}
	}

        if (options[OptQueryDvTimings]) {
                doioctl(fd, VIDIOC_QUERY_DV_TIMINGS, &dv_timings);
		print_dv_timings(&dv_timings);
        }
}

void stds_list(int fd)
{
	if (options[OptListStandards]) {
		struct v4l2_standard vs;

		printf("ioctl: VIDIOC_ENUMSTD\n");
		vs.index = 0;
		while (test_ioctl(fd, VIDIOC_ENUMSTD, &vs) >= 0) {
			if (options[OptConcise]) {
				printf("\t%2d: 0x%016llX %s\n", vs.index,
						(unsigned long long)vs.id, vs.name);
			} else {
				if (vs.index)
					printf("\n");
				printf("\tIndex       : %d\n", vs.index);
				printf("\tID          : 0x%016llX\n", (unsigned long long)vs.id);
				printf("\tName        : %s\n", vs.name);
				printf("\tFrame period: %d/%d\n",
						vs.frameperiod.numerator,
						vs.frameperiod.denominator);
				printf("\tFrame lines : %d\n", vs.framelines);
			}
			vs.index++;
		}
	}

	if (options[OptListDvTimings]) {
		struct v4l2_enum_dv_timings dv_enum_timings;

		dv_enum_timings.index = 0;
		printf("ioctl: VIDIOC_ENUM_DV_TIMINGS\n");
		while (test_ioctl(fd, VIDIOC_ENUM_DV_TIMINGS, &dv_enum_timings) >= 0) {
			if (options[OptConcise]) {
				printf("\t%d:", dv_enum_timings.index);
			} else {
				if (dv_enum_timings.index)
					printf("\n");
				printf("\tIndex: %d\n", dv_enum_timings.index);
			}
			print_dv_timings(&dv_enum_timings.timings);
			dv_enum_timings.index++;
		}
	}
}
