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

#include <libdvbv5/mpeg_ts.h>
#include <libdvbv5/descriptors.h>
#include <libdvbv5/dvb-fe.h>

#if __GNUC__ >= 9
#pragma GCC diagnostic ignored "-Waddress-of-packed-member"
#endif

ssize_t dvb_mpeg_ts_init(struct dvb_v5_fe_parms *parms, const uint8_t *buf, ssize_t buflen, uint8_t *table, ssize_t *table_length)
{
	struct dvb_mpeg_ts *ts = (struct dvb_mpeg_ts *) table;
	const uint8_t *p = buf;

	if (buf[0] != DVB_MPEG_TS) {
		dvb_logerr("mpeg ts invalid marker 0x%02x, should be 0x%02x", buf[0], DVB_MPEG_TS);
		*table_length = 0;
		return -1;
	}

	memcpy(table, p, sizeof(struct dvb_mpeg_ts));
	p += sizeof(struct dvb_mpeg_ts);

	bswap16(ts->bitfield);

	if (ts->adaptation_field) {
		memcpy(ts->adaption, p, sizeof(struct dvb_mpeg_ts_adaption));
		p += ts->adaption->length + 1;
		/* FIXME: copy adaption->lenght bytes */
	}

	*table_length = p - buf;
	return p - buf;
}

void dvb_mpeg_ts_free(struct dvb_mpeg_ts *ts)
{
	free(ts);
}

void dvb_mpeg_ts_print(struct dvb_v5_fe_parms *parms, struct dvb_mpeg_ts *ts)
{
	dvb_loginfo("MPEG TS");
	dvb_loginfo(" - sync            0x%02x", ts->sync_byte);
	dvb_loginfo(" - tei                %d", ts->tei);
	dvb_loginfo(" - payload_start      %d", ts->payload_start);
	dvb_loginfo(" - priority           %d", ts->priority);
	dvb_loginfo(" - pid           0x%04x", ts->pid);
	dvb_loginfo(" - scrambling         %d", ts->scrambling);
	dvb_loginfo(" - adaptation_field   %d", ts->adaptation_field);
	dvb_loginfo(" - continuity_counter %d", ts->continuity_counter);
	if (ts->adaptation_field) {
		dvb_loginfo(" Adaptation Field");
                dvb_loginfo("   - length         %d", ts->adaption->length);
                dvb_loginfo("   - discontinued   %d", ts->adaption->discontinued);
                dvb_loginfo("   - random_access  %d", ts->adaption->random_access);
                dvb_loginfo("   - priority       %d", ts->adaption->priority);
                dvb_loginfo("   - PCR            %d", ts->adaption->PCR);
                dvb_loginfo("   - OPCR           %d", ts->adaption->OPCR);
                dvb_loginfo("   - splicing_point %d", ts->adaption->splicing_point);
                dvb_loginfo("   - private_data   %d", ts->adaption->private_data);
                dvb_loginfo("   - extension      %d", ts->adaption->extension);
	}
}
