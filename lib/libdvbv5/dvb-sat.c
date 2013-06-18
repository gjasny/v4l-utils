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
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "dvb-fe.h"
#include "dvb-v5-std.h"

static const struct dvb_sat_lnb lnb[] = {
	{
		.name = "Europe",
		.alias = "UNIVERSAL",
		.lowfreq = 9750,
		.highfreq = 10600,
		.rangeswitch = 11700,
		.freqrange = {
			{ 10800, 11800 },
			{ 11600, 12700 },
		}
	}, {
		.name = "Expressvu, North America",
		.alias = "DBS",
		.lowfreq = 11250,
		.freqrange = {
			{ 12200, 12700 }
		}
	}, {
		.name = "Standard",
		.alias = "STANDARD",
		.lowfreq = 10000,
		.freqrange = {
			{ 10945, 11450 }
		},
	}, {
		.name = "Astra",
		.alias = "ENHANCED",
		.lowfreq = 9750,
		.freqrange = {
			{ 10700, 11700 }
		},
	}, {
		.name = "Big Dish - Monopoint LNBf",
		.alias = "C-BAND",
		.lowfreq = 5150,
		.freqrange = {
			{ 3700, 4200 }
		},
	}, {
		.name = "Big Dish - Multipoint LNBf",
		.alias = "C-MULT",
		.lowfreq = 5150,
		.highfreq = 5750,
		.freqrange = {
			{ 3700, 4200 }
		},
	}, {
		.name = "DishPro LNBf",
		.alias = "DISHPRO",
		.lowfreq = 11250,
		.highfreq = 14350,
		.freqrange = {
			{ 12200, 12700 }
		}
	},
};

int dvb_sat_search_lnb(const char *name)
{
	int i = 0;

	for (i = 0; i < ARRAY_SIZE(lnb); i++) {
		if (!strcasecmp(name, lnb[i].alias))
			return i;
	}
	return -1;
}

int print_lnb(int i)
{
	if (i < 0 || i >= ARRAY_SIZE(lnb))
		return -1;

	printf("%s\n\t%s\n", lnb[i].alias, lnb[i].name);
	printf("\t%d to %d MHz",
	       lnb[i].freqrange[0].low, lnb[i].freqrange[0].high);
	if (lnb[i].freqrange[1].low)
		printf(" and %d to %d MHz",
		       lnb[i].freqrange[1].low, lnb[i].freqrange[1].high);
	printf("\n\t%s LO, ", lnb[i].highfreq ? "Dual" : "Single");
	if (!lnb[i].highfreq) {
		printf("IF = %d MHz\n", lnb[i].lowfreq);
		return 0;
	}
	if (!lnb[i].rangeswitch) {
		printf("Bandstacking, LO POL_R %d MHZ, LO POL_L %d MHz\n",
		       lnb[i].lowfreq, lnb[i].highfreq);
		return 0;
	}
	printf("IF = lowband %d MHz, highband %d MHz\n",
	       lnb[i].lowfreq, lnb[i].highfreq);

	return 0;
}

void print_all_lnb(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(lnb); i++) {
		print_lnb(i);
		printf("\n");
	}
}

const struct dvb_sat_lnb *dvb_sat_get_lnb(int i)
{
	if (i < 0 || i >= ARRAY_SIZE(lnb))
		return NULL;

	return &lnb[i];
}

/*
 * DVB satellite Diseqc specifics
 * According with:
 *	http://www.eutelsat.com/satellites/pdf/Diseqc/Reference%20docs/bus_spec.pdf
 *	http://www.eutelsat.com/satellites/pdf/Diseqc/associated%20docs/applic_info_turner-receiver.pdf
 */

struct diseqc_cmd {
	int len;
	union {
		unsigned char msg[6];
		struct {
			unsigned char framing;
			unsigned char address;
			unsigned char command;
			unsigned char data0;
			unsigned char data1;
			unsigned char data2;
		};
	};
};

enum diseqc_type {
	DISEQC_BROADCAST,
	DISEQC_BROADCAST_LNB_SWITCHER_SMATV,
	DISEQC_LNB,
	DISEQC_LNB_WITH_LOOP_SWITCH,
	DISEQC_SWITCHER,
	DISEQC_SWITHCER_WITH_LOOP,
	DISEQC_SMATV,
	DISEQC_ANY_POLARISER,
	DISEQC_LINEAR_POLARISER,
	DISEQC_ANY_POSITIONER,
	DISEQC_AZIMUTH_POSITIONER,
	DISEQC_ELEVATON_POSITIONER,
	DISEQC_ANY_INSTALLER_AID,
	DISEQC_SIGNAL_STRENGH_ANALOGUE_VAL,
	DISEQC_ANY_INTELLIGENT_SLAVE,
	DISEQC_SUBSCRIBER_HEADENDS,
};

static int diseqc_addr[] = {
	[DISEQC_BROADCAST]			= 0x00,
	[DISEQC_BROADCAST_LNB_SWITCHER_SMATV]	= 0x10,
	[DISEQC_LNB]				= 0x11,
	[DISEQC_LNB_WITH_LOOP_SWITCH]		= 0x12,
	[DISEQC_SWITCHER]			= 0x14,
	[DISEQC_SWITHCER_WITH_LOOP]		= 0x15,
	[DISEQC_SMATV]				= 0x18,
	[DISEQC_ANY_POLARISER]			= 0x20,
	[DISEQC_LINEAR_POLARISER]		= 0x21,
	[DISEQC_ANY_POSITIONER]			= 0x30,
	[DISEQC_AZIMUTH_POSITIONER]		= 0x31,
	[DISEQC_ELEVATON_POSITIONER]		= 0x32,
	[DISEQC_ANY_INSTALLER_AID]		= 0x40,
	[DISEQC_SIGNAL_STRENGH_ANALOGUE_VAL]	= 0x41,
	[DISEQC_ANY_INTELLIGENT_SLAVE]		= 0x70,
	[DISEQC_SUBSCRIBER_HEADENDS]		= 0x71,
};

static void dvbsat_diseqc_prep_frame_addr(struct diseqc_cmd *cmd,
					 enum diseqc_type type,
					 int reply,
					 int repeat)
{
	cmd->framing = 0xe0;	/* First four bits are always 1110 */
	if (reply)
		cmd->framing |=0x02;
	if (repeat)
		cmd->framing |=1;

	cmd->address = diseqc_addr[type];
}

//struct dvb_v5_fe_parms *parms; // legacy code, used for parms->fd, FIXME anyway

/* Inputs are numbered from 1 to 16, according with the spec */
static int dvbsat_diseqc_write_to_port_group(struct dvb_v5_fe_parms *parms, struct diseqc_cmd *cmd,
					     int high_band,
					     int pol_v,
					     int sat_number)
{
	dvbsat_diseqc_prep_frame_addr(cmd,
				      DISEQC_BROADCAST_LNB_SWITCHER_SMATV,
				      0, 0);

	cmd->command = 0x38;	/* Write to Port group 0 (Committed switches) */
	cmd->len = 4;

	/* Fill the 4 bits for the "input" select */
	cmd->data0 = 0xf0;
	cmd->data0 |= high_band;
	cmd->data0 |= pol_v ? 0 : 2;
	/* Instead of using position/option, use a number from 0 to 3 */
	cmd->data0 |= (sat_number % 0x3) << 2;

	return dvb_fe_diseqc_cmd(parms, cmd->len, cmd->msg);
}

static int dvbsat_scr_odu_channel_change(struct dvb_v5_fe_parms *parms, struct diseqc_cmd *cmd,
					 int high_band,
					 int pol_v,
					 int sat_number,
					 uint16_t t)
{
	int pos_b;

	dvbsat_diseqc_prep_frame_addr(cmd,
				      DISEQC_BROADCAST_LNB_SWITCHER_SMATV,
				      0, 0);

	cmd->command = 0x5a;	/* ODU Channel Change */
	cmd->len = 5;

	/* Fill the tuning parameter */
	cmd->data0 = (t >> 8) & 0x03;
	cmd->data1 = t & 0xff;

	/* Fill the satelite number - highest bit is for pos A/pos B */
	cmd->data0 |= (sat_number % 0x7) << 5;
	pos_b =  (sat_number & 0x8) ? 1 : 0;

	/* Fill the LNB number */
	cmd->data0 |= high_band ? 0 : 4;
	cmd->data0 |= pol_v ? 8 : 0;
	cmd->data0 |= pos_b ? 16 : 0;

	return dvb_fe_diseqc_cmd(parms, cmd->len, cmd->msg);
}

static int dvbsat_diseqc_set_input(struct dvb_v5_fe_parms *parms, uint16_t t)
{
	int rc;
	enum dvb_sat_polarization pol;
	dvb_fe_retrieve_parm(parms, DTV_POLARIZATION, &pol);
	int pol_v = (pol == POLARIZATION_V) || (pol == POLARIZATION_R);
	int high_band = parms->high_band;
	int sat_number = parms->sat_number;
	int vol_high = 0;
	int tone_on = 0;
	int mini_b = 0;
	struct diseqc_cmd cmd;

	if (!lnb->rangeswitch) {
		/*
		 * Bandstacking switches don't use 2 bands nor use
		 * DISEqC for setting the polarization. It also doesn't
		 * use any tone/tone burst
		 */
		pol_v = 0;
		high_band = 1;
	} else {
		/* Adjust voltage/tone accordingly */
		if (parms->sat_number < 2) {
			vol_high = pol_v ? 0 : 1;
			tone_on = high_band;
			mini_b = parms->sat_number & 1;
		}
	}

	rc = dvb_fe_sec_voltage(parms, 1, vol_high);
	if (rc)
		return rc;
	
	if (parms->sat_number > 0) {
		rc = dvb_fe_sec_tone(parms, SEC_TONE_OFF);
		if (rc)
			return rc;

		usleep(15 * 1000);

		if (!t)
			rc = dvbsat_diseqc_write_to_port_group(parms, &cmd, high_band,
							       pol_v, sat_number);
		else
			rc = dvbsat_scr_odu_channel_change(parms, &cmd, high_band,
							   pol_v, sat_number, t);

		if (rc) {
			dvb_logerr("sending diseq failed");
			return rc;
		}
		usleep((15 + parms->diseqc_wait) * 1000);

		rc = dvb_fe_diseqc_burst(parms, mini_b);
		if (rc)
			return rc;
		usleep(15 * 1000);
	}

	rc = dvb_fe_sec_tone(parms, tone_on ? SEC_TONE_ON : SEC_TONE_OFF);

	return rc;
}

/*
 * DVB satellite get/set params hooks
 */


int dvb_sat_set_parms(struct dvb_v5_fe_parms *parms)
{
	const struct dvb_sat_lnb *lnb = parms->lnb;
	enum dvb_sat_polarization pol;
	dvb_fe_retrieve_parm(parms, DTV_POLARIZATION, &pol);
	uint32_t freq;
	uint16_t t = 0;
	int rc;

	dvb_fe_retrieve_parm(parms, DTV_FREQUENCY, &freq);

	if (!lnb) {
		dvb_logerr("Need a LNBf to work");
		return -EINVAL;
	}

	/* Simple case: LNBf with just Single LO */
	if (!lnb->highfreq) {
		parms->freq_offset = lnb->lowfreq * 1000;
		goto ret;
	}

	/* polarization-controlled multi LNBf */
	if (!lnb->rangeswitch) {
		if ((pol == POLARIZATION_V) || (pol == POLARIZATION_R))
			parms->freq_offset = lnb->lowfreq * 1000;
		else
			parms->freq_offset = lnb->highfreq * 1000;
		goto ret;
	}

	/* Voltage-controlled multiband switch */
	parms->high_band = (freq > lnb->rangeswitch * 1000) ? 1 : 0;

	/* Adjust frequency */
	if (parms->high_band)
		parms->freq_offset = lnb->highfreq * 1000;
	else
		parms->freq_offset = lnb->lowfreq * 1000;

	/* For SCR/Unicable setups */
	if (parms->freq_bpf) {
		t = (((freq / 1000) + parms->freq_bpf + 2) / 4) - 350;
		parms->freq_offset += ((t + 350) * 4) * 1000;
	}

ret:
	rc = dvbsat_diseqc_set_input(parms, t);

	freq = abs(freq - parms->freq_offset);
	dvb_fe_store_parm(parms, DTV_FREQUENCY, freq);

	return rc;
}

const char *dvbsat_polarization_name[5] = {
	"OFF",
	"H",
	"V",
	"L",
	"R",
};
