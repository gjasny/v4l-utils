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

#include "v4l2-ctl.h"

static v4l2_std_id standard;		/* get_std/set_std */
static struct v4l2_dv_timings dv_timings; /* set_dv_bt_timings/get_dv_timings/query_dv_timings */
static bool query_and_set_dv_timings = false;
static bool cleared_dv_timings = false;
static int enum_and_set_dv_timings = -1;
static unsigned set_dv_timing_opts;

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
	       "                     or specify timings using cvt/gtf options as follows:\n"
	       "                     cvt/gtf,width=<width>,height=<height>,fps=<frames per sec>\n"
	       "                     interlaced=<0/1>,reduced-blanking=<0/1/2>,reduced-fps=<0/1>\n"
	       "                     The value of reduced-blanking, if greater than 0, indicates\n"
	       "                     that reduced blanking is to be used and the value indicate the\n"
	       "                     version. For gtf, there is no version 2 for reduced blanking, and\n"
	       "		     the value 1 or 2 will give same results.\n"
	       "		     reduced-fps = 1, slows down pixel clock by factor of 1000 / 1001, allowing\n"
	       "		     to support NTSC frame rates like 29.97 or 59.94.\n"
	       "		     Reduced fps flag takes effect only with reduced blanking version 2 and,\n"
	       "		     when refresh rate is an integer multiple of 6, say, fps = 24,30,60 etc.\n"
	       "                     or update all or part of the current timings fields:\n"
	       "                     width=<width>,height=<height>,interlaced=<0/1>,\n"
	       "                     polarities=<polarities mask>,pixelclock=<pixelclock Hz>,\n"
	       "                     hfp=<horizontal front porch>,hs=<horizontal sync>,\n"
	       "                     hbp=<horizontal back porch>,vfp=<vertical front porch>,\n"
	       "                     vs=<vertical sync>,vbp=<vertical back porch>,\n"
	       "                     il_vfp=<vertical front porch for bottom field>,\n"
	       "                     il_vs=<vertical sync for bottom field>,\n"
	       "                     il_vbp=<vertical back porch for bottom field>.\n"
	       "                     clear: start with zeroed timings instead of the current timings.\n"
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

enum timing_opts {
	WIDTH = 0,
	HEIGHT,
	INTERLACED,
	POLARITIES,
	PIXEL_CLOCK,
	HORZ_FRONT_PORCH,
	HORZ_SYNC,
	HORZ_BACK_PORCH,
	VERT_FRONT_PORCH,
	VERT_SYNC,
	VERT_BACK_PORCH,
	IL_VERT_FRONT_PORCH,
	IL_VERT_SYNC,
	IL_VERT_BACK_PORCH,
	INDEX,
	CVT,
	GTF,
	FPS,
	REDUCED_BLANK,
	REDUCED_FPS,
	CLEAR,
	MAX_TIMINGS_OPTIONS
};

static int parse_timing_subopt(char **subopt_str, int *value)
{
	int opt;
	char *opt_str;

	static const char * const subopt_list[] = {
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
		"cvt",
		"gtf",
		"fps",
		"reduced-blanking",
		"reduced-fps",
		"clear",
		NULL
	};

	opt = getsubopt(subopt_str, (char* const*) subopt_list, &opt_str);

	if (opt == -1) {
		fprintf(stderr, "Invalid suboptions specified\n");
		stds_usage();
		exit(1);
	}
	if (opt_str == NULL && opt != CVT && opt != GTF && opt != CLEAR) {
		fprintf(stderr, "No value given to suboption <%s>\n",
				subopt_list[opt]);
		stds_usage();
		exit(1);
	}

	if (opt_str)
		*value = strtol(opt_str, 0L, 0);
	return opt;
}

static void get_cvt_gtf_timings(char *subopt, int standard,
				struct v4l2_bt_timings *bt)
{
	int width = 0;
	int height = 0;
	int fps = 0;
	int r_blank = 0;
	bool interlaced = false;
	bool reduced_fps = false;
	bool timings_valid = false;

	char *subopt_str = subopt;
	while (*subopt_str != '\0') {
		int opt;
		int opt_val;

		opt = parse_timing_subopt(&subopt_str, &opt_val);

		switch (opt) {
		case WIDTH:
			width = opt_val;
			break;
		case HEIGHT:
			height = opt_val;
			break;
		case FPS:
			fps = opt_val;
			break;
		case REDUCED_BLANK:
			r_blank = opt_val;
			break;
		case INTERLACED:
			interlaced = opt_val;
			break;
		case REDUCED_FPS:
			reduced_fps = opt_val;
			break;
		default:
			break;
		}
	}

	if (standard == V4L2_DV_BT_STD_CVT)
		timings_valid = calc_cvt_modeline(width, height, fps, r_blank,
						  interlaced, reduced_fps, bt);
	else
		timings_valid = calc_gtf_modeline(width, height, fps, r_blank,
						  interlaced, bt);

	if (!timings_valid) {
		stds_usage();
		exit(1);
	}
}

static void parse_dv_bt_timings(char *optarg, struct v4l2_dv_timings *dv_timings)
{
	char *subs = optarg;
	struct v4l2_bt_timings *bt = &dv_timings->bt;
	bool parse_cvt_gtf = false;

	dv_timings->type = V4L2_DV_BT_656_1120;

	if (!strcmp(subs, "query")) {
		query_and_set_dv_timings = true;
		return;
	}

	while (*subs != '\0' && !parse_cvt_gtf) {
		int opt;
		int opt_val;

		opt = parse_timing_subopt(&subs, &opt_val);

		switch (opt) {
		case WIDTH:
			bt->width = opt_val;
			break;
		case HEIGHT:
			bt->height = opt_val;
			break;
		case INTERLACED:
			bt->interlaced = opt_val;
			break;
		case POLARITIES:
			bt->polarities = opt_val;
			break;
		case PIXEL_CLOCK:
			bt->pixelclock = opt_val;
			break;
		case HORZ_FRONT_PORCH:
			bt->hfrontporch = opt_val;
			break;
		case HORZ_SYNC:
			bt->hsync = opt_val;
			break;
		case HORZ_BACK_PORCH:
			bt->hbackporch = opt_val;
			break;
		case VERT_FRONT_PORCH:
			bt->vfrontporch = opt_val;
			break;
		case VERT_SYNC:
			bt->vsync = opt_val;
			break;
		case VERT_BACK_PORCH:
			bt->vbackporch = opt_val;
			break;
		case IL_VERT_FRONT_PORCH:
			bt->il_vfrontporch = opt_val;
			break;
		case IL_VERT_SYNC:
			bt->il_vsync = opt_val;
			break;
		case IL_VERT_BACK_PORCH:
			bt->il_vbackporch = opt_val;
			break;
		case INDEX:
			enum_and_set_dv_timings = opt_val;
			break;
		case CVT:
			parse_cvt_gtf = true;
			cleared_dv_timings = true;
			get_cvt_gtf_timings(subs, V4L2_DV_BT_STD_CVT, bt);
			break;
		case GTF:
			parse_cvt_gtf = true;
			cleared_dv_timings = true;
			get_cvt_gtf_timings(subs, V4L2_DV_BT_STD_GTF, bt);
			break;
		case REDUCED_FPS:
			bt->flags |= opt_val ? V4L2_DV_FL_REDUCED_FPS : 0;
			break;
		case CLEAR:
			cleared_dv_timings = true;
			break;
		default:
			stds_usage();
			exit(1);
		}
		set_dv_timing_opts |= 1 << opt;
	}
}

static const flag_def dv_standards_def[] = {
	{ V4L2_DV_BT_STD_CEA861, "CEA-861" },
	{ V4L2_DV_BT_STD_DMT, "DMT" },
	{ V4L2_DV_BT_STD_CVT, "CVT" },
	{ V4L2_DV_BT_STD_GTF, "GTF" },
	{ V4L2_DV_BT_STD_SDI, "SDI" },
	{ 0, NULL }
};

static std::string dvflags2s(unsigned vsync, int val)
{
	std::string s;

	if (val & V4L2_DV_FL_REDUCED_BLANKING)
		s += vsync == 8 ?
			"reduced blanking v2, " :
			"reduced blanking, ";
	if (val & V4L2_DV_FL_CAN_REDUCE_FPS)
		s += "framerate can be reduced by 1/1.001, ";
	if (val & V4L2_DV_FL_REDUCED_FPS)
		s += "framerate is reduced by 1/1.001, ";
	if (val & V4L2_DV_FL_HALF_LINE)
		s += "half-line, ";
	if (val & V4L2_DV_FL_IS_CE_VIDEO)
		s += "CE-video, ";
	if (val & V4L2_DV_FL_FIRST_FIELD_EXTRA_LINE)
		s += "first field has extra line, ";
	if (s.length())
		return s.erase(s.length() - 2, 2);
	return s;
}

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
				dvflags2s(bt->vsync, bt->flags).c_str());
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
		printf("\tFlags: %s\n", dvflags2s(bt->vsync, bt->flags).c_str());
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
		parse_dv_bt_timings(optarg, &dv_timings);
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
		struct v4l2_enum_dv_timings et = {};
		struct v4l2_dv_timings new_dv_timings = {};

		if (query_and_set_dv_timings)
			doioctl(fd, VIDIOC_QUERY_DV_TIMINGS, &new_dv_timings);
		else if (enum_and_set_dv_timings >= 0) {
			__u32 reduced_fps = dv_timings.bt.flags & V4L2_DV_FL_REDUCED_FPS;

			et.index = enum_and_set_dv_timings;
			doioctl(fd, VIDIOC_ENUM_DV_TIMINGS, &et);
			et.timings.bt.flags |= reduced_fps;
			new_dv_timings = et.timings;
		} else if (cleared_dv_timings) {
			new_dv_timings = dv_timings;
		} else {
			struct v4l2_bt_timings *bt = &new_dv_timings.bt;
			struct v4l2_bt_timings *update_bt = &dv_timings.bt;

			doioctl(fd, VIDIOC_G_DV_TIMINGS, &new_dv_timings);
			for (unsigned i = 0; i < MAX_TIMINGS_OPTIONS; i++) {
				if (!(set_dv_timing_opts & (1 << i)))
					continue;
				switch (i) {
				case WIDTH:
					bt->width = update_bt->width;
					break;
				case HEIGHT:
					bt->height = update_bt->height;
					break;
				case INTERLACED:
					bt->interlaced = update_bt->interlaced;
					break;
				case POLARITIES:
					bt->polarities = update_bt->polarities;
					break;
				case PIXEL_CLOCK:
					bt->pixelclock = update_bt->pixelclock;
					break;
				case HORZ_FRONT_PORCH:
					bt->hfrontporch = update_bt->hfrontporch;
					break;
				case HORZ_SYNC:
					bt->hsync = update_bt->hsync;
					break;
				case HORZ_BACK_PORCH:
					bt->hbackporch = update_bt->hbackporch;
					break;
				case VERT_FRONT_PORCH:
					bt->vfrontporch = update_bt->vfrontporch;
					break;
				case VERT_SYNC:
					bt->vsync = update_bt->vsync;
					break;
				case VERT_BACK_PORCH:
					bt->vbackporch = update_bt->vbackporch;
					break;
				case IL_VERT_FRONT_PORCH:
					bt->il_vfrontporch = update_bt->il_vfrontporch;
					break;
				case IL_VERT_SYNC:
					bt->il_vsync = update_bt->il_vsync;
					break;
				case IL_VERT_BACK_PORCH:
					bt->il_vbackporch = update_bt->il_vbackporch;
					break;
				case REDUCED_FPS:
					bt->flags &= ~V4L2_DV_FL_REDUCED_FPS;
					bt->flags |= update_bt->flags & V4L2_DV_FL_REDUCED_FPS;
					break;
				}
			}
		}
		if (verbose)
			print_dv_timings(&new_dv_timings);
		if (doioctl(fd, VIDIOC_S_DV_TIMINGS, &new_dv_timings) >= 0) {
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
		struct v4l2_dv_timings_cap dv_timings_cap = {};

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
		struct v4l2_enum_dv_timings dv_enum_timings = {};

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
