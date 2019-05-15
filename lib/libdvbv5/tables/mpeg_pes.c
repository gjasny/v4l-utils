/*
 * Copyright (c) 2013 - Andre Roth <neolynx@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation version 2.1 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 * Or, point your browser to http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
 *
 */

#include <libdvbv5/mpeg_pes.h>
#include <libdvbv5/descriptors.h>
#include <libdvbv5/dvb-fe.h>
#include <inttypes.h>

#if __GNUC__ >= 9
#pragma GCC diagnostic ignored "-Waddress-of-packed-member"
#endif

ssize_t dvb_mpeg_pes_init(struct dvb_v5_fe_parms *parms, const uint8_t *buf, ssize_t buflen, uint8_t *table)
{
	struct dvb_mpeg_pes *pes = (struct dvb_mpeg_pes *) table;
	const uint8_t *p = buf;
	ssize_t bytes_read = 0;

	memcpy(table, p, sizeof(struct dvb_mpeg_pes));
	p += sizeof(struct dvb_mpeg_pes);
	bytes_read += sizeof(struct dvb_mpeg_pes);

	bswap32(pes->bitfield);
	bswap16(pes->length);

	if (pes->sync != 0x000001) {
		dvb_logerr("mpeg pes invalid, sync 0x%06x should be 0x000001", pes->sync);
		return -1;
	}

	if (pes->stream_id == DVB_MPEG_STREAM_PADDING) {
		dvb_logwarn("mpeg pes padding stream ignored");
	} else if (pes->stream_id == DVB_MPEG_STREAM_MAP ||
		   pes->stream_id == DVB_MPEG_STREAM_PRIVATE_2 ||
		   pes->stream_id == DVB_MPEG_STREAM_ECM ||
		   pes->stream_id == DVB_MPEG_STREAM_EMM ||
		   pes->stream_id == DVB_MPEG_STREAM_DIRECTORY ||
		   pes->stream_id == DVB_MPEG_STREAM_DSMCC ||
		   pes->stream_id == DVB_MPEG_STREAM_H222E ) {
		dvb_logerr("mpeg pes: unsupported stream type 0x%04x", pes->stream_id);
		return -2;
	} else {
		memcpy(pes->optional, p, sizeof(struct dvb_mpeg_pes_optional) -
					 sizeof(pes->optional->pts) -
					 sizeof(pes->optional->dts));
		p += sizeof(struct dvb_mpeg_pes_optional) -
		     sizeof(pes->optional->pts) -
		     sizeof(pes->optional->dts);
		bswap16(pes->optional->bitfield);
		pes->optional->pts = 0;
		pes->optional->dts = 0;
		if (pes->optional->PTS_DTS & 2) {
			struct ts_t pts;
			memcpy(&pts, p, sizeof(pts));
			p += sizeof(pts);
			bswap16(pts.bitfield);
			bswap16(pts.bitfield2);
			if (pts.one != 1 || pts.one1 != 1 || pts.one2 != 1)
				dvb_logwarn("mpeg pes: invalid pts");
			else {
				pes->optional->pts |= (uint64_t) pts.bits00;
				pes->optional->pts |= (uint64_t) pts.bits15 << 15;
				pes->optional->pts |= (uint64_t) pts.bits30 << 30;
			}
		}
		if (pes->optional->PTS_DTS & 1) {
			struct ts_t dts;
			memcpy(&dts, p, sizeof(dts));
			p += sizeof(dts);
			bswap16(dts.bitfield);
			bswap16(dts.bitfield2);
			pes->optional->dts |= (uint64_t) dts.bits00;
			pes->optional->dts |= (uint64_t) dts.bits15 << 15;
			pes->optional->dts |= (uint64_t) dts.bits30 << 30;
		}
		bytes_read += sizeof(struct dvb_mpeg_pes_optional);
	}
	return bytes_read;
}

void dvb_mpeg_pes_free(struct dvb_mpeg_pes *ts)
{
	free(ts);
}

void dvb_mpeg_pes_print(struct dvb_v5_fe_parms *parms, struct dvb_mpeg_pes *pes)
{
	dvb_loginfo("MPEG PES");
	dvb_loginfo(" - sync    0x%06x", pes->sync);
	dvb_loginfo(" - stream_id 0x%04x", pes->stream_id);
	dvb_loginfo(" - length      %d", pes->length);
	if (pes->stream_id == DVB_MPEG_STREAM_PADDING) {
	} else if (pes->stream_id == DVB_MPEG_STREAM_MAP ||
		   pes->stream_id == DVB_MPEG_STREAM_PRIVATE_2 ||
		   pes->stream_id == DVB_MPEG_STREAM_ECM ||
		   pes->stream_id == DVB_MPEG_STREAM_EMM ||
		   pes->stream_id == DVB_MPEG_STREAM_DIRECTORY ||
		   pes->stream_id == DVB_MPEG_STREAM_DSMCC ||
		   pes->stream_id == DVB_MPEG_STREAM_H222E ) {
		dvb_logwarn("  mpeg pes unsupported stream type 0x%04x", pes->stream_id);
	} else {
		dvb_loginfo("  mpeg pes optional");
		dvb_loginfo("   - two                      %d", pes->optional->two);
		dvb_loginfo("   - PES_scrambling_control   %d", pes->optional->PES_scrambling_control);
		dvb_loginfo("   - PES_priority             %d", pes->optional->PES_priority);
		dvb_loginfo("   - data_alignment_indicator %d", pes->optional->data_alignment_indicator);
		dvb_loginfo("   - copyright                %d", pes->optional->copyright);
		dvb_loginfo("   - original_or_copy         %d", pes->optional->original_or_copy);
		dvb_loginfo("   - PTS_DTS                  %d", pes->optional->PTS_DTS);
		dvb_loginfo("   - ESCR                     %d", pes->optional->ESCR);
		dvb_loginfo("   - ES_rate                  %d", pes->optional->ES_rate);
		dvb_loginfo("   - DSM_trick_mode           %d", pes->optional->DSM_trick_mode);
		dvb_loginfo("   - additional_copy_info     %d", pes->optional->additional_copy_info);
		dvb_loginfo("   - PES_CRC                  %d", pes->optional->PES_CRC);
		dvb_loginfo("   - PES_extension            %d", pes->optional->PES_extension);
		dvb_loginfo("   - length                   %d", pes->optional->length);
		if (pes->optional->PTS_DTS & 2)
			dvb_loginfo("   - pts                      %" PRIu64 " (%fs)",
				    pes->optional->pts, (float) pes->optional->pts / 90000.0);
		if (pes->optional->PTS_DTS & 1)
			dvb_loginfo("   - dts                      %" PRIu64 " (%fs)",
				    pes->optional->dts, (float) pes->optional->dts/ 90000.0);
	}
}
