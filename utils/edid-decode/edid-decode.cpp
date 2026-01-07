// SPDX-License-Identifier: MIT
/*
 * Copyright 2006-2012 Red Hat, Inc.
 * Copyright 2018-2020 Cisco Systems, Inc. and/or its affiliates. All rights reserved.
 *
 * Author: Adam Jackson <ajax@nwnk.net>
 * Maintainer: Hans Verkuil <hverkuil+cisco@kernel.org>
 */

#include <ctype.h>
#include <fcntl.h>
#include <getopt.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "edid-decode.h"

#define STR(x) #x
#define STRING(x) STR(x)

static edid_state state;

static unsigned char edid[EDID_PAGE_SIZE * EDID_MAX_BLOCKS];
static bool odd_hex_digits;

enum output_format {
	OUT_FMT_DEFAULT,
	OUT_FMT_HEX,
	OUT_FMT_RAW,
	OUT_FMT_CARRAY,
	OUT_FMT_XML,
};

/*
 * Options
 * Please keep in alphabetical order of the short option.
 * That makes it easier to see which options are still free.
 */
enum Option {
	OptI2CAdapter = 'a',
	OptCheck = 'c',
	OptCheckInline = 'C',
	OptEld = 'E',
	OptFBModeTimings = 'F',
	OptHelp = 'h',
	OptOnlyHexDump = 'H',
	OptInfoFrame = 'I',
	OptLongTimings = 'L',
	OptNativeResolution = 'n',
	OptNTSC = 'N',
	OptOutputFormat = 'o',
	OptPreferredTimings = 'p',
	OptPhysicalAddress = 'P',
	OptSkipHexDump = 's',
	OptShortTimings = 'S',
	OptUTF8 = 'u',
	OptV4L2Timings = 'V',
	OptXModeLineTimings = 'X',
	OptSkipSHA = 128,
	OptHideSerialNumbers,
	OptReplaceUniqueIDs,
	OptVersion,
	OptDiag,
	OptI2CEDID,
	OptI2CHDCP,
	OptI2CHDCPRi,
	OptI2CTestReliability,
	OptSTD,
	OptDMT,
	OptVIC,
	OptHDMIVIC,
	OptCVT,
	OptGTF,
	OptOVT,
	OptListEstTimings,
	OptListDMTs,
	OptListVICs,
	OptListHDMIVICs,
	OptListRIDTimings,
	OptListRIDs,
	OptLast = 256
};

static char options[OptLast];

#ifndef __EMSCRIPTEN__
static struct option long_options[] = {
	{ "help", no_argument, 0, OptHelp },
	{ "output-format", required_argument, 0, OptOutputFormat },
	{ "native-resolution", no_argument, 0, OptNativeResolution },
	{ "preferred-timings", no_argument, 0, OptPreferredTimings },
	{ "physical-address", no_argument, 0, OptPhysicalAddress },
	{ "skip-hex-dump", no_argument, 0, OptSkipHexDump },
	{ "only-hex-dump", no_argument, 0, OptOnlyHexDump },
	{ "skip-sha", no_argument, 0, OptSkipSHA },
	{ "hide-serial-numbers", no_argument, 0, OptHideSerialNumbers },
	{ "replace-unique-ids", no_argument, 0, OptReplaceUniqueIDs },
	{ "utf8", no_argument, 0, OptUTF8 },
	{ "version", no_argument, 0, OptVersion },
	{ "check-inline", no_argument, 0, OptCheckInline },
	{ "check", no_argument, 0, OptCheck },
	{ "short-timings", no_argument, 0, OptShortTimings },
	{ "long-timings", no_argument, 0, OptLongTimings },
	{ "ntsc", no_argument, 0, OptNTSC },
	{ "xmodeline", no_argument, 0, OptXModeLineTimings },
	{ "fbmode", no_argument, 0, OptFBModeTimings },
	{ "v4l2-timings", no_argument, 0, OptV4L2Timings },
	{ "diagonal", required_argument, 0, OptDiag },
#ifdef __HAS_I2C_DEV__
	{ "i2c-adapter", required_argument, 0, OptI2CAdapter },
	{ "i2c-edid", no_argument, 0, OptI2CEDID },
	{ "i2c-hdcp", no_argument, 0, OptI2CHDCP },
	{ "i2c-hdcp-ri", required_argument, 0, OptI2CHDCPRi },
	{ "i2c-test-reliability", optional_argument, 0, OptI2CTestReliability },
#endif
	{ "std", required_argument, 0, OptSTD },
	{ "dmt", required_argument, 0, OptDMT },
	{ "vic", required_argument, 0, OptVIC },
	{ "hdmi-vic", required_argument, 0, OptHDMIVIC },
	{ "cvt", required_argument, 0, OptCVT },
	{ "gtf", required_argument, 0, OptGTF },
	{ "ovt", required_argument, 0, OptOVT },
	{ "list-established-timings", no_argument, 0, OptListEstTimings },
	{ "list-dmts", no_argument, 0, OptListDMTs },
	{ "list-vics", no_argument, 0, OptListVICs },
	{ "list-hdmi-vics", no_argument, 0, OptListHDMIVICs },
	{ "list-rid-timings", required_argument, 0, OptListRIDTimings },
	{ "list-rids", no_argument, 0, OptListRIDs },
	{ "infoframe", required_argument, 0, OptInfoFrame },
	{ "eld", required_argument, 0, OptEld },
	{ 0, 0, 0, 0 }
};

static void usage(void)
{
	printf("Usage: edid-decode <options> [in [out]]\n"
	       "  [in]                  EDID file to parse. Read from standard input if none given\n"
	       "                        and neither --infoframe nor --eld was not used, or if the\n"
	       "                        input filename is '-'.\n"
	       "  [out]                 Output the read EDID to this file. Write to standard output\n"
	       "                        if the output filename is '-'.\n"
	       "\nOptions:\n"
	       "  -o, --output-format <fmt>\n"
	       "                        If [out] is specified, then write the EDID in this format.\n"
	       "                        <fmt> is one of:\n"
	       "                        hex:    hex numbers in ascii text (default for stdout)\n"
	       "                        raw:    binary data (default unless writing to stdout)\n"
	       "                        carray: c-program struct\n"
	       "                        xml:    XML data\n"
	       "  -c, --check           Check if the EDID conforms to the standards, failures and\n"
	       "                        warnings are reported at the end.\n"
	       "  -C, --check-inline    Check if the EDID conforms to the standards, failures and\n"
	       "                        warnings are reported inline.\n"
	       "  -n, --native-resolution Report the native resolution.\n"
	       "  -p, --preferred-timings Report the preferred timings.\n"
	       "  -P, --physical-address Only report the CEC physical address.\n"
	       "  -S, --short-timings   Report all video timings in a short format.\n"
	       "  -L, --long-timings    Report all video timings in a long format.\n"
	       "  -N, --ntsc            Report the video timings suitable for NTSC-based video.\n"
	       "  -X, --xmodeline       Report all long video timings in Xorg.conf format.\n"
	       "  -F, --fbmode          Report all long video timings in fb.modes format.\n"
	       "  -V, --v4l2-timings    Report all long video timings in v4l2-dv-timings.h format.\n"
	       "  -s, --skip-hex-dump   Skip the initial hex dump of the EDID.\n"
	       "  -H, --only-hex-dump   Only output the hex dump of the EDID.\n"
	       "  --skip-sha            Skip the SHA report.\n"
	       "  --hide-serial-numbers Hide serial numbers with '...'.\n"
	       "  --replace-unique-ids  Replace unique IDs (serial numbers, dates, Container IDs) with fixed values.\n"
	       "  -u, --utf8            Convert strings in EDIDs to UTF-8.\n"
	       "  --version             Show the edid-decode version (SHA).\n"
	       "  --diagonal <inches>   Set the display's diagonal in inches.\n"
#ifdef __HAS_I2C_DEV__
	       "  -a, --i2c-adapter <dev> Use <dev> to access the DDC lines.\n"
	       "                        If <dev> starts with a digit, then /dev/i2c-<dev> is used.\n"
	       "  --i2c-edid		Read the EDID from the DDC lines.\n"
	       "  --i2c-hdcp		Read the HDCP from the DDC lines.\n"
	       "  --i2c-hdcp-ri=<t>	Read and print the HDCP Ri information every <t> seconds.\n"
	       "  --i2c-test-reliability [duration=<secs>][,sleep=<msecs>]\n"
	       "                        Read the EDID continuously for <secs> seconds (default=0=forever), with a sleep\n"
	       "                        of <msecs> milliseconds (default value is 50 ms) in between each read.\n"
	       "                        Report a FAIL if there are mismatches between EDIDs.\n"
	       "                        This tests the i2c communication towards the display.\n"
#endif
	       "  --std <byte1>,<byte2> Show the standard timing represented by these two bytes.\n"
	       "  --dmt <dmt>           Show the timings for the DMT with the given DMT ID.\n"
	       "  --vic <vic>           Show the timings for this VIC.\n"
	       "  --hdmi-vic <hdmivic>  Show the timings for this HDMI VIC.\n"
	       "  --cvt w=<width>,h=<height>,fps=<fps>[,rb=<rb>][,interlaced][,overscan][,alt][,hblank=<hblank>][,vblank=<vblank>][,early-vsync]\n"
	       "                        Calculate the CVT timings for the given format.\n"
	       "                        <fps> is frames per second for progressive timings,\n"
	       "                        or fields per second for interlaced timings.\n"
	       "                        <rb> can be 0 (no reduced blanking, default), or\n"
	       "                        1-3 for the reduced blanking version.\n"
	       "                        If 'interlaced' is given, then this is an interlaced format.\n"
	       "                        If 'overscan' is given, then this is an overscanned format.\n"
	       "                        If 'alt' is given and <rb>=2, then report the timings\n"
	       "                        optimized for video: 1000 / 1001 * <fps>.\n"
	       "                        If 'alt' is given and <rb>=3, then the horizontal blanking\n"
	       "                        is 160 instead of 80 pixels.\n"
	       "                        If 'hblank' is given and <rb>=3, then the horizontal blanking\n"
	       "                        is <hblank> pixels (range of 80-200), overriding 'alt'.\n"
	       "                        If 'vblank' is given and <rb>=3, then the vertical blanking\n"
	       "                        time is <vblank> microseconds (range of 460-705 or 300-440).\n"
	       "                        If 'early-vsync' is given and <rb=3>, then select early vsync.\n"
	       "  --gtf w=<width>,h=<height>[,fps=<fps>][,horfreq=<horfreq>][,pixclk=<pixclk>][,interlaced]\n"
	       "        [,overscan][,secondary][,C=<c>][,M=<m>][,K=<k>][,J=<j>]\n"
	       "                        Calculate the GTF timings for the given format.\n"
	       "                        <fps> is frames per second for progressive timings,\n"
	       "                        or fields per second for interlaced timings.\n"
	       "                        <horfreq> is the horizontal frequency in kHz.\n"
	       "                        <pixclk> is the pixel clock frequency in MHz.\n"
	       "                        Only one of fps, horfreq or pixclk must be given.\n"
	       "                        If 'interlaced' is given, then this is an interlaced format.\n"
	       "                        If 'overscan' is given, then this is an overscanned format.\n"
	       "                        If 'secondary' is given, then the secondary GTF is used for\n"
	       "                        reduced blanking, where <c>, <m>, <k> and <j> are parameters\n"
	       "                        for the secondary curve.\n"
	       "  --ovt (rid=<rid>|w=<width>,h=<height>),fps=<fps>\n"
	       "                        Calculate the OVT timings for the given format.\n"
	       "                        Either specify a RID or explicitly specify width and height.\n"
	       "  --list-established-timings List all known Established Timings.\n"
	       "  --list-dmts           List all known DMTs.\n"
	       "  --list-vics           List all known VICs.\n"
	       "  --list-hdmi-vics      List all known HDMI VICs.\n"
	       "  --list-rids           List all known RIDs.\n"
	       "  --list-rid-timings <rid> List all timings for RID <rid> or all known RIDs if <rid> is 0.\n"
	       "  -I, --infoframe <file> Parse the InfoFrame from <file> (or stdin if '-' was specified) that was sent to this display.\n"
	       "                        This option can be specified multiple times for different InfoFrame files.\n"
	       "  -E, --eld <file>      Parse the EDID-Like Data, ELD from <file> (or stdin if '-' was specified).\n"
	       "                        This option can be specified multiple times for different ELD files.\n"
	       "  -h, --help            Display this help message.\n");
}
#endif

static std::string s_msgs[EDID_MAX_BLOCKS + 1][2];

static void print_version()
{
#define STR(x) #x
#define STRING(x) STR(x)
#ifndef __EMSCRIPTEN__
	printf("edid-decode %s%s\n", PACKAGE_VERSION, STRING(GIT_COMMIT_CNT));
#endif
	if (strlen(STRING(GIT_SHA)))
		printf("edid-decode SHA: %s %s\n", STRING(GIT_SHA), STRING(GIT_COMMIT_DATE));
}

void msg(bool is_warn, const char *fmt, ...)
{
	char buf[1024] = "";
	va_list ap;

	va_start(ap, fmt);
	vsprintf(buf, fmt, ap);
	va_end(ap);

	if (is_warn)
		state.warnings++;
	else
		state.failures++;
	if (state.data_block.empty())
		s_msgs[state.block_nr][is_warn] += std::string("  ") + buf;
	else
		s_msgs[state.block_nr][is_warn] += "  " + state.data_block + ": " + buf;

	if (options[OptCheckInline])
		printf("%s: %s", is_warn ? "WARN" : "FAIL", buf);
}

static void show_msgs(bool is_warn)
{
	printf("\n%s:\n\n", is_warn ? "Warnings" : "Failures");
	for (unsigned i = 0; i < state.num_blocks; i++) {
		if (s_msgs[i][is_warn].empty())
			continue;
		printf("Block %u, %s:\n%s",
		       i, block_name(edid[i * EDID_PAGE_SIZE]).c_str(),
		       s_msgs[i][is_warn].c_str());
	}
	if (s_msgs[EDID_MAX_BLOCKS][is_warn].empty())
		return;
	printf("EDID:\n%s",
	       s_msgs[EDID_MAX_BLOCKS][is_warn].c_str());
}


void replace_checksum(unsigned char *x, size_t len)
{
	unsigned char sum = 0;
	unsigned i;

	for (i = 0; i < len - 1; i++)
		sum += x[i];
	x[len - 1] = -sum & 0xff;
}

void do_checksum(const char *prefix, const unsigned char *x, size_t len, size_t checksum_pos,
		 unsigned unused_bytes)
{
	unsigned char check = x[checksum_pos];
	unsigned char sum = 0;
	unsigned i;

	for (i = 0; i < len; i++) {
		if (i != checksum_pos)
			sum += x[i];
	}

	printf("%sChecksum: 0x%02hhx", prefix, check);
	if ((unsigned char)(check + sum) != 0) {
		printf(" (should be 0x%02x)", -sum & 0xff);
		fail("Invalid checksum 0x%02x (should be 0x%02x).\n",
		     check, -sum & 0xff);
	}
	if (unused_bytes)
		printf("  Unused space in Extension Block: %u byte%s",
		       unused_bytes, unused_bytes > 1 ? "s" : "");
	printf("\n");
}

unsigned gcd(unsigned a, unsigned b)
{
	while (b) {
		unsigned t = b;

		b = a % b;
		a = t;
	}
	return a;
}

void calc_ratio(struct timings *t)
{
	unsigned d = gcd(t->hact, t->vact);

	if (d == 0) {
		t->hratio = t->vratio = 0;
		return;
	}
	t->hratio = t->hact / d;
	t->vratio = t->vact / d;

	if (t->hratio == 8 && t->vratio == 5) {
		t->hratio = 16;
		t->vratio = 10;
	}
}

unsigned calc_fps(const struct timings *t)
{
	unsigned vact = t->vact;
	unsigned vbl = t->vfp + t->vsync + t->vbp + 2 * t->vborder;
	unsigned hbl = t->hfp + t->hsync + t->hbp + 2 * t->hborder;
	unsigned htotal = t->hact + hbl;

	if (t->interlaced)
		vact /= 2;

	double vtotal = vact + vbl;

	if (t->even_vtotal)
		vtotal = vact + t->vfp + t->vsync + t->vbp;
	else if (t->interlaced)
		vtotal = vact + t->vfp + t->vsync + t->vbp + 0.5;

	return t->pixclk_khz * 1000.0 / (htotal * vtotal);
}

std::string edid_state::dtd_type(unsigned cnt)
{
	unsigned len = std::to_string(cta.preparsed_total_dtds).length();
	char buf[16];
	sprintf(buf, "DTD %*u", len, cnt);
	return buf;
}

bool match_timings(const timings &t1, const timings &t2)
{
	if (t1.hact != t2.hact ||
	    t1.vact != t2.vact ||
	    t1.rb != t2.rb ||
	    t1.interlaced != t2.interlaced ||
	    t1.hfp != t2.hfp ||
	    t1.hbp != t2.hbp ||
	    t1.hsync != t2.hsync ||
	    t1.pos_pol_hsync != t2.pos_pol_hsync ||
	    t1.hratio != t2.hratio ||
	    t1.vfp != t2.vfp ||
	    t1.vbp != t2.vbp ||
	    t1.vsync != t2.vsync ||
	    t1.pos_pol_vsync != t2.pos_pol_vsync ||
	    t1.vratio != t2.vratio ||
	    t1.pixclk_khz != t2.pixclk_khz)
		return false;
	return true;
}

static void or_str(std::string &s, const std::string &flag, unsigned &num_flags)
{
	if (!num_flags)
		s = flag;
	else if (num_flags % 2 == 0)
		s = s + " | \\\n\t\t" + flag;
	else
		s = s + " | " + flag;
	num_flags++;
}

/*
 * Return true if the timings are a close, but not identical,
 * match. The only differences allowed are polarities and
 * porches and syncs, provided the total blanking remains the
 * same.
 */
bool timings_close_match(const timings &t1, const timings &t2)
{
	// We don't want to deal with borders, you're on your own
	// if you are using those.
	if (t1.hborder || t1.vborder ||
	    t2.hborder || t2.vborder)
		return false;
	if (t1.hact != t2.hact || t1.vact != t2.vact ||
	    t1.interlaced != t2.interlaced ||
	    t1.pixclk_khz != t2.pixclk_khz ||
	    t1.hfp + t1.hsync + t1.hbp != t2.hfp + t2.hsync + t2.hbp ||
	    t1.vfp + t1.vsync + t1.vbp != t2.vfp + t2.vsync + t2.vbp)
		return false;
	if (t1.hfp == t2.hfp &&
	    t1.hsync == t2.hsync &&
	    t1.hbp == t2.hbp &&
	    t1.pos_pol_hsync == t2.pos_pol_hsync &&
	    t1.vfp == t2.vfp &&
	    t1.vsync == t2.vsync &&
	    t1.vbp == t2.vbp &&
	    t1.pos_pol_vsync == t2.pos_pol_vsync)
		return false;
	return true;
}

static void print_modeline(unsigned indent, const struct timings *t, double refresh)
{
	unsigned offset = (!t->even_vtotal && t->interlaced) ? 1 : 0;
	unsigned hfp = t->hborder + t->hfp;
	unsigned hbp = t->hborder + t->hbp;
	unsigned vfp = t->vborder + t->vfp;
	unsigned vbp = t->vborder + t->vbp;

	printf("%*sModeline \"%ux%u_%.2f%s\" %.3f  %u %u %u %u  %u %u %u %u  %cHSync",
	       indent, "",
	       t->hact, t->vact, refresh,
	       t->interlaced ? "i" : "", t->pixclk_khz / 1000.0,
	       t->hact, t->hact + hfp, t->hact + hfp + t->hsync,
	       t->hact + hfp + t->hsync + hbp,
	       t->vact, t->vact + vfp, t->vact + vfp + t->vsync,
	       t->vact + vfp + t->vsync + vbp + offset,
	       t->pos_pol_hsync ? '+' : '-');
	if (!t->no_pol_vsync)
		printf(" %cVSync", t->pos_pol_vsync ? '+' : '-');
	if (t->interlaced)
		printf(" Interlace");
	printf("\n");
}

static void print_fbmode(unsigned indent, const struct timings *t,
			 double refresh, double hor_freq_khz)
{
	printf("%*smode \"%ux%u-%u%s\"\n",
	       indent, "",
	       t->hact, t->vact,
	       (unsigned)(0.5 + (t->interlaced ? refresh / 2.0 : refresh)),
	       t->interlaced ? "-lace" : "");
	printf("%*s# D: %.2f MHz, H: %.3f kHz, V: %.2f Hz\n",
	       indent + 8, "",
	       t->pixclk_khz / 1000.0, hor_freq_khz, refresh);
	printf("%*sgeometry %u %u %u %u 32\n",
	       indent + 8, "",
	       t->hact, t->vact, t->hact, t->vact);
	unsigned mult = t->interlaced ? 2 : 1;
	unsigned offset = !t->even_vtotal && t->interlaced;
	unsigned hfp = t->hborder + t->hfp;
	unsigned hbp = t->hborder + t->hbp;
	unsigned vfp = t->vborder + t->vfp;
	unsigned vbp = t->vborder + t->vbp;
	printf("%*stimings %llu %d %d %d %u %u %u\n",
	       indent + 8, "",
	       (unsigned long long)(1000000000.0 / (double)(t->pixclk_khz) + 0.5),
	       hbp, hfp, mult * vbp, mult * vfp + offset, t->hsync, mult * t->vsync);
	if (t->interlaced)
		printf("%*slaced true\n", indent + 8, "");
	if (t->pos_pol_hsync)
		printf("%*shsync high\n", indent + 8, "");
	if (t->pos_pol_vsync)
		printf("%*svsync high\n", indent + 8, "");
	printf("%*sendmode\n", indent, "");
}

static void print_v4l2_timing(const struct timings *t,
			      double refresh, const char *type)
{
	printf("\t#define V4L2_DV_BT_%uX%u%c%u_%02u { \\\n",
	       t->hact, t->vact, t->interlaced ? 'I' : 'P',
	       (unsigned)refresh, (unsigned)(0.5 + 100.0 * (refresh - (unsigned)refresh)));
	printf("\t\t.type = V4L2_DV_BT_656_1120, \\\n");
	printf("\t\tV4L2_INIT_BT_TIMINGS(%u, %u, %u, ",
	       t->hact, t->vact, t->interlaced);
	if (!t->pos_pol_hsync && !t->pos_pol_vsync)
		printf("0, \\\n");
	else if (t->pos_pol_hsync && t->pos_pol_vsync)
		printf("\\\n\t\t\tV4L2_DV_HSYNC_POS_POL | V4L2_DV_VSYNC_POS_POL, \\\n");
	else if (t->pos_pol_hsync)
		printf("V4L2_DV_HSYNC_POS_POL, \\\n");
	else
		printf("V4L2_DV_VSYNC_POS_POL, \\\n");
	unsigned hfp = t->hborder + t->hfp;
	unsigned hbp = t->hborder + t->hbp;
	unsigned vfp = t->vborder + t->vfp;
	unsigned vbp = t->vborder + t->vbp;
	printf("\t\t\t%lluULL, %d, %u, %d, %u, %u, %d, %u, %u, %d, \\\n",
	       t->pixclk_khz * 1000ULL, hfp, t->hsync, hbp,
	       vfp, t->vsync, vbp,
	       t->interlaced ? vfp : 0,
	       t->interlaced ? t->vsync : 0,
	       t->interlaced ? vbp + !t->even_vtotal : 0);

	std::string flags;
	unsigned num_flags = 0;
	unsigned vic = 0;
	unsigned hdmi_vic = 0;
	const char *std = "0";

	if (t->interlaced && !t->even_vtotal)
		or_str(flags, "V4L2_DV_FL_HALF_LINE", num_flags);
	if (!memcmp(type, "VIC", 3)) {
		or_str(flags, "V4L2_DV_FL_HAS_CEA861_VIC", num_flags);
		or_str(flags, "V4L2_DV_FL_IS_CE_VIDEO", num_flags);
		vic = strtoul(type + 4, 0, 0);
	}
	if (!memcmp(type, "HDMI VIC", 8)) {
		or_str(flags, "V4L2_DV_FL_HAS_HDMI_VIC", num_flags);
		or_str(flags, "V4L2_DV_FL_IS_CE_VIDEO", num_flags);
		hdmi_vic = strtoul(type + 9, 0, 0);
		vic = hdmi_vic_to_vic(hdmi_vic);
		if (vic)
			or_str(flags, "V4L2_DV_FL_HAS_CEA861_VIC", num_flags);
	}
	if (vic && (fmod(refresh, 6)) == 0.0)
		or_str(flags, "V4L2_DV_FL_CAN_REDUCE_FPS", num_flags);
	if (t->rb)
		or_str(flags, "V4L2_DV_FL_REDUCED_BLANKING", num_flags);
	if (t->hratio && t->vratio)
		or_str(flags, "V4L2_DV_FL_HAS_PICTURE_ASPECT", num_flags);

	if (!memcmp(type, "VIC", 3) || !memcmp(type, "HDMI VIC", 8))
		std = "V4L2_DV_BT_STD_CEA861";
	else if (!memcmp(type, "DMT", 3))
		std = "V4L2_DV_BT_STD_DMT";
	else if (!memcmp(type, "CVT", 3))
		std = "V4L2_DV_BT_STD_CVT";
	else if (!memcmp(type, "GTF", 3))
		std = "V4L2_DV_BT_STD_GTF";
	printf("\t\t\t%s, \\\n", std);
	printf("\t\t\t%s, \\\n", flags.empty() ? "0" : flags.c_str());
	printf("\t\t\t{ %u, %u }, %u, %u) \\\n",
	       t->hratio, t->vratio, vic, hdmi_vic);
	printf("\t}\n");
}

static void print_detailed_timing(unsigned indent, const struct timings *t)
{
	printf("%*sHfront %4d Hsync %3u Hback %4d Hpol %s",
	       indent, "",
	       t->hfp, t->hsync, t->hbp, t->pos_pol_hsync ? "P" : "N");
	if (t->hborder)
		printf(" Hborder %u", t->hborder);
	printf("\n");

	printf("%*sVfront %4u Vsync %3u Vback %4d",
	       indent, "", t->vfp, t->vsync, t->vbp);
	if (!t->no_pol_vsync)
		printf(" Vpol %s", t->pos_pol_vsync ? "P" : "N");
	if (t->vborder)
		printf(" Vborder %u", t->vborder);
	if (t->even_vtotal) {
		printf(" Both Fields");
	} else if (t->interlaced) {
		printf(" Vfront +0.5 Odd Field\n");
		printf("%*sVfront %4d Vsync %3u Vback %4d",
		       indent, "", t->vfp, t->vsync, t->vbp);
		if (!t->no_pol_vsync)
			printf(" Vpol %s", t->pos_pol_vsync ? "P" : "N");
		if (t->vborder)
			printf(" Vborder %u", t->vborder);
		printf(" Vback  +0.5 Even Field");
	}
	printf("\n");
}

bool edid_state::print_timings(const char *prefix, const struct timings *t,
			       const char *type, const char *flags,
			       bool detailed, bool do_checks, unsigned ntsc)
{
	if (!t) {
		// Should not happen
		if (do_checks)
			fail("Unknown video timings.\n");
		return false;
	}

	if (detailed && options[OptShortTimings])
		detailed = false;
	if (options[OptLongTimings])
		detailed = true;

	unsigned vact = t->vact;
	unsigned hbl = t->hfp + t->hsync + t->hbp + 2 * t->hborder;
	unsigned vbl = t->vfp + t->vsync + t->vbp + 2 * t->vborder;
	unsigned htotal = t->hact + hbl;
	double hor_freq_khz = htotal ? (double)t->pixclk_khz / htotal : 0;

	if (t->interlaced)
		vact /= 2;

	double out_hor_freq_khz = hor_freq_khz;
	if (t->ycbcr420)
		hor_freq_khz /= 2;

	double vtotal = vact + vbl;

	bool ok = true;

	if (!t->hact || !hbl || !t->hfp || !t->hsync ||
	    !vact || !vbl || (!t->vfp && !t->interlaced && !t->even_vtotal) || !t->vsync) {
		if (do_checks)
			fail("0 values in the video timing:\n"
			     "    Horizontal Active/Blanking %u/%u\n"
			     "    Horizontal Frontporch/Sync Width %u/%u\n"
			     "    Vertical Active/Blanking %u/%u\n"
			     "    Vertical Frontporch/Sync Width %u/%u\n",
			     t->hact, hbl, t->hfp, t->hsync, vact, vbl, t->vfp, t->vsync);
		ok = false;
	}

	if (t->even_vtotal)
		vtotal = vact + t->vfp + t->vsync + t->vbp;
	else if (t->interlaced)
		vtotal = vact + t->vfp + t->vsync + t->vbp + 0.5;

	double refresh = t->pixclk_khz * 1000.0 / (htotal * vtotal);
	double pixclk = t->pixclk_khz * 1000.0;
	if (((ntsc > 1 && options[OptNTSC]) || ntsc == 1) && fmod(refresh, 6.0) == 0) {
		const double ntsc_fact = 1000.0 / 1001.0;
		pixclk *= ntsc_fact;
		refresh *= ntsc_fact;
		out_hor_freq_khz *= ntsc_fact;
	}

	std::string s;
	unsigned rb = t->rb & ~RB_ALT;
	if (rb) {
		bool alt = t->rb & RB_ALT;
		s = "RB";
		if (rb == RB_CVT_V2)
			s += std::string("v2") + (alt ? ",video-optimized" : "");
		else if (rb == RB_CVT_V3)
			s += std::string("v3") + (alt ? ",h-blank-160" : "");
	}
	add_str(s, flags);
	if (t->hsize_mm || t->vsize_mm)
		add_str(s, std::to_string(t->hsize_mm) + " mm x " + std::to_string(t->vsize_mm) + " mm");
	if (t->hsize_mm > dtd_max_hsize_mm)
		dtd_max_hsize_mm = t->hsize_mm;
	if (t->vsize_mm > dtd_max_vsize_mm)
		dtd_max_vsize_mm = t->vsize_mm;
	if (!s.empty())
		s = " (" + s + ")";
	unsigned pixclk_khz = t->pixclk_khz / (t->ycbcr420 ? 2 : 1);

	char buf[10];

	sprintf(buf, "%u%s", t->vact, t->interlaced ? "i" : "");
	printf("%s%s: %5ux%-5s %10.6f Hz %3u:%-3u %8.3f kHz %13.6f MHz%s\n",
	       prefix, type,
	       t->hact, buf,
	       refresh,
	       t->hratio, t->vratio,
	       out_hor_freq_khz,
	       pixclk / 1000000.0,
	       s.c_str());

	unsigned len = strlen(prefix) + 2;

	if (!t->ycbcr420 && detailed && options[OptXModeLineTimings])
		print_modeline(len, t, refresh);
	else if (!t->ycbcr420 && detailed && options[OptFBModeTimings])
		print_fbmode(len, t, refresh, hor_freq_khz);
	else if (!t->ycbcr420 && detailed && options[OptV4L2Timings])
		print_v4l2_timing(t, refresh, type);
	else if (detailed)
		print_detailed_timing(len + strlen(type) + 6, t);

	if (!do_checks)
		return ok;

	if (!memcmp(type, "DTD", 3)) {
		unsigned vic, dmt;
		const timings *vic_t = cta_close_match_to_vic(*t, vic);

		// We report this even if there is no CTA block since it
		// is still likely that the actual VIC timings were intended.
		if (vic_t)
			warn("DTD is similar but not identical to VIC %u.\n", vic);

		if (cta_matches_vic(*t, vic) && has_cta &&
		    !cta.preparsed_has_vic[0][vic]) {
			warn("DTD is identical to VIC %u, which is not present in the CTA Ext Block.\n", vic);

			if (cta.preparsed_max_vic_pixclk_khz && t->pixclk_khz > 340000 &&
			    t->pixclk_khz > cta.preparsed_max_vic_pixclk_khz)
				cta.warn_about_hdmi_2x_dtd = true;
		}

		const timings *dmt_t = close_match_to_dmt(*t, dmt);
		if (!vic_t && dmt_t)
			warn("DTD is similar but not identical to DMT 0x%02x.\n", dmt);
	}

	if (refresh) {
		min_vert_freq_hz = min(min_vert_freq_hz, refresh);
		max_vert_freq_hz = max(max_vert_freq_hz, refresh);
	}
	if (hor_freq_khz) {
		min_hor_freq_hz = min(min_hor_freq_hz, hor_freq_khz * 1000.0);
		max_hor_freq_hz = max(max_hor_freq_hz, hor_freq_khz * 1000.0);
		max_pixclk_khz = max(max_pixclk_khz, pixclk_khz);
		if (t->pos_pol_hsync && !t->pos_pol_vsync && t->vsync == 3)
			base.max_pos_neg_hor_freq_khz = hor_freq_khz;
	}

	if (t->ycbcr420 && t->pixclk_khz < 590000)
		warn_once("Some YCbCr 4:2:0 timings are invalid for HDMI 2.1 (which requires an RGB timings pixel rate >= 590 MHz).\n");
	if (t->hfp <= 0)
		fail("0 or negative horizontal front porch.\n");
	if (t->hbp <= 0)
		fail("0 or negative horizontal back porch.\n");
	if (t->vbp <= 0)
		fail("0 or negative vertical back porch.\n");
	if (!base.max_display_width_mm && !base.max_display_height_mm) {
		/* this is valid */
	} else if (!t->hsize_mm && !t->vsize_mm) {
		/* this is valid */
	} else if (cta.preparsed_image_size == hdmi_image_size_ratio) {
		/* this is valid */
	} else if (t->hsize_mm > base.max_display_width_mm + 9 ||
		   t->vsize_mm > base.max_display_height_mm + 9) {
		fail("Mismatch of image size %ux%u mm vs display size %ux%u mm.\n",
		     t->hsize_mm, t->vsize_mm, base.max_display_width_mm, base.max_display_height_mm);
	} else if (t->hsize_mm < base.max_display_width_mm - 9 &&
		   t->vsize_mm < base.max_display_height_mm - 9) {
		fail("Mismatch of image size %ux%u mm vs display size %ux%u mm.\n",
		     t->hsize_mm, t->vsize_mm, base.max_display_width_mm, base.max_display_height_mm);
	}
	if (t->hsize_mm && t->vsize_mm) {
		if (t->hsize_mm < 100 || t->vsize_mm < 100) {
			warn("Dubious image size (%ux%u mm is smaller than 100x100 mm).\n",
			     t->hsize_mm, t->vsize_mm);
		} else if (t->hratio && t->vratio) {
			unsigned vsize = (t->hsize_mm * t->vratio) / t->hratio;

			if (vsize < t->vsize_mm - 10 || vsize > t->vsize_mm + 10)
				warn("Image size is %dx%d mm, but based on the picture AR it should be %dx%d mm.\n",
				     t->hsize_mm, t->vsize_mm, t->hsize_mm, vsize);
		}
	}
	return ok;
}

std::string containerid2s(const unsigned char *x)
{
	char buf[40];

	sprintf(buf, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
		x[0], x[1], x[2], x[3],
		x[4], x[5],
		x[6], x[7],
		x[8], x[9],
		x[10], x[11], x[12], x[13], x[14], x[15]);
	return buf;
}

std::string utohex(unsigned char x)
{
	char buf[10];

	sprintf(buf, "0x%02hhx", x);
	return buf;
}

const char *oui_name(unsigned oui, unsigned *ouinum)
{
	unsigned ouinumscratch;
	if (!ouinum) ouinum = &ouinumscratch;
	const char *name;
	switch (oui) {
	#define oneoui(c,k,n) case c: *ouinum = kOUI_##k; name = n; break;
	#include "oui.h"
	default: *ouinum = 0; name = NULL; break;
	}
	return name;
}

void edid_state::data_block_oui(std::string block_name, const unsigned char *x,
	unsigned length, unsigned *ouinum, bool ignorezeros, bool do_ascii, bool big_endian,
	bool silent)
{
	std::string buf;
	char ascii[4];
	unsigned oui;
	const char *ouiname = NULL;
	bool matched_reverse = false;
	bool matched_ascii = false;
	bool valid_ascii = false;

	if (big_endian)
		oui = ((length > 0 ? x[0] : 0) << 16) + ((length > 1 ? x[1] : 0) << 8) + (length > 2 ? x[2] : 0);
	else
		oui = ((length > 2 ? x[2] : 0) << 16) + ((length > 1 ? x[1] : 0) << 8) + (length > 0 ? x[0] : 0);

	buf = ouitohex(oui);
	if (length < 3) {
		sprintf(ascii, "?"); // some characters are null
		if (ouinum) *ouinum = 0; // doesn't match a known OUI
	} else {
		valid_ascii = (x[0] >= 'A' && x[1] >= 'A' && x[2] >= 'A' && x[0] <= 'Z' && x[1] <= 'Z' && x[2] <= 'Z');
		sprintf(ascii, "%c%c%c", x[0], x[1], x[2]);

		ouiname = oui_name(oui, ouinum);
		if (!ouiname) {
			big_endian = !big_endian;
			unsigned reversedoui = ((oui & 0xff) << 16) + (oui & 0x00ff00) + (oui >> 16);
			ouiname = oui_name(reversedoui, ouinum);
			if (ouiname) {
				oui = reversedoui;
				buf = ouitohex(oui);
				matched_reverse = true;
			} else if (do_ascii && valid_ascii) {
				unsigned asciioui = (x[0] << 24) + (x[1] << 16) + (x[2] << 8);
				ouiname = oui_name(asciioui, ouinum);
				if (ouiname) {
					matched_ascii = true;
				}
			}
		}
	}

	std::string name;
	if (ouiname) {
		if (matched_ascii)
			name = block_name + " (" + ouiname + ")" + ", PNP ID '" + ascii + "'";
		else
			name = block_name + " (" + ouiname + ")" + ", OUI " + buf;
	} else if (do_ascii && valid_ascii) {
		name = block_name + ", PNP ID '" + ascii + "'";
	} else {
		name = block_name + ", OUI " + buf;
	}
	// assign string to data_block before outputting errors
	data_block = name;

	if (oui || !ignorezeros) {
		if (!silent)
			printf("  %s:\n", data_block.c_str());
		if (length < 3) {
			fail("Data block length (%d) is not enough to contain an OUI.\n", length);
		} else if (ouiname) {
			if (do_ascii && !valid_ascii)
				warn("Expected PNP ID but found OUI.\n");
			if (matched_reverse)
				fail("Endian-ness (%s) of OUI is different than expected (%s).\n", big_endian ? "be" : "le", big_endian ? "le" : "be");
		} else if (!do_ascii && !oui) {
			warn("All zeroes OUI.\n");
		} else if (!do_ascii && valid_ascii) {
			warn("Unknown OUI %s (possible PNP %s).\n", buf.c_str(), ascii);
		}
	}
}

std::string ouitohex(unsigned oui)
{
	char buf[32];

	sprintf(buf, "%02X-%02X-%02X", (oui >> 16) & 0xff, (oui >> 8) & 0xff, oui & 0xff);
	return buf;
}

bool memchk(const unsigned char *x, unsigned len, unsigned char v)
{
	for (unsigned i = 0; i < len; i++)
		if (x[i] != v)
			return false;
	return true;
}

void hex_block(const char *prefix, const unsigned char *x,
	       unsigned length, bool show_ascii, unsigned step)
{
	unsigned i, j;

	for (i = 0; i < length; i += step) {
		unsigned len = min(step, length - i);

		printf("%s", prefix);
		for (j = 0; j < len; j++)
			printf("%s%02x", j ? " " : "", x[i + j]);

		if (show_ascii) {
			for (j = len; j < step; j++)
				printf("   ");
			printf(" '");
			for (j = 0; j < len; j++)
				printf("%c", x[i + j] >= ' ' && x[i + j] <= '~' ? x[i + j] : '.');
			printf("'");
		}
		printf("\n");
	}
}

static bool edid_add_byte(const char *s, bool two_digits = true)
{
	char buf[3];

	if (state.edid_size == sizeof(edid))
		return false;
	buf[0] = s[0];
	buf[1] = two_digits ? s[1] : 0;
	buf[2] = 0;
	edid[state.edid_size++] = strtoul(buf, NULL, 16);
	return true;
}

static bool extract_edid_quantumdata(const char *start)
{
	/* Parse QuantumData 980 EDID files */
	do {
		start = strstr(start, ">");
		if (!start)
			return false;
		start++;
		for (unsigned i = 0; start[i] && start[i + 1] && i < 256; i += 2)
			if (!edid_add_byte(start + i))
				return false;
		start = strstr(start, "<BLOCK");
	} while (start);
	return state.edid_size;
}

static const char *ignore_chars = ",:;";

static bool extract_edid_hex(const char *s, bool require_two_digits = true)
{
	for (; *s; s++) {
		if (isspace(*s) || strchr(ignore_chars, *s))
			continue;

		if (*s == '0' && tolower(s[1]) == 'x') {
			s++;
			continue;
		}

		/* Read one or two hex digits from the log */
		if (!isxdigit(s[0])) {
			if (state.edid_size && state.edid_size % 128 == 0)
				break;
			return false;
		}
		if (require_two_digits && !isxdigit(s[1])) {
			odd_hex_digits = true;
			return false;
		}
		if (!edid_add_byte(s, isxdigit(s[1])))
			return false;
		if (isxdigit(s[1]))
			s++;
	}
	return state.edid_size;
}

static bool extract_edid_xrandr(const char *start)
{
	static const char indentation1[] = "                ";
	static const char indentation2[] = "\t\t";
	/* Used to detect that we've gone past the EDID property */
	static const char half_indentation1[] = "        ";
	static const char half_indentation2[] = "\t";
	const char *indentation;
	const char *s;

	for (;;) {
		unsigned j;

		/* Get the next start of the line of EDID hex, assuming spaces for indentation */
		s = strstr(start, indentation = indentation1);
		/* Did we skip the start of another property? */
		if (s && s > strstr(start, half_indentation1))
			break;

		/* If we failed, retry assuming tabs for indentation */
		if (!s) {
			s = strstr(start, indentation = indentation2);
			/* Did we skip the start of another property? */
			if (s && s > strstr(start, half_indentation2))
				break;
		}

		if (!s)
			break;

		start = s + strlen(indentation);

		for (j = 0; j < 16; j++, start += 2) {
			/* Read a %02x from the log */
			if (!isxdigit(start[0]) || !isxdigit(start[1])) {
				if (j)
					break;
				return false;
			}
			if (!edid_add_byte(start))
				return false;
		}
	}
	return state.edid_size;
}

static bool extract_edid_xorg(const char *start)
{
	bool find_first_num = true;

	for (; *start; start++) {
		if (find_first_num) {
			const char *s;

			/* skip ahead to the : */
			s = strstr(start, ": \t");
			if (!s)
				s = strstr(start, ":     ");
			if (!s)
				break;
			start = s;
			/* and find the first number */
			while (!isxdigit(start[1]))
				start++;
			find_first_num = false;
			continue;
		} else {
			/* Read a %02x from the log */
			if (!isxdigit(*start)) {
				find_first_num = true;
				continue;
			}
			if (!edid_add_byte(start))
				return false;
			start++;
		}
	}
	return state.edid_size;
}

static bool extract_edid(int fd, FILE *error)
{
	std::vector<char> edid_data;
	char buf[EDID_PAGE_SIZE];

	for (;;) {
		ssize_t i = read(fd, buf, sizeof(buf));

		if (i < 0)
			return false;
		if (i == 0)
			break;
		edid_data.insert(edid_data.end(), buf, buf + i);
	}

	if (edid_data.empty()) {
		state.edid_size = 0;
		return false;
	}
	// Ensure it is safely terminated by a 0 char
	edid_data.push_back('\0');

	const char *data = &edid_data[0];
	const char *start;

	/* Look for edid-decode output */
	start = strstr(data, "edid-decode (hex):");
	if (!start)
		start = strstr(data, "EDID (hex):");
	if (start) {
		bool ok = extract_edid_hex(strchr(start, ':'));
		float diag;

		if (state.diagonal)
			return ok;

		start = strstr(start, "Diagonal: ");
		if (start)
			start = strchr(start, ' ') + 1;
		if (ok && start && sscanf(start, "%f\"", &diag) == 1) {
			state.diagonal = diag;
		}
		return ok;
	}

	/* Look for C-array */
	start = strstr(data, "unsigned char edid[] = {");
	if (start)
		return extract_edid_hex(strchr(start, '{') + 1, false);

	/* Look for QuantumData EDID output */
	start = strstr(data, "<BLOCK");
	if (start)
		return extract_edid_quantumdata(start);

	/* Look for xrandr --verbose output (lines of 16 hex bytes) */
	start = strstr(data, "EDID_DATA:");
	if (!start)
		start = strstr(data, "EDID:");
	if (start)
		return extract_edid_xrandr(start);

	/* Look for an EDID in an Xorg.0.log file */
	start = strstr(data, "EDID (in hex):");
	if (start)
		start = strstr(start, "(II)");
	if (start)
		return extract_edid_xorg(start);

	unsigned i;

	/* Is the EDID provided in hex? */
	for (i = 0; i < 32 && i < edid_data.size() &&
	     (isspace(data[i]) || strchr(ignore_chars, data[i]) ||
	      tolower(data[i]) == 'x' || isxdigit(data[i])); i++);

	if (i == 32)
		return extract_edid_hex(data);

	// Drop the extra '\0' byte since we now assume binary data
	edid_data.pop_back();

	/* Assume binary */
	if (edid_data.size() > sizeof(edid)) {
		if (!options[OptPhysicalAddress])
			fprintf(error, "Binary EDID length %zu is greater than %zu.\n",
				edid_data.size(), sizeof(edid));
		return false;
	}
	memcpy(edid, data, edid_data.size());
	state.edid_size = edid_data.size();
	return true;
}

static int edid_from_file(const char *from_file, FILE *error)
{
#ifdef O_BINARY
	// Windows compatibility
	int flags = O_RDONLY | O_BINARY;
#else
	int flags = O_RDONLY;
#endif
	int fd;

	if (!strcmp(from_file, "-")) {
		from_file = "stdin";
		fd = 0;
	} else if ((fd = open(from_file, flags)) == -1) {
		perror(from_file);
		std::exit(EXIT_FAILURE);
	}

	odd_hex_digits = false;
	if (!extract_edid(fd, error)) {
		if (options[OptPhysicalAddress])
			return -1;
		if (!state.edid_size) {
			fprintf(error, "EDID of '%s' was empty.\n", from_file);
			return -1;
		}
		fprintf(error, "EDID extract of '%s' failed: ", from_file);
		if (odd_hex_digits)
			fprintf(error, "odd number of hexadecimal digits.\n");
		else
			fprintf(error, "unknown format.\n");
		return -1;
	}
	if (state.edid_size % EDID_PAGE_SIZE) {
		if (!options[OptPhysicalAddress])
			fprintf(error, "EDID length %u is not a multiple of %u.\n",
				state.edid_size, EDID_PAGE_SIZE);
		return -1;
	}
	state.num_blocks = state.edid_size / EDID_PAGE_SIZE;
	if (fd != 0)
		close(fd);

	if (memcmp(edid, "\x00\xFF\xFF\xFF\xFF\xFF\xFF\x00", 8)) {
		if (!options[OptPhysicalAddress])
			fprintf(error, "No EDID header found in '%s'.\n", from_file);
		return -1;
	}
	return 0;
}

/* generic extension code */

std::string block_name(unsigned char block)
{
	char buf[10];

	switch (block) {
	case 0x00: return "Base EDID";
	case 0x02: return "CTA-861 Extension Block";
	case 0x10: return "Video Timing Extension Block";
	case 0x20: return "EDID 2.0 Extension Block";
	case 0x40: return "Display Information Extension Block";
	case 0x50: return "Localized String Extension Block";
	case 0x60: return "Microdisplay Interface Extension Block";
	case 0x70: return "DisplayID Extension Block";
	case 0xf0: return "Block Map Extension Block";
	case 0xff: return "Manufacturer-Specific Extension Block";
	default:
		sprintf(buf, " 0x%02x", block);
		return std::string("Unknown EDID Extension Block") + buf;
	}
}

void edid_state::parse_block_map(const unsigned char *x)
{
	unsigned last_valid_block_tag = 0;
	bool fail_once = false;
	unsigned offset = 1;
	unsigned i;

	if (block_nr == 1)
		block_map.saw_block_1 = true;
	else if (!block_map.saw_block_1)
		fail("No EDID Block Map Extension found in block 1.\n");
	else if (block_nr == 128)
		block_map.saw_block_128 = true;

	if (block_nr > 1)
		offset = 128;

	for (i = 1; i < 127; i++) {
		unsigned block = offset + i;

		if (x[i]) {
			last_valid_block_tag++;
			if (i != last_valid_block_tag && !fail_once) {
				fail("Valid block tags are not consecutive.\n");
				fail_once = true;
			}
			printf("  Block %3u: %s\n", block, block_name(x[i]).c_str());
			if (block >= num_blocks) {
				if (!fail_once)
					fail("Invalid block number %u.\n", block);
				fail_once = true;
			} else if (x[i] != edid[block * EDID_PAGE_SIZE]) {
				fail("Block %u tag mismatch: expected 0x%02x, but got 0x%02x.\n",
				     block, edid[block * EDID_PAGE_SIZE], x[i]);
			}
		} else if (block < num_blocks) {
			fail("Block %u tag mismatch: expected 0x%02x, but got 0x00.\n",
			     block, edid[block * EDID_PAGE_SIZE]);
		}
	}
}

void edid_state::preparse_extension(unsigned char *x)
{
	switch (x[0]) {
	case 0x02:
		has_cta = true;
		preparse_cta_block(x);
		break;
	case 0x50:
		preparse_ls_ext_block(x);
		break;
	case 0x70:
		has_dispid = true;
		preparse_displayid_block(x);
		break;
	}
}

void edid_state::parse_extension(const unsigned char *x)
{
	block = block_name(x[0]);
	data_block.clear();
	unused_bytes = 0;

	printf("\n");
	if (block_nr && x[0] == 0)
		block = "Unknown EDID Extension Block 0x00";
	printf("Block %u, %s:\n", block_nr, block.c_str());

	switch (x[0]) {
	case 0x02:
		parse_cta_block(x);
		break;
	case 0x10:
		parse_vtb_ext_block(x);
		break;
	case 0x20:
		fail("Deprecated extension block for EDID 2.0, do not use.\n");
		break;
	case 0x40:
		parse_di_ext_block(x);
		break;
	case 0x50:
		parse_ls_ext_block(x);
		break;
	case 0x70:
		parse_displayid_block(x);
		break;
	case 0xf0:
		parse_block_map(x);
		if (block_nr != 1 && block_nr != 128)
			fail("Must be used in block 1 and 128.\n");
		break;
	default:
		hex_block("  ", x, EDID_PAGE_SIZE);
		fail("Unknown Extension Block.\n");
		break;
	}

	data_block.clear();
	do_checksum("", x, EDID_PAGE_SIZE, EDID_PAGE_SIZE - 1, unused_bytes);
}

void edid_state::print_preferred_timings()
{
	if (base.preferred_timing.is_valid()) {
		printf("\n----------------\n");
		printf("\nPreferred Video Timing if only Block 0 is parsed:\n");
		print_timings("  ", base.preferred_timing, true, false);
	}

	if (!cta.preferred_timings.empty()) {
		printf("\n----------------\n");
		printf("\nPreferred Video Timing%s if Block 0 and CTA-861 Blocks are parsed:\n",
		       cta.preferred_timings.size() > 1 ? "s" : "");
		for (vec_timings_ext::iterator iter = cta.preferred_timings.begin();
		     iter != cta.preferred_timings.end(); ++iter)
			print_timings("  ", *iter, true, false);
	}

	if (!cta.preferred_timings_vfpdb.empty()) {
		printf("\n----------------\n");
		printf("\nPreferred Video Timing%s if Block 0 and CTA-861 Blocks are parsed with VFPDB support:\n",
		       cta.preferred_timings_vfpdb.size() > 1 ? "s" : "");
		for (vec_timings_ext::iterator iter = cta.preferred_timings_vfpdb.begin();
		     iter != cta.preferred_timings_vfpdb.end(); ++iter)
			print_timings("  ", *iter, true, false);
	}

	if (!dispid.preferred_timings.empty()) {
		printf("\n----------------\n");
		printf("\nPreferred Video Timing%s if Block 0 and DisplayID Blocks are parsed:\n",
		       dispid.preferred_timings.size() > 1 ? "s" : "");
		for (vec_timings_ext::iterator iter = dispid.preferred_timings.begin();
		     iter != dispid.preferred_timings.end(); ++iter)
			print_timings("  ", *iter, true, false);
	}
}

void edid_state::print_native_res()
{
	typedef std::pair<unsigned, unsigned> resolution;
	typedef std::set<resolution> resolution_set;
	resolution_set native_prog, native_int, native_nvrdb;
	unsigned native_width = 0, native_height = 0;
	unsigned native_width_int = 0, native_height_int = 0;

	// Note: it is also a mismatch if Block 0 does not define a
	// native resolution, but other blocks do.
	bool native_mismatch = false;
	bool native_int_mismatch = false;

	if (base.preferred_timing.is_valid() && base.preferred_is_also_native) {
		if (base.preferred_timing.t.interlaced) {
			native_width_int = base.preferred_timing.t.hact;
			native_height_int = base.preferred_timing.t.vact;
		} else {
			native_width = base.preferred_timing.t.hact;
			native_height = base.preferred_timing.t.vact;
		}
	}

	if (!native_width && dispid.native_width) {
		native_width = dispid.native_width;
		native_height = dispid.native_height;
		native_mismatch = true;
	} else if (dispid.native_width && native_width &&
		   (dispid.native_width != native_width ||
		    dispid.native_height != native_height)) {
		native_mismatch = true;
	}

	for (vec_timings_ext::iterator iter = cta.native_timings.begin();
	     iter != cta.native_timings.end(); ++iter) {
		if (iter->t.interlaced) {
			native_int.insert(std::pair<unsigned, unsigned>(iter->t.hact, iter->t.vact));
			if (!native_width_int) {
				native_width_int = iter->t.hact;
				native_height_int = iter->t.vact;
				native_int_mismatch = true;
			} else if (native_width_int &&
				   (iter->t.hact != native_width_int ||
				    iter->t.vact != native_height_int)) {
				native_int_mismatch = true;
			}
		} else {
			native_prog.insert(std::pair<unsigned, unsigned>(iter->t.hact, iter->t.vact));
			if (!native_width) {
				native_width = iter->t.hact;
				native_height = iter->t.vact;
				native_mismatch = true;
			} else if (native_width &&
				   (iter->t.hact != native_width ||
				    iter->t.vact != native_height)) {
				native_mismatch = true;
			}
		}
	}

	for (vec_timings_ext::iterator iter = cta.native_timing_nvrdb.begin();
	     iter != cta.native_timing_nvrdb.end(); ++iter) {
		if (iter->t.interlaced) {
			fail("Interlaced native timing in NVRDB.\n");
		} else {
			native_nvrdb.insert(std::pair<unsigned, unsigned>(iter->t.hact, iter->t.vact));
			if (!native_width) {
				native_width = iter->t.hact;
				native_height = iter->t.vact;
				native_mismatch = true;
			} else if (native_width &&
				   (iter->t.hact != native_width ||
				    iter->t.vact != native_height)) {
				if (iter->t.hact >= 4096 || iter->t.vact >= 4096) {
					native_width = iter->t.hact;
					native_height = iter->t.vact;
				}
				native_mismatch = true;
			}
		}
	}

	if (diagonal) {
		if (image_width) {
			double w = image_width;
			double h = image_height;
			double d = sqrt(w * w + h * h) / 254.0;

			if (fabs(diagonal - d) >= 0.1) {
				warn("Specified diagonal is %.1f\", calculated diagonal is %.1f\" for EDID image size %.1fx%.1fmm.\n",
				     diagonal, d, w / 10.0, h / 10.0);
			}
		}
		if (native_width) {
			double w = native_width;
			double h = native_height;
			double d = diagonal * 254.0;
			double c = sqrt((d * d) / (w * w + h * h));

			w *= c;
			h *= c;

			if (image_width) {
				printf("\n----------------\n");
				printf("\nCalculated image size for a diagonal of %.1f\" is %.1fx%.1fmm (native resolution %ux%u).\n",
				     diagonal, w / 10.0, h / 10.0, native_width, native_height);

				if (fabs((double)image_width - w) >= 100.0 ||
				    fabs((double)image_height - h) >= 100.0)
					warn("Calculated image size is %.1fx%.1fmm, EDID image size is %.1fx%.1fmm.\n",
					     w / 10.0, h / 10.0,
					     image_width / 10.0, image_height / 10.0);
			} else {
				warn("No image size was specified, but it is calculated as %.1fx%.1fmm.\n",
				     w / 10.0, h / 10.0);
			}
			if (has_cta && !cta.nvrdb_has_size && (w > 25500 || h > 25500))
				warn("Calculated image width or height > 255 cm, recommend including an NVRDB with image size.\n");
		}
	}

	if (!base.is_analog && native_width == 0 && native_width_int == 0) {
		warn("No Native Video Resolution was defined.\n");
		// See also section 7.2.2 in the CTA-861-I (or later) standard:
		if (has_cta)
			warn("  Hint: set 'Native detailed modes' to a non-0 value, or add a Native Video Resolution Data Block.\n");
	}

	if (!options[OptNativeResolution])
		return;

	if (native_width == 0 && native_width_int == 0) {
		printf("\n----------------\n");
		printf("\nNo Native Video Resolution was defined.\n");
		return;
	}

	if ((native_width || native_width_int) &&
	    !native_mismatch && !native_int_mismatch) {
		printf("\n----------------\n");
		printf("\nNative Video Resolution%s:\n",
		       native_width && native_width_int ? "s" : "");
		if (native_width)
			printf("  %ux%u\n", native_width, native_height);
		if (native_width_int)
			printf("  %ux%ui\n", native_width_int, native_height_int);
		return;
	}

	if (base.preferred_timing.is_valid() && base.preferred_is_also_native) {
		printf("\n----------------\n");
		printf("\nNative Video Resolution if only Block 0 is parsed:\n");
		printf("  %ux%u%s\n",
		       base.preferred_timing.t.hact, base.preferred_timing.t.vact,
		       base.preferred_timing.t.interlaced ? "i" : "");
	}

	if (!cta.native_timings.empty()) {
		printf("\n----------------\n");
		printf("\nNative Video Resolution%s if Block 0 and CTA-861 Blocks are parsed:\n",
		       native_prog.size() + native_int.size() > 1 ? "s" : "");
		for (resolution_set::iterator iter = native_prog.begin();
		     iter != native_prog.end(); ++iter)
			printf("  %ux%u\n", iter->first, iter->second);
		for (resolution_set::iterator iter = native_int.begin();
		     iter != native_int.end(); ++iter)
			printf("  %ux%ui\n", iter->first, iter->second);
	}

	if (!cta.native_timing_nvrdb.empty()) {
		printf("\n----------------\n");
		printf("\nNative Video Resolution if Block 0 and CTA-861 Blocks are parsed with NVRDB support:\n");
		for (resolution_set::iterator iter = native_nvrdb.begin();
		     iter != native_nvrdb.end(); ++iter)
			printf("  %ux%u\n", iter->first, iter->second);
	}

	if (dispid.native_width) {
		printf("\n----------------\n");
		printf("\nNative Video Resolution if the DisplayID Blocks are parsed:\n");
		printf("  %ux%u\n", dispid.native_width, dispid.native_height);
	}
}

int edid_state::parse_edid()
{
	hide_serial_numbers = options[OptHideSerialNumbers];
	replace_unique_ids = options[OptReplaceUniqueIDs];

	to_utf8 = options[OptUTF8];

	preparse_base_block(edid);
	if (replace_unique_ids)
		replace_checksum(edid, EDID_PAGE_SIZE);

	for (unsigned i = 1; i < num_blocks; i++)
		preparse_extension(edid + i * EDID_PAGE_SIZE);

	if (options[OptPhysicalAddress]) {
		printf("%x.%x.%x.%x\n",
		       (cta.preparsed_phys_addr >> 12) & 0xf,
		       (cta.preparsed_phys_addr >> 8) & 0xf,
		       (cta.preparsed_phys_addr >> 4) & 0xf,
		       cta.preparsed_phys_addr & 0xf);
		return 0;
	}

	if (!options[OptSkipHexDump]) {
		printf("edid-decode (hex):\n\n");
		for (unsigned i = 0; i < num_blocks; i++) {
			hex_block("", edid + i * EDID_PAGE_SIZE, EDID_PAGE_SIZE, false);
			if (i == num_blocks - 1 && options[OptOnlyHexDump])
				return 0;
			printf("\n");
		}
		printf("----------------\n\n");
		if (diagonal) {
			printf("Diagonal: %.1f\"\n\n", diagonal);
			printf("----------------\n\n");
		}
	}

	block = block_name(0x00);
	printf("Block %u, %s:\n", block_nr, block.c_str());
	parse_base_block(edid);

	for (unsigned i = 1; i < num_blocks; i++) {
		block_nr++;
		printf("\n----------------\n");
		parse_extension(edid + i * EDID_PAGE_SIZE);
	}

	block = "";
	block_nr = EDID_MAX_BLOCKS;

	if (cta.has_svrs)
		cta_resolve_svrs();

	if (options[OptPreferredTimings])
		print_preferred_timings();

	print_native_res();

	if (!options[OptCheck] && !options[OptCheckInline])
		return 0;

	check_base_block(edid);
	if (has_cta)
		check_cta_blocks();
	if (has_dispid)
		check_displayid_blocks();

	printf("\n----------------\n");

	if (!options[OptSkipSHA] && strlen(STRING(SHA))) {
		options[OptSkipSHA] = 1;
		printf("\n");
		print_version();
	}

	if (options[OptCheck]) {
		if (warnings)
			show_msgs(true);
		if (failures)
			show_msgs(false);
	}
	printf("\nEDID conformity: %s\n", failures ? "FAIL" : "PASS");
	return failures ? -2 : 0;
}

/* InfoFrame parsing */

static unsigned char infoframe[32];
static unsigned if_size;

static unsigned char eld[128];
static unsigned eld_size;

static bool if_add_byte(const char *s)
{
	char buf[3];

	if (if_size == sizeof(infoframe))
		return false;
	buf[0] = s[0];
	buf[1] = s[1];
	buf[2] = 0;
	infoframe[if_size++] = strtoul(buf, NULL, 16);
	return true;
}

static bool extract_if_hex(const char *s)
{
	for (; *s; s++) {
		if (isspace(*s) || strchr(ignore_chars, *s))
			continue;

		/* Read one or two hex digits from the log */
		if (!isxdigit(s[0]))
			break;

		if (!isxdigit(s[1])) {
			odd_hex_digits = true;
			return false;
		}
		if (!if_add_byte(s))
			return false;
		s++;
	}
	return if_size;
}

static bool extract_if(int fd)
{
	std::vector<char> if_data;
	char buf[128];

	for (;;) {
		ssize_t i = read(fd, buf, sizeof(buf));

		if (i < 0)
			return false;
		if (i == 0)
			break;
		if_data.insert(if_data.end(), buf, buf + i);
	}

	if (if_data.empty()) {
		if_size = 0;
		return false;
	}
	// Ensure it is safely terminated by a 0 char
	if_data.push_back('\0');

	const char *data = &if_data[0];
	const char *start;

	/* Look for edid-decode output */
	start = strstr(data, "edid-decode InfoFrame (hex):");
	if (start)
		return extract_if_hex(strchr(start, ':') + 1);

	unsigned i;

	for (i = 0; i < 32 && (isspace(data[i]) || strchr(ignore_chars, data[i]) ||
			       tolower(data[i]) == 'x' || isxdigit(data[i])); i++);

	if (i == 32)
		return extract_if_hex(data);

	// Drop the extra '\0' byte since we now assume binary data
	if_data.pop_back();

	if_size = if_data.size();

	/* Assume binary */
	if (if_size > sizeof(infoframe)) {
		fprintf(stderr, "Binary InfoFrame length %u is greater than %zu.\n",
			if_size, sizeof(infoframe));
		return false;
	}
	memcpy(infoframe, data, if_size);
	return true;
}

static int if_from_file(const char *from_file)
{
#ifdef O_BINARY
	// Windows compatibility
	int flags = O_RDONLY | O_BINARY;
#else
	int flags = O_RDONLY;
#endif
	int fd;

	memset(infoframe, 0, sizeof(infoframe));
	if_size = 0;

	if (!strcmp(from_file, "-")) {
		from_file = "stdin";
		fd = 0;
	} else if ((fd = open(from_file, flags)) == -1) {
		perror(from_file);
		return -1;
	}

	odd_hex_digits = false;
	if (!extract_if(fd)) {
		if (!if_size) {
			fprintf(stderr, "InfoFrame of '%s' was empty.\n", from_file);
			return -1;
		}
		fprintf(stderr, "InfoFrame extraction of '%s' failed: ", from_file);
		if (odd_hex_digits)
			fprintf(stderr, "odd number of hexadecimal digits.\n");
		else
			fprintf(stderr, "unknown format.\n");
		return -1;
	}
	close(fd);

	return 0;
}

static void show_if_msgs(bool is_warn)
{
	printf("\n%s:\n\n", is_warn ? "Warnings" : "Failures");
	if (s_msgs[0][is_warn].empty())
		return;
	printf("InfoFrame:\n%s",
	       s_msgs[0][is_warn].c_str());
}

static bool eld_add_byte(const char *s)
{
	char buf[3];

	if (eld_size == sizeof(eld))
		return false;
	buf[0] = s[0];
	buf[1] = s[1];
	buf[2] = 0;
	eld[eld_size++] = strtoul(buf, NULL, 16);
	return true;
}

static bool extract_eld_hex(const char *s)
{
	for (; *s; s++) {
		if (isspace(*s) || strchr(ignore_chars, *s))
			continue;

		if (*s == '0' && tolower(s[1]) == 'x') {
			s++;
			continue;
		}

		/* Read one or two hex digits from the log */
		if (!isxdigit(s[0]))
			break;

		if (!isxdigit(s[1])) {
			odd_hex_digits = true;
			return false;
		}
		if (!eld_add_byte(s))
			return false;
		s++;
	}
	return eld_size;
}

static bool extract_eld(int fd)
{
	std::vector<char> eld_data;
	char buf[128];

	for (;;) {
		ssize_t i = read(fd, buf, sizeof(buf));

		if (i < 0)
			return false;
		if (i == 0)
			break;
		eld_data.insert(eld_data.end(), buf, buf + i);
	}

	if (eld_data.empty()) {
		eld_size = 0;
		return false;
	}
	// Ensure it is safely terminated by a 0 char
	eld_data.push_back('\0');

	const char *data = &eld_data[0];
	const char *start;

	/* Look for edid-decode output */
	start = strstr(data, "edid-decode ELD (hex):");
	if (start)
		return extract_eld_hex(strchr(start, ':') + 1);

	unsigned i;

	/* Is the EDID provided in hex? */
	for (i = 0; i < 32 && (isspace(data[i]) || strchr(ignore_chars, data[i]) ||
			       tolower(data[i]) == 'x' || isxdigit(data[i])); i++);

	if (i == 32)
		return extract_eld_hex(data);

	// Drop the extra '\0' byte since we now assume binary data
	eld_data.pop_back();

	eld_size = eld_data.size();

	/* Assume binary */
	if (eld_size > sizeof(eld)) {
		fprintf(stderr, "Binary ELD length %u is greater than %zu.\n",
			eld_size, sizeof(eld));
		return false;
	}
	memcpy(eld, data, eld_size);
	return true;
}

static int eld_from_file(const char *from_file)
{
#ifdef O_BINARY
	// Windows compatibility
	int flags = O_RDONLY | O_BINARY;
#else
	int flags = O_RDONLY;
#endif
	int fd;

	memset(eld, 0, sizeof(eld));
	eld_size = 0;

	if (!strcmp(from_file, "-")) {
		from_file = "stdin";
		fd = 0;
	} else if ((fd = open(from_file, flags)) == -1) {
		perror(from_file);
		return -1;
	}

	odd_hex_digits = false;
	if (!extract_eld(fd)) {
		if (!eld_size) {
			fprintf(stderr, "ELD of '%s' was empty.\n", from_file);
			return -1;
		}
		fprintf(stderr, "ELD extraction of '%s' failed: ", from_file);
		if (odd_hex_digits)
			fprintf(stderr, "odd number of hexadecimal digits.\n");
		else
			fprintf(stderr, "unknown format.\n");
		return -1;
	}
	close(fd);

	return 0;
}

static void show_eld_msgs(bool is_warn)
{
	printf("\n%s:\n\n", is_warn ? "Warnings" : "Failures");
	if (s_msgs[0][is_warn].empty())
		return;
	printf("ELD:\n%s",
	       s_msgs[0][is_warn].c_str());
}

int edid_state::parse_eld(const std::string &fname)
{
	int ret = eld_from_file(fname.c_str());
	unsigned int min_size = 4;
	unsigned baseline_size;
	unsigned char ver;

	if (ret)
		return ret;

	if (!options[OptSkipHexDump]) {
		printf("edid-decode ELD (hex):\n\n");
		hex_block("", eld, eld_size, false);
		if (options[OptOnlyHexDump])
			return 0;
		printf("\n----------------\n\n");
	}

	if (eld_size < min_size) {
		fail("ELD is too small to parse.\n");
		return -1;
	}

	ver = eld[0] >> 3;
	switch (ver) {
	case 1:
		warn("Obsolete Baseline ELD version (%d)\n", ver);
		break;
	case 2:
		printf("Baseline ELD version: 861.D or below\n");
		break;
	default:
		warn("Unsupported ELD version (%d)\n", ver);
		break;
	}

	baseline_size = eld[2] * 4;
	if (baseline_size > 80)
		warn("ELD too big\n");

	parse_eld_baseline(&eld[4], baseline_size);

	if (!options[OptCheck] && !options[OptCheckInline])
		return 0;

	printf("\n----------------\n");

	if (!options[OptSkipSHA] && strlen(STRING(SHA))) {
		options[OptSkipSHA] = 1;
		printf("\n");
		print_version();
	}

	if (options[OptCheck]) {
		if (warnings)
			show_eld_msgs(true);
		if (failures)
			show_eld_msgs(false);
	}

	printf("\n%s conformity: %s\n",
	       state.data_block.empty() ? "ELD" : state.data_block.c_str(),
	       failures ? "FAIL" : "PASS");
	return failures ? -2 : 0;
}
int edid_state::parse_if(const std::string &fname)
{
	int ret = if_from_file(fname.c_str());
	unsigned min_size = 4;
	bool is_hdmi = false;

	if (ret)
		return ret;

	state.block_nr = 0;
	state.data_block.clear();

	if (!options[OptSkipHexDump]) {
		printf("edid-decode InfoFrame (hex):\n\n");
		hex_block("", infoframe, if_size, false);
		if (options[OptOnlyHexDump])
			return 0;
		printf("\n----------------\n\n");
	}

	if (infoframe[0] >= 0x80) {
		is_hdmi = true;
		min_size++;
	}

	if (if_size < min_size) {
		fail("InfoFrame is too small to parse.\n");
		return -1;
	}

	if (is_hdmi) {
		do_checksum("HDMI InfoFrame ", infoframe, if_size, 3);
		printf("\n");
		memcpy(infoframe + 3, infoframe + 4, if_size - 4);
		infoframe[0] &= 0x7f;
		if_size--;
	}

	switch (infoframe[0]) {
	case 0x01:
		parse_if_vendor(infoframe, if_size);
		break;
	case 0x02:
		parse_if_avi(infoframe, if_size);
		break;
	case 0x03:
		parse_if_spd(infoframe, if_size);
		break;
	case 0x04:
		parse_if_audio(infoframe, if_size);
		break;
	case 0x05:
		parse_if_mpeg_source(infoframe, if_size);
		break;
	case 0x06:
		parse_if_ntsc_vbi(infoframe, if_size);
		break;
	case 0x07:
		parse_if_drm(infoframe, if_size);
		break;
	default:
		if (infoframe[0] <= 0x1f)
			fail("Reserved InfoFrame type %hhx.\n", infoframe[0]);
		else
			fail("Forbidden InfoFrame type %hhx.\n", infoframe[0]);
		break;
	}

	if (!options[OptCheck] && !options[OptCheckInline])
		return 0;

	printf("\n----------------\n");

	if (!options[OptSkipSHA] && strlen(STRING(SHA))) {
		options[OptSkipSHA] = 1;
		printf("\n");
		print_version();
	}

	if (options[OptCheck]) {
		if (warnings)
			show_if_msgs(true);
		if (failures)
			show_if_msgs(false);
	}

	printf("\n%s conformity: %s\n",
	       state.data_block.empty() ? "InfoFrame" : state.data_block.c_str(),
	       failures ? "FAIL" : "PASS");
	return failures ? -2 : 0;
}

#ifndef __EMSCRIPTEN__

static unsigned char crc_calc(const unsigned char *b)
{
	unsigned char sum = 0;
	unsigned i;

	for (i = 0; i < 127; i++)
		sum += b[i];
	return 256 - sum;
}

static int crc_ok(const unsigned char *b)
{
	return crc_calc(b) == b[127];
}

static void hexdumpedid(FILE *f, const unsigned char *edid, unsigned size)
{
	unsigned b, i, j;

	for (b = 0; b < size / 128; b++) {
		const unsigned char *buf = edid + 128 * b;

		if (b)
			fprintf(f, "\n");
		for (i = 0; i < 128; i += 0x10) {
			fprintf(f, "%02x", buf[i]);
			for (j = 1; j < 0x10; j++) {
				fprintf(f, " %02x", buf[i + j]);
			}
			fprintf(f, "\n");
		}
		if (!crc_ok(buf))
			fprintf(f, "Block %u has a checksum error (should be 0x%02x).\n",
				b, crc_calc(buf));
	}
}

static void carraydumpedid(FILE *f, const unsigned char *edid, unsigned size)
{
	unsigned b, i, j;

	fprintf(f, "const unsigned char edid[] = {\n");
	for (b = 0; b < size / 128; b++) {
		const unsigned char *buf = edid + 128 * b;

		if (b)
			fprintf(f, "\n");
		for (i = 0; i < 128; i += 8) {
			fprintf(f, "\t0x%02x,", buf[i]);
			for (j = 1; j < 8; j++) {
				fprintf(f, " 0x%02x,", buf[i + j]);
			}
			fprintf(f, "\n");
		}
		if (!crc_ok(buf))
			fprintf(f, "\t/* Block %u has a checksum error (should be 0x%02x). */\n",
				b, crc_calc(buf));
	}
	fprintf(f, "};\n");
}

// This format can be read by the QuantumData EDID editor
static void xmldumpedid(FILE *f, const unsigned char *edid, unsigned size)
{
	fprintf(f, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>\n");
	fprintf(f, "<DATAOBJ>\n");
	fprintf(f, "    <HEADER TYPE=\"DID\" VERSION=\"1.0\"/>\n");
	fprintf(f, "    <DATA>\n");
	for (unsigned b = 0; b < size / 128; b++) {
		const unsigned char *buf = edid + 128 * b;

		fprintf(f, "        <BLOCK%u>", b);
		for (unsigned i = 0; i < 128; i++)
			fprintf(f, "%02X", buf[i]);
		fprintf(f, "</BLOCK%u>\n", b);
	}
	fprintf(f, "    </DATA>\n");
	fprintf(f, "</DATAOBJ>\n");
}

static int edid_to_file(const char *to_file, enum output_format out_fmt)
{
	FILE *out;

	if (!strcmp(to_file, "-")) {
		to_file = "stdout";
		out = stdout;
	} else if ((out = fopen(to_file, "w")) == NULL) {
		perror(to_file);
		return -1;
	}
	if (out_fmt == OUT_FMT_DEFAULT)
		out_fmt = out == stdout ? OUT_FMT_HEX : OUT_FMT_RAW;

	switch (out_fmt) {
	default:
	case OUT_FMT_HEX:
		hexdumpedid(out, edid, state.edid_size);
		break;
	case OUT_FMT_RAW:
		fwrite(edid, state.edid_size, 1, out);
		break;
	case OUT_FMT_CARRAY:
		carraydumpedid(out, edid, state.edid_size);
		break;
	case OUT_FMT_XML:
		xmldumpedid(out, edid, state.edid_size);
		break;
	}

	if (out != stdout)
		fclose(out);
	return 0;
}

enum cvt_opts {
	CVT_WIDTH = 0,
	CVT_HEIGHT,
	CVT_FPS,
	CVT_INTERLACED,
	CVT_OVERSCAN,
	CVT_RB,
	CVT_ALT,
	CVT_RB_H_BLANK,
	CVT_RB_V_BLANK,
	CVT_EARLY_VSYNC,
};

static int parse_cvt_subopt(char **subopt_str, double *value)
{
	int opt;
	char *opt_str;

	static const char * const subopt_list[] = {
		"w",
		"h",
		"fps",
		"interlaced",
		"overscan",
		"rb",
		"alt",
		"hblank",
		"vblank",
		"early-vsync",
		nullptr
	};

	opt = getsubopt(subopt_str, (char * const *)subopt_list, &opt_str);

	if (opt_str == nullptr && opt != CVT_INTERLACED && opt != CVT_ALT &&
	    opt != CVT_OVERSCAN && opt != CVT_EARLY_VSYNC) {
		fprintf(stderr, "No value given to suboption <%s>.\n",
				subopt_list[opt]);
		usage();
		std::exit(EXIT_FAILURE);
	}

	if (opt_str)
		*value = strtod(opt_str, nullptr);
	else
		*value = 0;
	return opt;
}

static void parse_cvt(char *optarg)
{
	unsigned w = 0, h = 0;
	double fps = 0;
	unsigned rb = RB_NONE;
	unsigned rb_h_blank = 0;
	unsigned rb_v_blank = 460;
	bool interlaced = false;
	bool alt = false;
	bool overscan = false;
	bool early_vsync = false;

	while (*optarg != '\0') {
		int opt;
		double opt_val;

		opt = parse_cvt_subopt(&optarg, &opt_val);

		switch (opt) {
		case CVT_WIDTH:
			w = round(opt_val);
			break;
		case CVT_HEIGHT:
			h = round(opt_val);
			break;
		case CVT_FPS:
			fps = opt_val;
			break;
		case CVT_RB:
			rb = opt_val;
			break;
		case CVT_OVERSCAN:
			overscan = true;
			break;
		case CVT_INTERLACED:
			interlaced = opt_val;
			break;
		case CVT_ALT:
			alt = opt_val;
			break;
		case CVT_RB_H_BLANK:
			rb_h_blank = opt_val;
			break;
		case CVT_RB_V_BLANK:
			rb_v_blank = opt_val;
			if (rb_v_blank < 460) {
				fprintf(stderr, "vblank must be >= 460, set to 460.\n");
				rb_v_blank = 460;
			} else if (rb_v_blank > 705) {
				fprintf(stderr, "warning: vblank values > 705 might not be supported by RBv3 compliant sources.\n");
			}
			break;
		case CVT_EARLY_VSYNC:
			early_vsync = true;
			break;
		default:
			break;
		}
	}

	if (!w || !h || !fps) {
		fprintf(stderr, "Missing width, height and/or fps.\n");
		usage();
		std::exit(EXIT_FAILURE);
	}
	if (interlaced)
		fps /= 2;
	timings t = state.calc_cvt_mode(w, h, fps, rb, interlaced, overscan, alt,
					rb_h_blank, rb_v_blank, early_vsync);
	state.print_timings("", &t, "CVT", "", true, false);
}

struct gtf_parsed_data {
	unsigned w, h;
	double freq;
	double C, M, K, J;
	bool overscan;
	bool interlaced;
	bool secondary;
	bool params_from_edid;
	enum gtf_ip_parm ip_parm;
};

enum gtf_opts {
	GTF_WIDTH = 0,
	GTF_HEIGHT,
	GTF_FPS,
	GTF_HORFREQ,
	GTF_PIXCLK,
	GTF_INTERLACED,
	GTF_OVERSCAN,
	GTF_SECONDARY,
	GTF_C2,
	GTF_M,
	GTF_K,
	GTF_J2,
};

static int parse_gtf_subopt(char **subopt_str, double *value)
{
	int opt;
	char *opt_str;

	static const char * const subopt_list[] = {
		"w",
		"h",
		"fps",
		"horfreq",
		"pixclk",
		"interlaced",
		"overscan",
		"secondary",
		"C",
		"M",
		"K",
		"J",
		nullptr
	};

	opt = getsubopt(subopt_str, (char * const *)subopt_list, &opt_str);

	if (opt == -1) {
		fprintf(stderr, "Invalid suboptions specified.\n");
		usage();
		std::exit(EXIT_FAILURE);
	}
	if (opt_str == nullptr && opt != GTF_INTERLACED && opt != GTF_OVERSCAN &&
	    opt != GTF_SECONDARY) {
		fprintf(stderr, "No value given to suboption <%s>.\n",
				subopt_list[opt]);
		usage();
		std::exit(EXIT_FAILURE);
	}

	if (opt == GTF_C2 || opt == GTF_J2)
		*value = round(2.0 * strtod(opt_str, nullptr));
	else if (opt_str)
		*value = strtod(opt_str, nullptr);
	else
		*value = 0;
	return opt;
}

static void parse_gtf(char *optarg, gtf_parsed_data &data)
{
	memset(&data, 0, sizeof(data));
	data.params_from_edid = true;
	data.C = 40;
	data.M = 600;
	data.K = 128;
	data.J = 20;

	while (*optarg != '\0') {
		int opt;
		double opt_val;

		opt = parse_gtf_subopt(&optarg, &opt_val);

		switch (opt) {
		case GTF_WIDTH:
			data.w = round(opt_val);
			break;
		case GTF_HEIGHT:
			data.h = round(opt_val);
			break;
		case GTF_FPS:
			data.freq = opt_val;
			data.ip_parm = gtf_ip_vert_freq;
			break;
		case GTF_HORFREQ:
			data.freq = opt_val;
			data.ip_parm = gtf_ip_hor_freq;
			break;
		case GTF_PIXCLK:
			data.freq = opt_val;
			data.ip_parm = gtf_ip_clk_freq;
			break;
		case GTF_INTERLACED:
			data.interlaced = true;
			break;
		case GTF_OVERSCAN:
			data.overscan = true;
			break;
		case GTF_SECONDARY:
			data.secondary = true;
			break;
		case GTF_C2:
			data.C = opt_val / 2.0;
			data.params_from_edid = false;
			break;
		case GTF_M:
			data.M = round(opt_val);
			data.params_from_edid = false;
			break;
		case GTF_K:
			data.K = round(opt_val);
			data.params_from_edid = false;
			break;
		case GTF_J2:
			data.J = opt_val / 2.0;
			data.params_from_edid = false;
			break;
		default:
			break;
		}
	}

	if (!data.w || !data.h) {
		fprintf(stderr, "Missing width and/or height.\n");
		usage();
		std::exit(EXIT_FAILURE);
	}
	if (!data.freq) {
		fprintf(stderr, "One of fps, horfreq or pixclk must be given.\n");
		usage();
		std::exit(EXIT_FAILURE);
	}
	if (!data.secondary)
		data.params_from_edid = false;
	if (data.interlaced && data.ip_parm == gtf_ip_vert_freq)
		data.freq /= 2;
}

static void show_gtf(gtf_parsed_data &data)
{
	timings t;

	t = state.calc_gtf_mode(data.w, data.h, data.freq, data.interlaced,
				data.ip_parm, data.overscan, data.secondary,
				data.C, data.M, data.K, data.J);
	calc_ratio(&t);
	state.print_timings("", &t, "GTF", "", true, false);
}

enum ovt_opts {
	OVT_RID,
	OVT_WIDTH,
	OVT_HEIGHT,
	OVT_FPS,
};

static int parse_ovt_subopt(char **subopt_str, unsigned *value)
{
	int opt;
	char *opt_str;

	static const char * const subopt_list[] = {
		"rid",
		"w",
		"h",
		"fps",
		nullptr
	};

	opt = getsubopt(subopt_str, (char* const*) subopt_list, &opt_str);

	if (opt == -1) {
		fprintf(stderr, "Invalid suboptions specified.\n");
		usage();
		std::exit(EXIT_FAILURE);
	}
	if (opt_str == nullptr) {
		fprintf(stderr, "No value given to suboption <%s>.\n",
				subopt_list[opt]);
		usage();
		std::exit(EXIT_FAILURE);
	}

	if (opt_str)
		*value = strtoul(opt_str, NULL, 0);
	return opt;
}

static void parse_ovt(char *optarg)
{
	unsigned rid = 0;
	unsigned w = 0, h = 0;
	unsigned fps = 0;

	while (*optarg != '\0') {
		int opt;
		unsigned opt_val;

		opt = parse_ovt_subopt(&optarg, &opt_val);

		switch (opt) {
		case OVT_RID:
			rid = opt_val;
			break;
		case OVT_WIDTH:
			w = opt_val;
			break;
		case OVT_HEIGHT:
			h = opt_val;
			break;
		case OVT_FPS:
			fps = opt_val;
			break;
		default:
			break;
		}
	}

	if ((!rid && (!w || !h)) || !fps) {
		fprintf(stderr, "Missing rid, width, height and/or fps.\n");
		usage();
		std::exit(EXIT_FAILURE);
	}
	unsigned hratio = 0, vratio = 0;
	if (rid) {
		const cta_rid *r = find_rid(rid);

		if (r) {
			w = r->hact;
			h = r->vact;
			hratio = r->hratio;
			vratio = r->vratio;
		}
	}
	timings t = state.calc_ovt_mode(w, h, hratio, vratio, fps);
	state.print_timings("", &t, "OVT", "", true, false);
}

enum test_reliability_opts {
	REL_DURATION,
	REL_MSLEEP,
};

static int parse_test_reliability_subopt(char **subopt_str, unsigned *value)
{
	int opt;
	char *opt_str;

	static const char * const subopt_list[] = {
		"duration",
		"msleep",
		nullptr
	};

	opt = getsubopt(subopt_str, (char* const*) subopt_list, &opt_str);

	if (opt == -1) {
		fprintf(stderr, "Invalid suboptions specified.\n");
		usage();
		std::exit(EXIT_FAILURE);
	}
	if (opt_str == nullptr) {
		fprintf(stderr, "No value given to suboption <%s>.\n",
				subopt_list[opt]);
		usage();
		std::exit(EXIT_FAILURE);
	}

	if (opt_str)
		*value = strtoul(opt_str, NULL, 0);
	return opt;
}

static void parse_test_reliability(char *optarg, unsigned &duration, unsigned &msleep)
{
	while (*optarg != '\0') {
		int opt;
		unsigned opt_val;

		opt = parse_test_reliability_subopt(&optarg, &opt_val);

		switch (opt) {
		case REL_DURATION:
			duration = opt_val;
			break;
		case REL_MSLEEP:
			msleep = opt_val;
			break;
		default:
			break;
		}
	}
}

int main(int argc, char **argv)
{
	char short_options[26 * 2 * 3 + 1];
	enum output_format out_fmt = OUT_FMT_DEFAULT;
	gtf_parsed_data gtf_data;
	unsigned list_rid = 0;
	int adapter_fd = -1;
	double hdcp_ri_sleep = 0;
	std::vector<std::string> if_names;
	std::vector<std::string> eld_names;
	unsigned test_rel_duration = 0;
	unsigned test_rel_msleep = 50;
	unsigned idx = 0;
	unsigned i;
	int ret = 0;

	for (i = 0; long_options[i].name; i++) {
		if (!isalpha(long_options[i].val))
			continue;
		short_options[idx++] = long_options[i].val;
		if (long_options[i].has_arg == required_argument) {
			short_options[idx++] = ':';
		} else if (long_options[i].has_arg == optional_argument) {
			short_options[idx++] = ':';
			short_options[idx++] = ':';
		}
	}
	while (true) {
		int option_index = 0;
		unsigned val;
		const timings *t;
		char buf[16];
		int ch;

		short_options[idx] = 0;
		ch = getopt_long(argc, argv, short_options,
				 long_options, &option_index);
		if (ch == -1)
			break;

		options[ch] = 1;

		if (!option_index) {
			for (i = 0; long_options[i].val; i++) {
				if (long_options[i].val == ch) {
					option_index = i;
					break;
				}
			}
		}
		if (long_options[option_index].has_arg == optional_argument &&
		    !optarg && argv[optind] && argv[optind][0] != '-')
			optarg = argv[optind++];

		switch (ch) {
		case OptHelp:
			usage();
			return -1;
		case OptOutputFormat:
			if (!strcmp(optarg, "hex")) {
				out_fmt = OUT_FMT_HEX;
			} else if (!strcmp(optarg, "raw")) {
				out_fmt = OUT_FMT_RAW;
			} else if (!strcmp(optarg, "carray")) {
				out_fmt = OUT_FMT_CARRAY;
			} else if (!strcmp(optarg, "xml")) {
				out_fmt = OUT_FMT_XML;
			} else {
				usage();
				std::exit(EXIT_FAILURE);
			}
			break;
		case OptDiag: {
			double diag;

			diag = strtod(optarg, NULL);
			if (isnormal(diag)) {
				if (diag == 0 || (diag >= 10 && diag <= 2559))
					state.diagonal = diag;
				else
					fprintf(stderr, "Ignored diagonal, expected to be between 10 and 2559 inches.\n");
			}
			break;
		}
#ifdef __HAS_I2C_DEV__
		case OptI2CAdapter: {
			std::string device = optarg;

			if (device[0] >= '0' && device[0] <= '9' && device.length() <= 3) {
				static char newdev[20];

				sprintf(newdev, "/dev/i2c-%s", optarg);
				device = newdev;
			}

			adapter_fd = request_i2c_adapter(device.c_str());
			if (adapter_fd < 0)
				std::exit(EXIT_FAILURE);
			break;
		}
		case OptI2CTestReliability:
			if (optarg)
				parse_test_reliability(optarg, test_rel_duration, test_rel_msleep);
			break;
#endif
		case OptI2CHDCPRi:
			hdcp_ri_sleep = strtod(optarg, NULL);
			break;
		case OptSTD: {
			unsigned char byte1, byte2 = 0;
			char *endptr;

			byte1 = strtoul(optarg, &endptr, 0);
			if (*endptr == ',')
				byte2 = strtoul(endptr + 1, NULL, 0);
			state.print_standard_timing("", byte1, byte2, false, true);
			break;
		}
		case OptDMT:
			val = strtoul(optarg, NULL, 0);
			t = find_dmt_id(val);
			if (t) {
				sprintf(buf, "DMT 0x%02x", val);
				state.print_timings("", t, buf, "", true, false);
			} else {
				fprintf(stderr, "Unknown DMT code 0x%02x.\n", val);
			}
			break;
		case OptVIC:
			val = strtoul(optarg, NULL, 0);
			t = find_vic_id(val);
			if (t) {
				sprintf(buf, "VIC %3u", val);
				state.print_timings("", t, buf, "", true, false);
			} else {
				fprintf(stderr, "Unknown VIC code %u.\n", val);
			}
			break;
		case OptHDMIVIC:
			val = strtoul(optarg, NULL, 0);
			t = find_hdmi_vic_id(val);
			if (t) {
				sprintf(buf, "HDMI VIC %u", val);
				state.print_timings("", t, buf, "", true, false);
			} else {
				fprintf(stderr, "Unknown HDMI VIC code %u.\n", val);
			}
			break;
		case OptCVT:
			parse_cvt(optarg);
			break;
		case OptGTF:
			parse_gtf(optarg, gtf_data);
			break;
		case OptOVT:
			parse_ovt(optarg);
			break;
		case OptListRIDTimings:
			list_rid = strtoul(optarg, NULL, 0);
			break;
		case OptInfoFrame:
			if_names.push_back(optarg);
			break;
		case OptEld:
			eld_names.push_back(optarg);
			break;
		case ':':
			fprintf(stderr, "Option '%s' requires a value.\n",
				argv[optind]);
			usage();
			return -1;
		case '?':
			fprintf(stderr, "Unknown argument '%s'.\n",
				argv[optind]);
			usage();
			return -1;
		}
	}
	if (optind == argc && options[OptVersion]) {
		print_version();
		return 0;
	}

	if (options[OptListEstTimings])
		state.list_established_timings();
	if (options[OptListDMTs])
		state.list_dmts();
	if (options[OptListVICs])
		state.cta_list_vics();
	if (options[OptListHDMIVICs])
		state.cta_list_hdmi_vics();
	if (options[OptListRIDs])
		state.cta_list_rids();
	if (options[OptListRIDTimings])
		state.cta_list_rid_timings(list_rid);

	if (options[OptListEstTimings] || options[OptListDMTs] ||
	    options[OptListVICs] || options[OptListHDMIVICs] ||
	    options[OptListRIDs] || options[OptListRIDTimings])
		return 0;

	if (options[OptCVT] || options[OptDMT] || options[OptVIC] ||
	    options[OptHDMIVIC] || options[OptSTD] || options[OptOVT])
		return 0;

	if (options[OptGTF] && (!gtf_data.params_from_edid || optind == argc)) {
		show_gtf(gtf_data);
		return 0;
	}

	if (optind == argc) {
		if (adapter_fd >= 0 && options[OptI2CEDID]) {
			ret = read_edid(adapter_fd, edid, options[OptPhysicalAddress]);
			if (ret > 0) {
				state.edid_size = ret * EDID_PAGE_SIZE;
				state.num_blocks = ret;
				ret = 0;
			}
		} else if (adapter_fd >= 0) {
			if (options[OptI2CHDCP])
				ret = read_hdcp(adapter_fd);
			if (options[OptI2CHDCPRi])
				ret = read_hdcp_ri(adapter_fd, hdcp_ri_sleep);
			if (options[OptI2CTestReliability])
				ret = test_reliability(adapter_fd, test_rel_duration, test_rel_msleep);
		} else if ((options[OptInfoFrame] || options[OptEld]) && !options[OptGTF]) {
			ret = 0;
		} else {
			ret = edid_from_file("-", stdout);
		}
	} else {
		ret = edid_from_file(argv[optind], argv[optind + 1] ? stderr : stdout);
	}

	if (ret && options[OptPhysicalAddress]) {
		printf("f.f.f.f\n");
		return 0;
	}
	if (optind < argc - 1)
		return ret ? ret : edid_to_file(argv[optind + 1], out_fmt);

	if (options[OptGTF]) {
		timings t;

		state.preparse_base_block(edid);

		t = state.calc_gtf_mode(gtf_data.w, gtf_data.h, gtf_data.freq,
					gtf_data.interlaced, gtf_data.ip_parm,
					gtf_data.overscan);
		unsigned hbl = t.hfp + t.hsync + t.hbp;
		unsigned htotal = t.hact + hbl;
		double hor_freq_khz = htotal ? (double)t.pixclk_khz / htotal : 0;

		if (state.base.supports_sec_gtf &&
		    hor_freq_khz >= state.base.sec_gtf_start_freq) {
			t = state.calc_gtf_mode(gtf_data.w, gtf_data.h, gtf_data.freq,
						gtf_data.interlaced, gtf_data.ip_parm,
						gtf_data.overscan, true,
						state.base.C, state.base.M,
						state.base.K, state.base.J);
		}
		calc_ratio(&t);
		if (t.hfp <= 0)
			state.print_timings("", &t, "GTF", "INVALID: Hfront <= 0", true, false);
		else
			state.print_timings("", &t, "GTF", "", true, false);
		return 0;
	}

	if (!ret && state.edid_size)
		ret = state.parse_edid();

	bool show_line = state.edid_size;

	for (const auto &n : if_names) {
		if (show_line)
			printf("\n================\n\n");
		show_line = true;

		state.warnings = state.failures = 0;
		for (unsigned i = 0; i < EDID_MAX_BLOCKS + 1; i++) {
			s_msgs[i][0].clear();
			s_msgs[i][1].clear();
		}
		int r = state.parse_if(n);
		if (r && !ret)
			ret = r;
	}

	for (const auto &n : eld_names) {
		if (show_line)
			printf("\n================\n\n");
		show_line = true;

		state.warnings = state.failures = 0;
		for (unsigned i = 0; i < EDID_MAX_BLOCKS + 1; i++) {
			s_msgs[i][0].clear();
			s_msgs[i][1].clear();
		}
		int r = state.parse_eld(n);
		if (r && !ret)
			ret = r;
	}
	return ret;
}

#else

/*
 * The surrounding JavaScript implementation will call this function
 * each time it wants to decode an EDID. So this should reset all the
 * state and start over.
 */
extern "C" int parse_edid(const char *input, float diag)
{
	for (unsigned i = 0; i < EDID_MAX_BLOCKS + 1; i++) {
		s_msgs[i][0].clear();
		s_msgs[i][1].clear();
	}
	options[OptCheck] = 1;
	options[OptPreferredTimings] = 1;
	options[OptNativeResolution] = 1;
	options[OptSkipSHA] = 0;
	options[OptUTF8] = 1;
	state = edid_state();
	// Accept values
	if (isnormal(diag) && diag) {
	        if (diag >= 10 && diag <= 2559)
			state.diagonal = diag;
		else
			fprintf(stderr, "Ignored diagonal, expected to be between 10 and 2559 inches.\n");
	}
	int ret = edid_from_file(input, stderr);
	return ret ? ret : state.parse_edid();
}

#endif
