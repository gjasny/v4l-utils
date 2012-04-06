/*
 * Copyright (c) 2011-2012 - Mauro Carvalho Chehab <mchehab@redhat.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation version 2
 * of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 * Or, point your browser to http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
 */

#include <inttypes.h>
#include <malloc.h>
#include <stdio.h>

#include "dvb-fe.h"
#include "libscan.h"
#include "descriptors.h"
#include "parse_string.h"
#include "dvb_frontend.h"

static char *default_charset = "iso-8859-1";
static char *output_charset = "utf-8";

static char *table[] = {
	[PAT] = "PAT",
	[PMT] = "PMT",
	[NIT] = "NIT",
	[SDT] = "SDT",
};

static const char *descriptors[] = {
	[0 ...255 ] = "Unknown descriptor",
	[video_stream_descriptor] = "video_stream_descriptor",
	[audio_stream_descriptor] = "audio_stream_descriptor",
	[hierarchy_descriptor] = "hierarchy_descriptor",
	[dvbpsi_registration_descriptor] = "dvbpsi_registration_descriptor",
	[ds_alignment_descriptor] = "ds_alignment_descriptor",
	[target_background_grid_descriptor] = "target_background_grid_descriptor",
	[video_window_descriptor] = "video_window_descriptor",
	[conditional_access_descriptor] = "conditional_access_descriptor",
	[iso639_language_descriptor] = "iso639_language_descriptor",
	[system_clock_descriptor] = "system_clock_descriptor",
	[multiplex_buffer_utilization_descriptor] = "multiplex_buffer_utilization_descriptor",
	[copyright_descriptor] = "copyright_descriptor",
	[maximum_bitrate_descriptor] = "maximum_bitrate_descriptor",
	[private_data_indicator_descriptor] = "private_data_indicator_descriptor",
	[smoothing_buffer_descriptor] = "smoothing_buffer_descriptor",
	[std_descriptor] = "std_descriptor",
	[ibp_descriptor] = "ibp_descriptor",
	[mpeg4_video_descriptor] = "mpeg4_video_descriptor",
	[mpeg4_audio_descriptor] = "mpeg4_audio_descriptor",
	[iod_descriptor] = "iod_descriptor",
	[sl_descriptor] = "sl_descriptor",
	[fmc_descriptor] = "fmc_descriptor",
	[external_es_id_descriptor] = "external_es_id_descriptor",
	[muxcode_descriptor] = "muxcode_descriptor",
	[fmxbuffersize_descriptor] = "fmxbuffersize_descriptor",
	[multiplexbuffer_descriptor] = "multiplexbuffer_descriptor",
	[content_labeling_descriptor] = "content_labeling_descriptor",
	[metadata_pointer_descriptor] = "metadata_pointer_descriptor",
	[metadata_descriptor] = "metadata_descriptor",
	[metadata_std_descriptor] = "metadata_std_descriptor",
	[AVC_video_descriptor] = "AVC_video_descriptor",
	[ipmp_descriptor] = "ipmp_descriptor",
	[AVC_timing_and_HRD_descriptor] = "AVC_timing_and_HRD_descriptor",
	[mpeg2_aac_audio_descriptor] = "mpeg2_aac_audio_descriptor",
	[flexmux_timing_descriptor] = "flexmux_timing_descriptor",
	[network_name_descriptor] = "network_name_descriptor",
	[service_list_descriptor] = "service_list_descriptor",
	[stuffing_descriptor] = "stuffing_descriptor",
	[satellite_delivery_system_descriptor] = "satellite_delivery_system_descriptor",
	[cable_delivery_system_descriptor] = "cable_delivery_system_descriptor",
	[VBI_data_descriptor] = "VBI_data_descriptor",
	[VBI_teletext_descriptor] = "VBI_teletext_descriptor",
	[bouquet_name_descriptor] = "bouquet_name_descriptor",
	[service_descriptor] = "service_descriptor",
	[country_availability_descriptor] = "country_availability_descriptor",
	[linkage_descriptor] = "linkage_descriptor",
	[NVOD_reference_descriptor] = "NVOD_reference_descriptor",
	[time_shifted_service_descriptor] = "time_shifted_service_descriptor",
	[short_event_descriptor] = "short_event_descriptor",
	[extended_event_descriptor] = "extended_event_descriptor",
	[time_shifted_event_descriptor] = "time_shifted_event_descriptor",
	[component_descriptor] = "component_descriptor",
	[mosaic_descriptor] = "mosaic_descriptor",
	[stream_identifier_descriptor] = "stream_identifier_descriptor",
	[CA_identifier_descriptor] = "CA_identifier_descriptor",
	[content_descriptor] = "content_descriptor",
	[parental_rating_descriptor] = "parental_rating_descriptor",
	[teletext_descriptor] = "teletext_descriptor",
	[telephone_descriptor] = "telephone_descriptor",
	[local_time_offset_descriptor] = "local_time_offset_descriptor",
	[subtitling_descriptor] = "subtitling_descriptor",
	[terrestrial_delivery_system_descriptor] = "terrestrial_delivery_system_descriptor",
	[multilingual_network_name_descriptor] = "multilingual_network_name_descriptor",
	[multilingual_bouquet_name_descriptor] = "multilingual_bouquet_name_descriptor",
	[multilingual_service_name_descriptor] = "multilingual_service_name_descriptor",
	[multilingual_component_descriptor] = "multilingual_component_descriptor",
	[private_data_specifier_descriptor] = "private_data_specifier_descriptor",
	[service_move_descriptor] = "service_move_descriptor",
	[short_smoothing_buffer_descriptor] = "short_smoothing_buffer_descriptor",
	[frequency_list_descriptor] = "frequency_list_descriptor",
	[partial_transport_stream_descriptor] = "partial_transport_stream_descriptor",
	[data_broadcast_descriptor] = "data_broadcast_descriptor",
	[scrambling_descriptor] = "scrambling_descriptor",
	[data_broadcast_id_descriptor] = "data_broadcast_id_descriptor",
	[transport_stream_descriptor] = "transport_stream_descriptor",
	[DSNG_descriptor] = "DSNG_descriptor",
	[PDC_descriptor] = "PDC_descriptor",
	[AC_3_descriptor] = "AC_3_descriptor",
	[ancillary_data_descriptor] = "ancillary_data_descriptor",
	[cell_list_descriptor] = "cell_list_descriptor",
	[cell_frequency_link_descriptor] = "cell_frequency_link_descriptor",
	[announcement_support_descriptor] = "announcement_support_descriptor",
	[application_signalling_descriptor] = "application_signalling_descriptor",
	[adaptation_field_data_descriptor] = "adaptation_field_data_descriptor",
	[service_identifier_descriptor] = "service_identifier_descriptor",
	[service_availability_descriptor] = "service_availability_descriptor",
	[default_authority_descriptor] = "default_authority_descriptor",
	[related_content_descriptor] = "related_content_descriptor",
	[TVA_id_descriptor] = "TVA_id_descriptor",
	[content_identifier_descriptor] = "content_identifier_descriptor",
	[time_slice_fec_identifier_descriptor] = "time_slice_fec_identifier_descriptor",
	[ECM_repetition_rate_descriptor] = "ECM_repetition_rate_descriptor",
	[S2_satellite_delivery_system_descriptor] = "S2_satellite_delivery_system_descriptor",
	[enhanced_AC_3_descriptor] = "enhanced_AC_3_descriptor",
	[DTS_descriptor] = "DTS_descriptor",
	[AAC_descriptor] = "AAC_descriptor",
	[XAIT_location_descriptor] = "XAIT_location_descriptor",
	[FTA_content_management_descriptor] = "FTA_content_management_descriptor",
	[extension_descriptor] = "extension_descriptor",

	[CUE_identifier_descriptor] = "CUE_identifier_descriptor",

	[component_name_descriptor] = "component_name_descriptor",
	[logical_channel_number_descriptor] = "logical_channel_number_descriptor",

	[carousel_id_descriptor] = "carousel_id_descriptor",
	[association_tag_descriptor] = "association_tag_descriptor",
	[deferred_association_tags_descriptor] = "deferred_association_tags_descriptor",

	[hierarchical_transmission_descriptor] = "hierarchical_transmission_descriptor",
	[digital_copy_control_descriptor] = "digital_copy_control_descriptor",
	[network_identifier_descriptor] = "network_identifier_descriptor",
	[partial_transport_stream_time_descriptor] = "partial_transport_stream_time_descriptor",
	[audio_component_descriptor] = "audio_component_descriptor",
	[hyperlink_descriptor] = "hyperlink_descriptor",
	[target_area_descriptor] = "target_area_descriptor",
	[data_contents_descriptor] = "data_contents_descriptor",
	[video_decode_control_descriptor] = "video_decode_control_descriptor",
	[download_content_descriptor] = "download_content_descriptor",
	[CA_EMM_TS_descriptor] = "CA_EMM_TS_descriptor",
	[CA_contract_information_descriptor] = "CA_contract_information_descriptor",
	[CA_service_descriptor] = "CA_service_descriptor",
	[TS_Information_descriptior] = "TS_Information_descriptior",
	[extended_broadcaster_descriptor] = "extended_broadcaster_descriptor",
	[logo_transmission_descriptor] = "logo_transmission_descriptor",
	[basic_local_event_descriptor] = "basic_local_event_descriptor",
	[reference_descriptor] = "reference_descriptor",
	[node_relation_descriptor] = "node_relation_descriptor",
	[short_node_information_descriptor] = "short_node_information_descriptor",
	[STC_reference_descriptor] = "STC_reference_descriptor",
	[series_descriptor] = "series_descriptor",
	[event_group_descriptor] = "event_group_descriptor",
	[SI_parameter_descriptor] = "SI_parameter_descriptor",
	[broadcaster_Name_Descriptor] = "broadcaster_Name_Descriptor",
	[component_group_descriptor] = "component_group_descriptor",
	[SI_prime_TS_descriptor] = "SI_prime_TS_descriptor",
	[board_information_descriptor] = "board_information_descriptor",
	[LDT_linkage_descriptor] = "LDT_linkage_descriptor",
	[connected_transmission_descriptor] = "connected_transmission_descriptor",
	[content_availability_descriptor] = "content_availability_descriptor",
	[service_group_descriptor] = "service_group_descriptor",
	[carousel_compatible_composite_Descriptor] = "carousel_compatible_composite_Descriptor",
	[conditional_playback_descriptor] = "conditional_playback_descriptor",
	[ISDBT_delivery_system_descriptor] = "ISDBT_delivery_system_descriptor",
	[partial_reception_descriptor] = "partial_reception_descriptor",
	[emergency_information_descriptor] = "emergency_information_descriptor",
	[data_component_descriptor] = "data_component_descriptor",
	[system_management_descriptor] = "system_management_descriptor",
};

static const char *extension_descriptors[] = {
	[image_icon_descriptor] = "image_icon_descriptor",
	[cpcm_delivery_signalling_descriptor] = "cpcm_delivery_signalling_descriptor",
	[CP_descriptor] = "CP_descriptor",
	[CP_identifier_descriptor] = "CP_identifier_descriptor",
	[T2_delivery_system_descriptor] = "T2_delivery_system_descriptor",
	[SH_delivery_system_descriptor] = "SH_delivery_system_descriptor",
	[supplementary_audio_descriptor] = "supplementary_audio_descriptor",
	[network_change_notify_descriptor] = "network_change_notify_descriptor",
	[message_descriptor] = "message_descriptor",
	[target_region_descriptor] = "target_region_descriptor",
	[target_region_name_descriptor] = "target_region_name_descriptor",
	[service_relocated_descriptor] = "service_relocated_descriptor",
};


static int bcd_to_int(const unsigned char *bcd, int bits)
{
	int nibble = 0;
	int ret = 0;

	while (bits) {
		ret *= 10;
		if (!nibble)
			ret += *bcd >> 4;
		else
			ret += *bcd & 0x0f;
		bits -= 4;
		nibble = !nibble;
		if (!nibble)
			bcd++;
	}
	return ret;
}

static int add_frequency(struct nit_table *nit_table, uint32_t freq)
{
	unsigned n = nit_table->frequency_len;

	nit_table->frequency = realloc(nit_table->frequency,
				       (n + 1) * sizeof(*nit_table->frequency));

	if (!nit_table->frequency)
		return -ENOMEM;

	nit_table->frequency[n] = freq;
	nit_table->frequency_len++;

	return 0;
}

static void parse_NIT_ISDBT(struct nit_table *nit_table,
			     const unsigned char *buf, int dlen,
			     int verbose)
{
	uint64_t freq;
	static const uint32_t interval[] = {
		[0] = GUARD_INTERVAL_1_32,
		[1] = GUARD_INTERVAL_1_16,
		[2] = GUARD_INTERVAL_1_8,
		[3] = GUARD_INTERVAL_1_4,
	};
	static const char *interval_name[] = {
		[GUARD_INTERVAL_1_4] =    "1/4",
		[GUARD_INTERVAL_1_8] =    "1/8",
		[GUARD_INTERVAL_1_16] =   "1/16",
		[GUARD_INTERVAL_1_32] =   "1/32",
	};
	static const uint32_t mode[] = {
		[0] = TRANSMISSION_MODE_2K,	/* Mode 1 */
		[1] = TRANSMISSION_MODE_4K,	/* Mode 2 */
		[2] = TRANSMISSION_MODE_8K,	/* Mode 3 */
		[3] = TRANSMISSION_MODE_AUTO	/* Reserved */
	};
	static const char *tm_name[] = {
		[TRANSMISSION_MODE_2K] =   "2K",
		[TRANSMISSION_MODE_4K] =   "4K",
		[TRANSMISSION_MODE_8K] =   "8K",
		[TRANSMISSION_MODE_AUTO] = "AUTO",
	};
	int i, isdbt_mode, guard;

	buf += 2;
	nit_table->delivery_system = SYS_ISDBT;
	isdbt_mode = buf[0] >> 6;
	guard = buf[0] >> 4 & 0x3;
	nit_table->area_code = ((buf[0] << 8) | buf[1]) & 0x0fff;
	nit_table->guard_interval = interval[guard];
	nit_table->transmission_mode = mode[isdbt_mode];
	if (verbose)
		printf("Area code: %d, mode %d (%s), guard interval: %s\n",
			nit_table->area_code,
			isdbt_mode + 1,
			tm_name[nit_table->transmission_mode],
			interval_name[nit_table->guard_interval]);
	for (i = 2; i < dlen; i += 2) {
		buf += 2;
		/*
		 * The spec is not very clear about that, but some tests
		 * showed how this is supported to be calculated.
		 */
		freq = ((buf[0] << 8 | buf[1]) * 1000000l) / 7; /* Hz */
		add_frequency(nit_table, freq);

		if (verbose)
			printf("Frequency %" PRId64 "\n", freq);
	}
}

static const unsigned dvbc_dvbs_freq_inner[] = {
	[0]  = FEC_AUTO,
	[1]  = FEC_1_2,
	[2]  = FEC_2_3,
	[3]  = FEC_3_4,
	[4]  = FEC_5_6,
	[5]  = FEC_7_8,
	[6]  = FEC_8_9,
	[7]  = FEC_3_5,
	[8]  = FEC_4_5,
	[9]  = FEC_9_10,
	[10 ...14] = FEC_AUTO,	/* Currently, undefined */
	[15] = FEC_NONE,
};

static void parse_NIT_DVBS(struct nit_table *nit_table,
			     const unsigned char *buf, int dlen,
			     int verbose)
{
	unsigned orbit, west;
	uint32_t freq;

	static const unsigned polarization[] = {
		[0] = POLARIZATION_H,
		[1] = POLARIZATION_V,
		[2] = POLARIZATION_L,
		[3] = POLARIZATION_R
	};
	static const unsigned rolloff[] = {
		[0] = ROLLOFF_35,
		[1] = ROLLOFF_25,
		[2] = ROLLOFF_20,
		[3] = ROLLOFF_AUTO,	/* Reserved */
	};
	static const unsigned modulation[] = {
		[0] = QAM_AUTO,
		[1] = QPSK,
		[2] = PSK_8,
		[3] = QAM_16
	};

	freq =  bcd_to_int(&buf[2], 32) * 10; /* KHz */
	add_frequency(nit_table, freq);

	orbit = bcd_to_int(&buf[6], 16);
	west = buf[8] >> 7;
	asprintf(&nit_table->orbit, "%d%c", orbit, west ? 'W' : 'E');

	nit_table->pol = polarization[(buf[8] >> 5) & 0x3];
	nit_table->modulation = modulation[(buf[8] >> 3) & 0x3];
	if (buf[8] & 1) {
		nit_table->delivery_system = SYS_DVBS2;
		nit_table->rolloff = rolloff[(buf[8] >> 1) & 0x3];
	} else {
		nit_table->delivery_system = SYS_DVBS;
		nit_table->rolloff = ROLLOFF_35;
	}
	nit_table->symbol_rate = bcd_to_int(&buf[9], 28) * 100; /* Bauds */
	nit_table->fec_inner = dvbc_dvbs_freq_inner[buf[12] & 0x07];

	if (verbose) {
		printf("DVB-%s orbit %s, freq %d, pol %d, modulation %d, rolloff %d\n",
		       (nit_table->delivery_system == SYS_DVBS) ? "S" : "S2",
		       nit_table->orbit, freq,
		       nit_table->pol, nit_table->modulation,
		       nit_table->rolloff);
		printf("Symbol rate %d, fec_inner %d\n",
		       nit_table->symbol_rate, nit_table->fec_inner);
	}
}

static void parse_NIT_DVBC(struct nit_table *nit_table,
			     const unsigned char *buf, int dlen,
			     int verbose)
{
	uint32_t freq;
	static const unsigned modulation[] = {
		[0] = QAM_AUTO,
		[1] = QAM_16,
		[2] = QAM_32,
		[3] = QAM_64,
		[4] = QAM_128,
		[5] = QAM_256,
		[6 ...255] = QAM_AUTO	/* Reserved for future usage*/
	};

	freq = bcd_to_int(&buf[2], 32) * 100; /* KHz */
	add_frequency(nit_table, freq);

	nit_table->fec_outer = dvbc_dvbs_freq_inner[buf[7] & 0x07];
	nit_table->modulation = modulation[buf[8]];
	nit_table->symbol_rate = bcd_to_int(&buf[9], 28) * 100; /* Bauds */
	nit_table->fec_inner = dvbc_dvbs_freq_inner[buf[12] & 0x07];

	nit_table->delivery_system = SYS_DVBC_ANNEX_A;

	if (verbose) {
		printf("DVB-C freq %d, modulation %d, Symbol rate %d\n",
		       freq,
		       nit_table->modulation,
		       nit_table->symbol_rate);
		printf("fec_inner %d, fec_inner %d\n",
		       nit_table->fec_inner, nit_table->fec_outer);
	}
}

static void parse_NIT_DVBT(struct nit_table *nit_table,
			     const unsigned char *buf, int dlen,
			     int verbose)
{
	uint32_t freq;
	static const unsigned bw[] = {
		[0] = 8000000,
		[1] = 7000000,
		[2] = 6000000,
		[3] = 5000000,
		[4 ...7] = 0,		/* Reserved */
	};
	static const unsigned modulation[] = {
		[0] = QPSK,
		[1] = QAM_16,
		[2] = QAM_32,
		[3] = QAM_AUTO	/* Reserved */
	};
	static const unsigned hierarchy[] = {
		[0] = HIERARCHY_NONE,
		[1] = HIERARCHY_1,
		[2] = HIERARCHY_2,
		[3] = HIERARCHY_4,
	};
	static const unsigned code_rate[] = {
		[0] = FEC_1_2,
		[1] = FEC_2_3,
		[2] = FEC_3_4,
		[3] = FEC_5_6,
		[4] = FEC_7_8,
		[5 ...7] = FEC_AUTO,	/* Reserved */
	};
	static const uint32_t interval[] = {
		[0] = GUARD_INTERVAL_1_32,
		[1] = GUARD_INTERVAL_1_16,
		[2] = GUARD_INTERVAL_1_8,
		[3] = GUARD_INTERVAL_1_4,
	};
	static const unsigned transmission_mode[] = {
		[0] = TRANSMISSION_MODE_2K,
		[1] = TRANSMISSION_MODE_8K,
		[2] = TRANSMISSION_MODE_4K,
		[3] = TRANSMISSION_MODE_AUTO,	/* Reserved */
	};

	freq = (buf[2] << 24) | (buf[3] << 16) | (buf[4] << 8)  |  buf[5];
	freq *= 10; /* Hz */
	add_frequency(nit_table, freq);

	nit_table->has_dvbt = 1;
	if (nit_table->delivery_system != SYS_DVBT2)
		nit_table->delivery_system = SYS_DVBT;
	nit_table->bandwidth = bw[(buf[6] >> 5) & 0x07];
	nit_table->is_hp = (buf[6] & 0x10) ? 1 : 0;
	nit_table->has_time_slicing = (buf[6] & 0x08) ? 0 : 1;
	nit_table->has_mpe_fec = (buf[6] & 0x04) ? 0 : 1;

	nit_table->modulation = modulation[buf[7] >> 6];
	nit_table->hierarchy = hierarchy[(buf[7] >> 3) & 0x03];
	nit_table->is_in_depth_interleaver = (buf[7] & 0x20) ? 1 : 0;
	nit_table->code_rate_hp = code_rate[buf[7] & 0x07];
	nit_table->code_rate_lp = code_rate[(buf[8] >> 5) & 0x07];
	nit_table->guard_interval = interval[(buf[8] >> 3) & 0x03];
	nit_table->transmission_mode = transmission_mode[(buf[8] >> 1) & 0x02];
	nit_table->has_other_frequency = buf[8] & 0x01;

	if (verbose) {
		printf("DVB-T freq %d, bandwidth %d modulation %d\n",
		       freq,
		       nit_table->bandwidth,
		       nit_table->modulation);
		printf("hierarchy %d, code rate HP %d, LP %d, guard interval %d\n",
		       nit_table->hierarchy,
		       nit_table->code_rate_hp,
		       nit_table->code_rate_lp,
		       nit_table->guard_interval);
		printf("transmission mode %d\n", nit_table->transmission_mode);
	}
}

static void parse_NIT_DVBT2(struct nit_table *nit_table,
			    const unsigned char *buf, int dlen,
			    int verbose)
{
	static const unsigned bw[] = {
		[0] =  8000000,
		[1] =  7000000,
		[2] =  6000000,
		[3] =  5000000,
		[4] = 10000000,
		[5] =  1712000,
		[6 ...15] = 0,		/* Reserved */
	};
	static const uint32_t interval[] = {
		[0] = GUARD_INTERVAL_1_32,
		[1] = GUARD_INTERVAL_1_16,
		[2] = GUARD_INTERVAL_1_8,
		[3] = GUARD_INTERVAL_1_4,
		[4] = GUARD_INTERVAL_1_128,
		[5] = GUARD_INTERVAL_19_128,
		[6] = GUARD_INTERVAL_19_256,
		[7 ...15] = GUARD_INTERVAL_AUTO /* Reserved */
	};
	static const unsigned transmission_mode[] = {
		[0] = TRANSMISSION_MODE_2K,
		[1] = TRANSMISSION_MODE_8K,
		[2] = TRANSMISSION_MODE_4K,
		[3] = TRANSMISSION_MODE_1K,
		[4] = TRANSMISSION_MODE_16K,
		[5] = TRANSMISSION_MODE_32K,
		[6 ...7] = TRANSMISSION_MODE_AUTO,	/* Reserved */
	};

	nit_table->delivery_system = SYS_DVBT2;


	buf += 2;
	nit_table->plp_id = buf[0];
	nit_table->system_id = buf[1] << 8 | buf[2];

	buf += 2;
	dlen -= 4;
	if (dlen <= 0)
		return;

	nit_table->bandwidth = bw[(buf[0] >> 2) & 0x0f];
	nit_table->guard_interval = interval[buf[1] >> 5];
	nit_table->transmission_mode = transmission_mode[(buf[1] >> 2) & 0x07];
	nit_table->has_other_frequency = buf[1] & 0x02 ? 1 : 0;

	/* FIXME: add a parser for the cell tables */
	return;
}

static void parse_freq_list(struct nit_table *nit_table,
			    const unsigned char *buf, int dlen,
			    int verbose)
{
	int i;
	uint32_t freq;

	buf += 3;
	for (i = 3; i < dlen; i += 4) {
		freq = (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
		freq *= 10;	/* Hz */
		add_frequency(nit_table, freq);

		if (verbose)
			printf("Frequency %d\n", freq);
		buf += 4;
	}
}

static void parse_partial_reception(struct nit_table *nit_table,
				    const unsigned char *buf, int dlen,
				    int verbose)
{
	int i;
	uint16_t **pid = &nit_table->partial_reception;
	unsigned *n = &nit_table->partial_reception_len;

	buf += 2;
	for (i = 0; i < dlen; i += 2) {
		*pid = realloc(*pid, (*n + 1) * sizeof(**pid));
		nit_table->partial_reception[*n] = buf[i] << 8 | buf[i + 1];
		if (verbose)
			printf("Service 0x%04x has partial reception\n",
			       nit_table->partial_reception[*n]);
		buf += 2;
		(*n)++;
	}
}

static int parse_extension_descriptor(enum dvb_tables type,
				       struct dvb_descriptors *dvb_desc,
				       const unsigned char *buf, int dlen)
{
	unsigned char ext = buf[0];
	int i;

	if (dlen < 1)
		return 0;

	/* buf[0] will point to the first useful data */
	buf++;
	dlen--;

	if (dvb_desc->verbose) {
		printf("Extension descriptor %s (0x%02x), len %d",
			extension_descriptors[ext], ext, dlen);
		for (i = 0; i < dlen; i++) {
			if (!(i % 16))
				printf("\n\t");
			printf("%02x ", (uint8_t) *(buf + i));
		}
		printf("\n");
	}
	switch(ext) {
	case T2_delivery_system_descriptor:
		if (type != NIT)
			return 1;

		parse_NIT_DVBT2(&dvb_desc->nit_table, buf, dlen,
				dvb_desc->verbose);
		break;
	}

	return 0;
};

static void parse_net_name(struct nit_table *nit_table,
			   const unsigned char *buf, int dlen, int verbose)
{
	parse_string(&nit_table->network_name, &nit_table->network_alias,
		     &buf[2], dlen, default_charset, output_charset);
	if (verbose) {
		printf("Network");
		if (nit_table->network_name)
			printf(" %s", nit_table->network_name);
		if (nit_table->network_alias)
			printf(" (%s)", nit_table->network_alias);
		if (!nit_table->network_name && !nit_table->network_alias)
			printf(" unknown");
		printf("\n");
	}
}


static void parse_lcn(struct nit_table *nit_table,
		      const unsigned char *buf, int dlen, int verbose)
{
	int i, n = nit_table->lcn_len;
	const unsigned char *p = &buf[2];


	for (i = 0; i < dlen; i+= 4, p+= 4) {
		struct lcn_table **lcn = &nit_table->lcn;

		*lcn = realloc(*lcn, (n + 1) * sizeof(*lcn));
		(*lcn)[n].service_id = p[0] << 8 | p[1];
		(*lcn)[n].lcn = (p[2] << 8 | p[3]) & 0x3ff;
		nit_table->lcn_len++;
		n++;

		if (verbose)
			printf("Service ID: 0x%04x, LCN: %d\n",
			       (*lcn)[n].service_id,
			       (*lcn)[n].lcn);
	}
}

static void parse_service(struct service_table *service_table,
			  const unsigned char *buf, int dlen, int verbose)
{
	service_table->type = buf[2];
	parse_string(&service_table->provider_name,
		     &service_table->provider_alias,
		     &buf[4], buf[3],
		     default_charset, output_charset);
	buf += 4 + buf[3];
	parse_string(&service_table->service_name,
		     &service_table->service_alias,
		     &buf[1], buf[0],
		     default_charset, output_charset);
	if (verbose) {
		if (service_table->provider_name)
			printf("Provider %s", service_table->provider_name);
		if (service_table->service_alias)
			printf("(%s)", service_table->provider_alias);
		if (service_table->provider_name || service_table->service_alias)
			printf("\n");
		if (service_table->service_name)
			printf("Service %s", service_table->service_name);
		if (service_table->service_alias)
			printf("(%s)", service_table->service_alias);
		if (!service_table->provider_name &&
		    !service_table->service_alias &&
		    !service_table->service_name &&
		    !service_table->service_alias)
			printf("Service 0x%04x", service_table->service_id);
		printf("\n");
	}
}

void parse_descriptor(enum dvb_tables type,
			     struct dvb_descriptors *dvb_desc,
			     const unsigned char *buf, int len)
{
	int i;

	if (len == 0)
		return;

	if (dvb_desc->verbose)
		printf("Descriptors table len %d\n", len);
	do {
		int dlen = buf[1];
		int err = 0;

		if (dlen > len) {
			fprintf(stderr, "descriptor size %d is longer than %d!\n",
				dlen, len);
			return;
		}
		if (dvb_desc->verbose) {
			printf("%s (0x%02x), len %d",
			       descriptors[buf[0]], buf[0], buf[1]);
			for (i = 0; i < dlen; i++) {
				if (!(i % 16))
					printf("\n\t");
				printf("%02x ", (uint8_t) *(buf + i + 2));
			}
			printf("\n");
		}
		switch(buf[0]) {
		case extension_descriptor:
			err = parse_extension_descriptor(type, dvb_desc,
							 &buf[2], dlen);
			break;
		case iso639_language_descriptor:
		{
			int i;
			const unsigned char *p = &buf[2];

			if (dvb_desc->verbose) {
				for (i = 0; i < dlen; i+= 4, p += 4) {
					printf("Language = %c%c%c, amode = %d\n",
						p[0], p[1], p[2], p[3]);
				}
			}
			break;
		}
		case AAC_descriptor:
			if (dvb_desc->verbose)
				printf("AAC descriptor with len %d\n", dlen);
			break;
		case stream_identifier_descriptor:
			/* Don't need to parse it */
			if (dvb_desc->verbose)
				printf("Component tag 0x%02x\n", buf[2]);
			break;
		case network_name_descriptor:
			if (type != NIT) {
				err = 1;
				break;
			}
			parse_net_name(&dvb_desc->nit_table, buf, dlen,
				       dvb_desc->verbose);
			break;

		/* DVB NIT decoders */
		case satellite_delivery_system_descriptor:
			if (type != NIT) {
				err = 1;
				break;
			}
			parse_NIT_DVBS(&dvb_desc->nit_table, buf, dlen,
				       dvb_desc->verbose);
			break;
		case cable_delivery_system_descriptor:
			if (type != NIT) {
				err = 1;
				break;
			}
			parse_NIT_DVBC(&dvb_desc->nit_table, buf, dlen,
				       dvb_desc->verbose);
			break;
		case terrestrial_delivery_system_descriptor:
			if (type != NIT) {
				err = 1;
				break;
			}
			parse_NIT_DVBT(&dvb_desc->nit_table, buf, dlen,
				       dvb_desc->verbose);
			break;

		/* ISDBT NIT decoders */
		case ISDBT_delivery_system_descriptor:
			if (type != NIT) {
				err = 1;
				break;
			}

			parse_NIT_ISDBT(&dvb_desc->nit_table, buf, dlen,
					dvb_desc->verbose);
			break;
		case partial_reception_descriptor:
			if (type != NIT) {
				err = 1;
				break;
			}
			parse_partial_reception(&dvb_desc->nit_table, buf, dlen,
						dvb_desc->verbose);
			break;

		/* LCN decoder */
		case logical_channel_number_descriptor:
		{
			/*
			 * According with SCTE 57 2011, descriptor 0x83
			 * is the extended video descriptor. We don't need
			 * it, but don't print an error for this condition.
			 */
			if (type == PMT)
				break;
			if (type != NIT) {
				err = 1;
				break;
			}
			parse_lcn(&dvb_desc->nit_table, buf, dlen,
				  dvb_desc->verbose);
			break;
		}

		case TS_Information_descriptior:
			if (type != NIT) {
				err = 1;
				break;
			}
			dvb_desc->nit_table.virtual_channel = buf[2];
			if (dvb_desc->verbose)
				printf("Virtual channel = %d\n", buf[2]);
			break;

		case frequency_list_descriptor:
			if (type != NIT) {
				err = 1;
				break;
			}
			parse_freq_list(&dvb_desc->nit_table, buf, dlen,
					dvb_desc->verbose);
			break;

		case service_descriptor: {
			if (type != SDT) {
				err = 1;
				break;
			}
			parse_service(&dvb_desc->sdt_table.service_table[dvb_desc->cur_service],
				      buf, dlen, dvb_desc->verbose);
			break;
		}
		default:
			break;
		}
		if (err) {
			fprintf(stderr,
				"descriptor %s is invalid on %s table\n",
				descriptors[buf[0]], table[type]);
		}
		buf += dlen + 2;
		len -= dlen + 2;
	} while (len > 0);
}

int has_descriptor(struct dvb_descriptors *dvb_desc,
		    unsigned char needed_descriptor,
	            const unsigned char *buf, int len)
{
	if (len == 0)
		return 0;

	do {
		int dlen = buf[1];

		if (buf[0] == needed_descriptor)
			return 1;

		buf += dlen + 2;
		len -= dlen + 2;
	} while (len > 0);

	return 0;
}

#if 0
	/* TODO: remove those stuff */

		case ds_alignment_descriptor:
		case dvbpsi_registration_descriptor:
		case service_list_descriptor:
		case stuffing_descriptor:
		case VBI_data_descriptor:
		case VBI_teletext_descriptor:
		case bouquet_name_descriptor:
		case country_availability_descriptor:
		case linkage_descriptor:
		case NVOD_reference_descriptor:
		case time_shifted_service_descriptor:
		case short_event_descriptor:
		case extended_event_descriptor:
		case time_shifted_event_descriptor:
		case component_descriptor:
		case mosaic_descriptor:
		case CA_identifier_descriptor:
		case content_descriptor:
		case parental_rating_descriptor:
		case teletext_descriptor:
		case telephone_descriptor:
		case local_time_offset_descriptor:
		case subtitling_descriptor:
		case multilingual_network_name_descriptor:
		case multilingual_bouquet_name_descriptor:
		case multilingual_service_name_descriptor:
		case multilingual_component_descriptor:
		case private_data_specifier_descriptor:
		case service_move_descriptor:
		case short_smoothing_buffer_descriptor:
		case partial_transport_stream_descriptor:
		case data_broadcast_descriptor:
		case scrambling_descriptor:
		case data_broadcast_id_descriptor:
		case transport_stream_descriptor:
		case DSNG_descriptor:
		case PDC_descriptor:
		case AC_3_descriptor:
		case ancillary_data_descriptor:
		case cell_list_descriptor:
		case cell_frequency_link_descriptor:
		case announcement_support_descriptor:
		case application_signalling_descriptor:
		case adaptation_field_data_descriptor:
		case service_identifier_descriptor:
		case service_availability_descriptor:
		case default_authority_descriptor:
		case related_content_descriptor:
		case TVA_id_descriptor:
		case content_identifier_descriptor:
		case time_slice_fec_identifier_descriptor:
		case ECM_repetition_rate_descriptor:
		case S2_satellite_delivery_system_descriptor:
		case enhanced_AC_3_descriptor:
		case DTS_descriptor:
		case XAIT_location_descriptor:
		case FTA_content_management_descriptor:
		case extension_descriptor:

		case CUE_identifier_descriptor:
		case component_name_descriptor:
		case conditional_access_descriptor:
		case copyright_descriptor:
		case carousel_id_descriptor:
		case association_tag_descriptor:
		case deferred_association_tags_descriptor:
		case AVC_video_descriptor:
		case AVC_timing_and_HRD_descriptor:
		case hierarchical_transmission_descriptor:
		case digital_copy_control_descriptor:
		case network_identifier_descriptor:
		case partial_transport_stream_time_descriptor:
		case audio_component_descriptor:
		case hyperlink_descriptor:
		case target_area_descriptor:
		case data_contents_descriptor:
		case video_decode_control_descriptor:
		case download_content_descriptor:
		case CA_EMM_TS_descriptor:
		case CA_contract_information_descriptor:
		case CA_service_descriptor:
		case extended_broadcaster_descriptor:
		case logo_transmission_descriptor:
		case basic_local_event_descriptor:
		case reference_descriptor:
		case node_relation_descriptor:
		case short_node_information_descriptor:
		case STC_reference_descriptor:
		case series_descriptor:
		case event_group_descriptor:
		case SI_parameter_descriptor:
		case broadcaster_Name_Descriptor:
		case component_group_descriptor:
		case SI_prime_TS_descriptor:
		case board_information_descriptor:
		case LDT_linkage_descriptor:
		case connected_transmission_descriptor:
		case content_availability_descriptor:
		case service_group_descriptor:
		case carousel_compatible_composite_Descriptor:
		case conditional_playback_descriptor:
		case emergency_information_descriptor:
		case data_component_descriptor:
		case system_management_descriptor:
#endif
