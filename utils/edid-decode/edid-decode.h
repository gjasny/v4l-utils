// SPDX-License-Identifier: MIT
/*
 * Copyright 2006-2012 Red Hat, Inc.
 * Copyright 2018-2020 Cisco Systems, Inc. and/or its affiliates. All rights reserved.
 *
 * Author: Adam Jackson <ajax@nwnk.net>
 * Maintainer: Hans Verkuil <hverkuil-cisco@xs4all.nl>
 */

#ifndef __EDID_DECODE_H_
#define __EDID_DECODE_H_

#include <string>
#include <vector>
#include <set>
#include <string.h>

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(*(x)))
#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))

#define EDID_PAGE_SIZE 128U
#define EDID_MAX_BLOCKS 256U

#define RB_ALT		(1U << 7)

#define RB_NONE		(0U)
#define RB_CVT_V1	(1U)
#define RB_CVT_V2	(2U)
#define RB_CVT_V3	(3U)
#define RB_GTF		(4U)

// Video Timings
// If interlaced is true, then the vertical blanking
// for each field is (vfp + vsync + vbp + 0.5), except for
// the VIC 39 timings that doesn't have the 0.5 constant.
//
// The sequence of the various video parameters is as follows:
//
// border - front porch - sync - back porch - border - active video
//
// Note: this is slightly different from EDID 1.4 which calls
// 'active video' as 'addressable video' and the EDID 1.4 term
// 'active video' includes the borders.
//
// But since borders are rarely used, the term 'active video' will
// typically be the same as 'addressable video', and that's how I
// use it.
struct timings {
	// Active horizontal and vertical frame height, excluding any
	// borders, if present.
	// Note: for interlaced formats the active field height is vact / 2
	unsigned hact, vact;
	unsigned hratio, vratio;
	unsigned pixclk_khz;
	// 0: no reduced blanking
	// 1: CVT reduced blanking version 1
	// 2: CVT reduced blanking version 2
	// 2 | RB_ALT: CVT reduced blanking version 2 video-optimized (1000/1001 fps)
	// 3: CVT reduced blanking version 3
	// 3 | RB_ALT: v3 with a horizontal blank of 160
	// 4: GTF Secondary Curve
	unsigned rb;
	bool interlaced;
	// The horizontal frontporch may be negative in GTF calculations,
	// so use int instead of unsigned for hfp. Example: 292x176@76.
	int hfp;
	unsigned hsync;
	// The backporch may be negative in buggy detailed timings.
	// So use int instead of unsigned for hbp and vbp.
	int hbp;
	bool pos_pol_hsync;
	// For interlaced formats the vertical front porch of the Even Field
	// is actually a half-line longer.
	unsigned vfp, vsync;
	// For interlaced formats the vertical back porch of the Odd Field
	// is actually a half-line longer.
	int vbp;
	bool pos_pol_vsync;
	unsigned hborder, vborder;
	bool even_vtotal; // special for VIC 39
	bool no_pol_vsync; // digital composite signals have no vsync polarity
	unsigned hsize_mm, vsize_mm;
	bool ycbcr420; // YCbCr 4:2:0 encoding
};

struct timings_ext {
	timings_ext()
	{
		memset(&t, 0, sizeof(t));
	}
	timings_ext(unsigned svr, const std::string &_type)
	{
		memset(&t, 0, sizeof(t));
		t.hact = svr;
		type = _type;
	}
	timings_ext(const timings &_t, const std::string &_type, const std::string &_flags)
	{
		t = _t;
		type = _type;
		flags = _flags;
	}

	bool is_valid() const { return t.hact; }
	bool has_svr() const { return t.hact && !t.vact; }
	unsigned svr() const { return t.hact; }
	timings t;
	std::string type;
	std::string flags;
};

enum gtf_ip_parm {
	gtf_ip_vert_freq = 1,
	gtf_ip_hor_freq,
	gtf_ip_clk_freq,
};

enum hdmi_image_size {
	hdmi_image_size_none,
	hdmi_image_size_ratio,
	hdmi_image_size_rounded,
	hdmi_image_size_5cm,
};

typedef std::vector<timings_ext> vec_timings_ext;

struct cta_rid {
	unsigned hact, vact;
	unsigned hratio, vratio;
};

struct cta_vfd {
	unsigned char rid;
	unsigned char fr_factor;
	unsigned int bfr50:1;
	unsigned int fr24:1;
	unsigned int bfr60:1;
	unsigned int fr144:1;
	unsigned int fr48:1;
};

struct edid_state {
	edid_state()
	{
		// Global state
		edid_size = num_blocks = block_nr = 0;
		max_hor_freq_hz = max_vert_freq_hz = max_pixclk_khz = 0;
		min_hor_freq_hz = 0xffffff;
		min_vert_freq_hz = 0xffffffff;
		dtd_max_vsize_mm = dtd_max_hsize_mm = 0;
		warnings = failures = 0;
		has_cta = has_dispid = false;
		// Note: for now we do not support native DisplayID data,
		// so this is always false. But some tests are different
		// depending on whether it is a native DisplayID structure
		// or an extension block, so they can use this bool.
		native_dispid = false;
		hide_serial_numbers = false;
		replace_unique_ids = false;
		image_width = image_height = diagonal = 0;
		serial_string_cnt = 0;
		serial_strings.clear();

		// Base block state
		base.edid_minor = 0;
		base.has_name_descriptor = base.has_display_range_descriptor =
			base.supports_continuous_freq = base.supports_gtf =
			base.supports_cvt = base.seen_non_detailed_descriptor =
			base.has_640x480p60_est_timing = base.has_spwg =
			base.preferred_is_also_native = false;
		base.serial_number = 0;
		base.week = base.year = 0;
		base.is_analog = false;
		base.supports_sec_gtf = false;
		base.sec_gtf_start_freq = 0;
		base.C = base.M = base.K = base.J = 0;
		base.max_pos_neg_hor_freq_khz = 0;
		base.uses_srgb = false;
		base.detailed_block_cnt = base.dtd_cnt = 0;

		base.min_display_hor_freq_hz = base.max_display_hor_freq_hz =
			base.min_display_vert_freq_hz = base.max_display_vert_freq_hz =
			base.max_display_pixclk_khz = base.max_display_width_mm =
			base.max_display_height_mm = 0;

		// CTA-861 block state
		cta.has_vic_1 = cta.first_svd_might_be_preferred = cta.has_sldb =
			cta.has_hdmi = cta.has_vcdb = cta.has_vfpdb = cta.has_cdb =
			cta.has_nvrdb = cta.has_pidb = false;
		cta.previous_cta_tag = 0xfff;
		cta.have_hf_vsdb = cta.have_hf_scdb = false;
		cta.hdmi_max_rate = 0;
		cta.hf_eeodb_blocks = 0;
		cta.image_width = cta.image_height = 0;
		cta.nvrdb_has_size = false;
		cta.block_number = 0;
		cta.has_svrs = false;
		cta.first_svd = true;
		cta.first_cta = true;
		cta.supported_hdmi_vic_codes = cta.supported_hdmi_vic_vsb_codes = 0;
		memset(cta.vics, 0, sizeof(cta.vics));
		memset(cta.preparsed_has_vic, 0, sizeof(cta.preparsed_has_vic));
		memset(&cta.preparsed_first_vfd, 0, sizeof(cta.preparsed_first_vfd));
		cta.preparsed_phys_addr = 0xffff;
		cta.preparsed_speaker_count = 0;
		cta.preparsed_sld = false;
		cta.preparsed_sld_has_coord = false;
		cta.preparsed_total_dtds = 0;
		cta.preparsed_total_vtdbs = 0;
		cta.preparsed_has_t8vtdb = false;
		cta.preparsed_t8vtdb_dmt = 0;
		cta.preparsed_max_vic_pixclk_khz = 0;
		cta.preparsed_image_size = hdmi_image_size_none;
		cta.warn_about_hdmi_2x_dtd = false;
		cta.avi_version = 2;
		cta.avi_v4_length = 14;
		cta.has_ycbcr444 = false;
		cta.has_ycbcr422 = false;
		cta.has_ycbcr420 = false;

		// DisplayID block state
		dispid.version = 0;
		dispid.native_width = dispid.native_height = 0;
		dispid.preparsed_color_ids = dispid.preparsed_xfer_ids = 0;
		dispid.preparsed_displayid_blocks = 0;
		dispid.is_base_block = true;
		dispid.is_display = dispid.has_product_identification =
			dispid.has_display_parameters = dispid.has_type_1_7 =
			dispid.has_display_interface_features =
			dispid.has_tiled_display_topology = dispid.has_ycbcr_420 =
			dispid.is_arvr = dispid.has_arvr_hdm = dispid.has_arvr_layer =
			dispid.has_stereo = dispid.has_stereo_display_interface = false;
		dispid.block_number = 0;
		dispid.image_width = dispid.image_height = 0;

		// Block Map block state
		block_map.saw_block_1 = false;
		block_map.saw_block_128 = false;
	}

	// Global state
	unsigned edid_size;
	unsigned num_blocks;
	unsigned block_nr;
	std::string block;
	std::string data_block;
	unsigned unused_bytes;
	bool has_cta;
	bool has_dispid;
	bool native_dispid;
	bool hide_serial_numbers;
	bool replace_unique_ids;
	std::vector<std::string> serial_strings;
	unsigned serial_string_cnt;

	unsigned min_hor_freq_hz;
	unsigned max_hor_freq_hz;
	double min_vert_freq_hz;
	double max_vert_freq_hz;
	unsigned max_pixclk_khz;
	unsigned dtd_max_hsize_mm;
	unsigned dtd_max_vsize_mm;

	// in 0.1 mm units
	unsigned image_width, image_height;
	// in inches
	double diagonal;

	unsigned warnings;
	unsigned failures;

	// Base block state
	struct {
		unsigned edid_minor;
		bool has_name_descriptor;
		bool has_display_range_descriptor;
		unsigned serial_number;
		unsigned char week, year;
		bool is_analog;
		bool supports_continuous_freq;
		bool supports_gtf;
		bool supports_sec_gtf;
		unsigned sec_gtf_start_freq;
		double C, M, K, J;
		bool supports_cvt;
		bool has_spwg;
		bool uses_srgb;
		unsigned detailed_block_cnt;
		unsigned dtd_cnt;
		bool seen_non_detailed_descriptor;
		bool has_640x480p60_est_timing;
		bool preferred_is_also_native;
		timings_ext preferred_timing;

		unsigned min_display_hor_freq_hz;
		unsigned max_display_hor_freq_hz;
		unsigned min_display_vert_freq_hz;
		unsigned max_display_vert_freq_hz;
		unsigned max_display_pixclk_khz;
		unsigned max_display_width_mm;
		unsigned max_display_height_mm;
		unsigned max_pos_neg_hor_freq_khz;
	} base;

	// CTA-861 block state
	struct {
		unsigned preparsed_total_dtds;
		vec_timings_ext vec_dtds;
		unsigned preparsed_total_vtdbs;
		vec_timings_ext vec_vtdbs;
		cta_vfd preparsed_first_vfd;
		vec_timings_ext preferred_timings;
		vec_timings_ext preferred_timings_vfpdb;
		bool preparsed_has_t8vtdb;
		unsigned preparsed_t8vtdb_dmt;
		// Keep track of the found Tag/Extended Tag pairs.
		// The unsigned value is equal to: (tag) | (OUI enum << 12) or (extended tag) | (tag << 8) | (OUI enum << 12)
		std::vector<unsigned> found_tags;
		timings_ext t8vtdb;
		vec_timings_ext native_timings;
		vec_timings_ext native_timing_nvrdb;
		// in 0.1 mm units
		unsigned image_width, image_height;
		bool nvrdb_has_size;
		bool has_vic_1;
		bool first_svd_might_be_preferred;
		unsigned char byte3;
		bool has_hdmi;
		unsigned hdmi_max_rate;
		bool has_vcdb;
		bool has_vfpdb;
		bool has_nvrdb;
		bool has_cdb;
		unsigned preparsed_speaker_count;
		bool preparsed_sld_has_coord;
		bool preparsed_sld;
		bool has_sldb;
		bool has_pidb;
		unsigned short preparsed_phys_addr;
		unsigned previous_cta_tag;
		bool have_hf_vsdb, have_hf_scdb;
		unsigned hf_eeodb_blocks;
		unsigned block_number;
		bool has_svrs;
		bool first_svd;
		unsigned supported_hdmi_vic_codes;
		unsigned supported_hdmi_vic_vsb_codes;
		unsigned short vics[256][2];
		bool preparsed_has_vic[2][256];
		std::vector<unsigned char> preparsed_svds[2];
		unsigned preparsed_max_vic_pixclk_khz;
		enum hdmi_image_size preparsed_image_size;
		bool warn_about_hdmi_2x_dtd;
		unsigned avi_version;
		unsigned avi_v4_length;
		bool has_ycbcr444;
		bool has_ycbcr422;
		bool has_ycbcr420;
		bool first_cta;
	} cta;

	// DisplayID block state
	struct {
		unsigned char version;
		unsigned short preparsed_color_ids;
		unsigned short preparsed_xfer_ids;
		unsigned preparsed_displayid_blocks;
		bool is_base_block;
		bool is_display;
		bool is_arvr;
		bool has_product_identification;
		bool has_display_parameters;
		bool has_type_1_7;
		bool has_display_interface_features;
		bool has_tiled_display_topology;
		bool has_stereo_display_interface;
		bool has_arvr_hdm;
		bool has_arvr_layer;
		bool has_ycbcr_420;
		bool has_stereo;
		vec_timings_ext preferred_timings;
		unsigned native_width, native_height;
		// in 0.1 mm units
		unsigned image_width, image_height;
		unsigned block_number;
		// Keep track of the found CTA-861 Tag/Extended Tag pairs.
		// The unsigned value is equal to: (tag) | (OUI enum << 12) or (extended tag) | (tag << 8) | (OUI enum << 12)
		std::vector<unsigned> found_tags;
	} dispid;

	// Block Map block state
	struct {
		bool saw_block_1;
		bool saw_block_128;
	} block_map;

	std::string dtd_type(unsigned dtd);
	std::string dtd_type() { return dtd_type(base.dtd_cnt); }
	bool print_timings(const char *prefix, const struct timings *t,
			   const char *type, const char *flags = "",
			   bool detailed = false, bool do_checks = true,
			   unsigned ntsc = 2);
	bool print_timings(const char *prefix, const struct timings_ext &t,
			   bool detailed = false, bool do_checks = true,
			   unsigned ntsc = 2)
	{
		return print_timings(prefix, &t.t, t.type.c_str(), t.flags.c_str(),
				     detailed, do_checks, ntsc);
	};
	timings calc_gtf_mode(unsigned h_pixels, unsigned v_lines,
			      double ip_freq_rqd, bool int_rqd = false,
			      enum gtf_ip_parm ip_parm = gtf_ip_vert_freq,
			      bool margins_rqd = false, bool secondary = false,
			      double C = 40, double M = 600, double K = 128, double J = 20);
	void edid_gtf_mode(unsigned refresh, struct timings &t);
	timings calc_cvt_mode(unsigned h_pixels, unsigned v_lines,
			      double ip_freq_rqd, unsigned rb, bool int_rqd = false,
			      bool margins_rqd = false, bool alt = false,
			      unsigned rb_h_blank = 0, unsigned rb_v_blank = 460,
			      bool early_vsync_rqd = false);
	void edid_cvt_mode(unsigned refresh, struct timings &t, unsigned rb_h_blank = 0,
			   unsigned rb_v_blank = 460, bool early_vsync_rqd = false);
	void detailed_cvt_descriptor(const char *prefix, const unsigned char *x, bool first);
	timings calc_ovt_mode(unsigned hact, unsigned vact,
			      unsigned hratio, unsigned vratio,
			      unsigned frame_rate);
	void print_standard_timing(const char *prefix, unsigned char b1, unsigned char b2,
				   bool gtf_only = false, bool show_both = false);
	void detailed_display_range_limits(const unsigned char *x);
	void detailed_epi(const unsigned char *x);
	void detailed_timings(const char *prefix, const unsigned char *x,
			      bool base_or_cta = true);
	bool preparse_detailed_block(unsigned char *x);
	void preparse_base_block(unsigned char *x);
	void detailed_block(const unsigned char *x);
	void parse_base_block(const unsigned char *x);
	void check_base_block(const unsigned char *x);
	void list_dmts();
	void list_established_timings();
	char *manufacturer_name(const unsigned char *x);

	void data_block_oui(std::string block_name, const unsigned char *x, unsigned length, unsigned *ouinum,
	                    bool ignorezeros = false, bool do_ascii = false, bool big_endian = false,
			    bool silent = false);

	void print_vic_index(const char *prefix, unsigned idx, const char *suffix, bool ycbcr420 = false);
	void hdmi_latency(unsigned char vid_lat, unsigned char aud_lat, bool is_ilaced);
	void cta_vcdb(const unsigned char *x, unsigned length);
	void cta_svd(const unsigned char *x, unsigned n, bool for_ycbcr420);
	void cta_vfdb(const unsigned char *x, unsigned n);
	void cta_y420cmdb(const unsigned char *x, unsigned length);
	void cta_print_svr(unsigned char svr, vec_timings_ext &vec_tim);
	void cta_vfpdb(const unsigned char *x, unsigned length);
	void cta_nvrdb(const unsigned char *x, unsigned length);
	cta_vfd cta_parse_vfd(const unsigned char *x, unsigned lvfd);
	void cta_rcdb(const unsigned char *x, unsigned length);
	void cta_sldb(const unsigned char *x, unsigned length);
	void cta_pidb(const unsigned char *x, unsigned length);
	void cta_preparse_sldb(const unsigned char *x, unsigned length);
	void cta_colorimetry_block(const unsigned char *x, unsigned length);
	void cta_hdmi_block(const unsigned char *x, unsigned length);
	void cta_hf_scdb(const unsigned char *x, unsigned length);
	void cta_displayid_type_7(const unsigned char *x, unsigned length);
	void cta_displayid_type_8(const unsigned char *x, unsigned length);
	void cta_displayid_type_10(const unsigned char *x, unsigned length);
	void cta_block(const unsigned char *x, std::vector<unsigned> &found_tags);
	void cta_sadb(const unsigned char *x, unsigned length);
	void cta_audio_block(const unsigned char *x, unsigned length);
	void preparse_cta_block(unsigned char *x);
	void parse_cta_block(const unsigned char *x);
	void cta_resolve_svr(timings_ext &t_ext);
	void cta_resolve_svrs();
	void check_cta_blocks();
	void cta_list_vics();
	void cta_list_hdmi_vics();
	void cta_list_rids();
	void cta_list_rid_timings(unsigned list_rid = 0);

	void set_displayid_native_res(unsigned w, unsigned h);
	void parse_digital_interface(const unsigned char *x);
	void parse_display_device(const unsigned char *x);
	void parse_display_caps(const unsigned char *x);
	void parse_display_xfer(const unsigned char *x);
	void parse_di_ext_block(const unsigned char *x);

	void check_displayid_datablock_revision(unsigned char hdr,
						unsigned char valid_flags = 0,
						unsigned char rev = 0);
	void parse_displayid_product_id(const unsigned char *x);
	std::string product_type(unsigned char x, bool heading);
	void parse_displayid_interface_features(const unsigned char *x);
	void parse_displayid_parameters(const unsigned char *x);
	void parse_displayid_parameters_v2(const unsigned char *x, unsigned block_rev);
	void parse_displayid_display_intf(const unsigned char *x);
	void parse_displayid_color_characteristics(const unsigned char *x);
	void parse_displayid_transfer_characteristics(const unsigned char *x);
	void parse_displayid_stereo_display_intf(const unsigned char *x);
	void parse_displayid_type_1_7_timing(const unsigned char *x,
					     bool type7, unsigned block_rev, bool is_cta = false);
	void parse_displayid_type_2_timing(const unsigned char *x);
	void parse_displayid_type_3_timing(const unsigned char *x);
	void parse_displayid_type_4_8_timing(unsigned char type, unsigned short id, bool is_cta = false);
	void parse_displayid_video_timing_range_limits(const unsigned char *x);
	void parse_displayid_string(const unsigned char *x);
	void parse_displayid_display_device(const unsigned char *x);
	void parse_displayid_intf_power_sequencing(const unsigned char *x);
	void parse_displayid_type_5_timing(const unsigned char *x);
	void parse_displayid_tiled_display_topology(const unsigned char *x, bool is_v2);
	void parse_displayid_type_6_timing(const unsigned char *x);
	void parse_displayid_type_9_timing(const unsigned char *x);
	void parse_displayid_dynamic_video_timings_range_limits(const unsigned char *x);
	void parse_displayid_ContainerID(const unsigned char *x);
	void parse_displayid_adaptive_sync(const unsigned char *x);
	void parse_displayid_arvr_hmd(const unsigned char *x);
	void parse_displayid_arvr_layer(const unsigned char *x);
	void parse_displayid_brightness_lum_range(const unsigned char *x);
	void parse_displayid_type_10_timing(const unsigned char *x, unsigned sz,
					    bool is_cta = false);
	void preparse_displayid_block(unsigned char *x);
	unsigned displayid_block(const unsigned version, const unsigned char *x, unsigned length);
	void parse_displayid_block(const unsigned char *x);
	void parse_displayid_vesa(const unsigned char *x);
	void parse_displayid_apple(const unsigned char *x);
	void parse_displayid_cta_data_block(const unsigned char *x);
	void check_displayid_blocks();

	void parse_vtb_ext_block(const unsigned char *x);

	void parse_string_table(const unsigned char *x);
	void preparse_ls_ext_block(unsigned char *x);
	void parse_ls_ext_block(const unsigned char *x);

	void parse_block_map(const unsigned char *x);

	void preparse_extension(unsigned char *x);
	void parse_extension(const unsigned char *x);
	void print_preferred_timings();
	void print_native_res();
	int parse_edid();

	int parse_if(const std::string &fname);
	int parse_if_hdr(const unsigned char *x, unsigned size, unsigned char mask = 0xff);
	void parse_if_hdmi(const unsigned char *x, unsigned len);
	void parse_if_hdmi_forum(const unsigned char *x, unsigned len);
	void parse_if_vendor(const unsigned char *x, unsigned size);
	void parse_if_avi(const unsigned char *x, unsigned size);
	void parse_if_spd(const unsigned char *x, unsigned size);
	void parse_if_audio(const unsigned char *x, unsigned size);
	void parse_if_mpeg_source(const unsigned char *x, unsigned size);
	void parse_if_ntsc_vbi(const unsigned char *x, unsigned size);
	void parse_if_drm(const unsigned char *x, unsigned size);

	int parse_eld(const std::string &fname);
	void parse_eld_baseline(const unsigned char *x, unsigned size);
};

static inline void add_str(std::string &s, const std::string &add)
{
	if (s.empty())
		s = add;
	else if (!add.empty())
		s = s + ", " + add;
}

void msg(bool is_warn, const char *fmt, ...);

#ifdef _WIN32

#define warn(fmt, ...) msg(true, fmt, __VA_ARGS__)
#define warn_once(fmt, ...)				\
	do {						\
		static bool shown_warn;			\
		if (!shown_warn) {			\
			shown_warn = true;		\
			msg(true, fmt, __VA_ARGS__);	\
		}					\
	} while (0)
#define fail(fmt, ...) msg(false, fmt, __VA_ARGS__)

#else

#define warn(fmt, args...) msg(true, fmt, ##args)
#define warn_once(fmt, args...)				\
	do {						\
		static bool shown_warn;			\
		if (!shown_warn) {			\
			shown_warn = true;		\
			msg(true, fmt, ##args);		\
		}					\
	} while (0)
#define fail(fmt, args...) msg(false, fmt, ##args)

#endif

// NULL terminated array
extern const char *cta_speaker_map[];

static inline double chrom2d(const unsigned char *x)
{
	unsigned v = x[0] + (x[1] << 8);

	return v * 0.00002;
}

std::string utohex(unsigned char x);
std::string ouitohex(unsigned oui);
std::string containerid2s(const unsigned char *x);
bool memchk(const unsigned char *x, unsigned len, unsigned char v = 0);
void hex_block(const char *prefix, const unsigned char *x, unsigned length,
	       bool show_ascii = true, unsigned step = 16);
std::string block_name(unsigned char block);
void do_checksum(const char *prefix, const unsigned char *x, size_t len, size_t checksum_pos,
		 unsigned unused_bytes = 0);
void replace_checksum(unsigned char *x, size_t len);
void calc_ratio(struct timings *t);
unsigned calc_fps(const struct timings *t);
const char *oui_name(unsigned oui, unsigned *ouinum = NULL);
unsigned gcd(unsigned a, unsigned b);

bool match_timings(const timings &t1, const timings &t2);
bool timings_close_match(const timings &t1, const timings &t2);
const struct timings *find_dmt_id(unsigned char dmt_id);
const struct timings *close_match_to_dmt(const timings &t, unsigned &dmt);
const struct timings *find_vic_id(unsigned char vic);
const struct cta_rid *find_rid(unsigned char rid);
unsigned char rid_fps_to_vic(unsigned char rid, unsigned fps);
const struct timings *find_hdmi_vic_id(unsigned char hdmi_vic);
const struct timings *cta_close_match_to_vic(const timings &t, unsigned &vic);
bool cta_matches_vic(const timings &t, unsigned &vic);
unsigned char hdmi_vic_to_vic(unsigned char hdmi_vic);

extern bool to_utf8;
char *extract_string(const unsigned char *x, unsigned len, bool is_cp437);

#define oneoui(c,k,n) const unsigned kOUI_##k = __LINE__<<12;
#include "oui.h"

#ifdef __HAS_I2C_DEV__

int request_i2c_adapter(const char *device);
int read_edid(int adapter_fd, unsigned char *edid, bool silent = false);
int test_reliability(int adapter_fd, unsigned secs, unsigned msleep);
int read_hdcp(int adapter_fd);
int read_hdcp_ri(int adapter_fd, double ri_time);

#else

static inline int read_edid(int adapter_fd, unsigned char *edid) { return -ENODEV; }
static inline int test_reliability(int adapter_fd, unsigned secs, unsigned msleep) { return -ENODEV; }
static inline int read_hdcp(int adapter_fd) { return -ENODEV; }
static inline int read_hdcp_ri(int adapter_fd, double ri_time) { return -ENODEV; }

#endif

#endif
