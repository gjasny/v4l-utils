/*
 * Copyright (c) 2011-2016 - Mauro Carvalho Chehab
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <strings.h> /* strcasecmp */

#include "dvb-fe-priv.h"
#include <libdvbv5/dvb-v5-std.h>

#include <config.h>

#ifdef ENABLE_NLS
# include "gettext.h"
# include <libintl.h>
# define _(string) dgettext(LIBDVBV5_DOMAIN, string)

#else
# define _(string) string
#endif

# define N_(string) string

struct dvbsat_freqrange_priv {
	unsigned low, high, int_freq, rangeswitch;
	enum dvb_sat_polarization pol;
};

struct dvb_sat_lnb_priv {
	struct dvb_sat_lnb desc;

	/* Private members used internally */
	struct dvbsat_freqrange_priv freqrange[4];
};

static const struct dvb_sat_lnb_priv lnb_array[] = {
	{
		.desc = {
			.name = N_("Astra 19.2E, European Universal Ku (extended)"),
			.alias = "EXTENDED",
			// Legacy fields - kept just to avoid API/ABI breakages
			.lowfreq = 9750,
			.highfreq = 10600,
			.rangeswitch = 11700,
			.freqrange = {
				{ 10700, 11700 },
				{ 11700, 12750 },
			},
		},
		.freqrange = {
			{ 10700, 11700, 9750, 11700},
			{ 11700, 12750, 10600, 0 },
		}
	}, {
		.desc = {
			.name = N_("Old European Universal. Nowadays mostly replaced by Astra 19.2E"),
			.alias = "UNIVERSAL",
			// Legacy fields - kept just to avoid API/ABI breakages
			.lowfreq = 9750,
			.highfreq = 10600,
			.rangeswitch = 11700,
			.freqrange = {
				{ 10800, 11800 },
				{ 11600, 12700 },
			},
		},
		.freqrange = {
			{ 10800, 11800, 9750, 11700 },
			{ 11600, 12700, 10600, 0 },
		}
	}, {
		.desc = {
			.name = N_("Expressvu, North America"),
			.alias = "DBS",
			// Legacy fields - kept just to avoid API/ABI breakages
			.lowfreq = 11250,
			.freqrange = {
				{ 12200, 12700 }
			},
		},
		.freqrange = {
			{ 12200, 12700, 11250 }
		}
	}, {
		.desc = {
			.name = N_("Standard"),
			.alias = "STANDARD",
			// Legacy fields - kept just to avoid API/ABI breakages
			.lowfreq = 10000,
			.freqrange = {
				{ 10945, 11450 }
			},
		},
		.freqrange = {
			{ 10945, 11450, 10000, 0 }
		},
	}, {
		.desc = {
			.name = N_("L10700"),
			.alias = "L10700",
			// Legacy fields - kept just to avoid API/ABI breakages
			.lowfreq = 10700,
			.freqrange = {
				{ 11750, 12750 }
			},
		},
		.freqrange = {
		       { 11750, 12750, 10700, 0 }
		},
	}, {
		.desc = {
			.name = N_("L10750"),
			.alias = "L10750",
			// Legacy fields - kept just to avoid API/ABI breakages
			.lowfreq = 10750,
			.freqrange = {
				{ 11700, 12200 }
			},
		},
		.freqrange = {
		       { 11700, 12200, 10750, 0 }
		},
	}, {
		.desc = {
			.name = N_("L11300"),
			.alias = "L11300",
			// Legacy fields - kept just to avoid API/ABI breakages
			.lowfreq = 11300,
			.freqrange = {
				{ 12250, 12750 }
			},
		},
		.freqrange = {
			{ 12250, 12750, 11300, 0 }
		},
	}, {
		.desc = {
			.name = N_("Astra"),
			.alias = "ENHANCED",
			// Legacy fields - kept just to avoid API/ABI breakages
			.lowfreq = 9750,
			.freqrange = {
				{ 10700, 11700 }
			},
		},
		.freqrange = {
			{ 10700, 11700, 9750, 0 }
		},
	}, {
		.desc = {
			.name = N_("Invacom QPH-031"),
			.alias = "QPH031",
			// Legacy fields - kept just to avoid API/ABI breakages
			.lowfreq = 10750,
			.highfreq = 11250,
			.rangeswitch = 12200,
			.freqrange = {
				{ 11700, 12200 },
				{ 12200, 12700 },
			},
		},
		// Note: This LNBf can accept both V/H and L/R polarization
		// on ports 1 and 3, V is 12V and H is 19V
		// on ports 2 and 4, R is 12V and L is 19V
		// This is the same as what's done for Universal LNBf, so,
		// we don't need any special logic here to handle this special
		// case.
		.freqrange = {
			{ 11700, 12200, 10750, 12200  },
			{ 12200, 12700, 11250, 0  },
		},
	}, {
		.desc = {
			.name = N_("Big Dish - Monopoint LNBf"),
			.alias = "C-BAND",
			// Legacy fields - kept just to avoid API/ABI breakages
			.lowfreq = 5150,
			.freqrange = {
				{ 3700, 4200 }
			},
		},
		.freqrange = {
			{ 3700, 4200, 5150, 0 }
		},
	}, {
		.desc = {
			.name = N_("Big Dish - Multipoint LNBf"),
			.alias = "C-MULT",
			// Legacy fields - kept just to avoid API/ABI breakages
			.lowfreq = 5150,
			.highfreq = 5750,
			.freqrange = {
				{ 3700, 4200 }
			},
		},
		.freqrange = {
			{ 3700, 4200, 5150, 0, POLARIZATION_R },
			{ 3700, 4200, 5750, 0, POLARIZATION_L }
		},
	}, {
		.desc = {
			.name = N_("DishPro LNBf"),
			.alias = "DISHPRO",
			// Legacy fields - kept just to avoid API/ABI breakages
			.lowfreq = 11250,
			.highfreq = 14350,
			.freqrange = {
				{ 12200, 12700 }
			},
		},
		.freqrange = {
			{ 12200, 12700, 11250, 0, POLARIZATION_V },
			{ 12200, 12700, 14350, 0, POLARIZATION_H }
		}
	}, {
		.desc = {
			.name = N_("Japan 110BS/CS LNBf"),
			.alias = "110BS",
			// Legacy fields - kept just to avoid API/ABI breakages
			.lowfreq = 10678,
			.freqrange = {
				{ 11710, 12751 }
			},
		},
		.freqrange = {
			{ 11710, 12751, 10678, 0 }
		}
	}, {
		.desc = {
			.name = N_("BrasilSat Stacked"),
			.alias = "STACKED-BRASILSAT",
			// Legacy fields - kept just to avoid API/ABI breakages
			.lowfreq = 9710,
			.highfreq = 9750,
			.freqrange = {
				{ 10700, 11700 },
			},
		},
		.freqrange = {
			{ 10700, 11700, 9710, 0, POLARIZATION_H },
			{ 10700, 11700, 9750, 0, POLARIZATION_H },
		},
	}, {
		.desc = {
			.name = N_("BrasilSat Oi"),
			.alias = "OI-BRASILSAT",
			// Legacy fields - kept just to avoid API/ABI breakages
			.lowfreq = 10000,
			.highfreq = 10445,
			.rangeswitch = 11700,
			.freqrange = {
				{ 10950, 11200 },
				{ 11800, 12200 },
			},
		},
		.freqrange = {
			{ 10950, 11200, 10000, 11700 },
			{ 11800, 12200, 10445, 0 },
		}
	}, {
		.desc = {
			.name = N_("BrasilSat Amazonas 1/2 - 3 Oscilators"),
			.alias = "AMAZONAS",
			// No legacy fields - as old API doesn't allow 3 LO
		},
		.freqrange = {
			{ 11037, 11450, 9670, 0, POLARIZATION_V },
			{ 11770, 12070, 9922, 0, POLARIZATION_H },
			{ 10950, 11280, 10000, 0, POLARIZATION_H },
		},
	}, {
		.desc = {
			.name = N_("BrasilSat Amazonas 1/2 - 2 Oscilators"),
			.alias = "AMAZONAS",
			// No legacy fields - as old API doesn't allow 3 ranges
		},
		.freqrange = {
			{ 11037, 11360, 9670, 0, POLARIZATION_V },
			{ 11780, 12150, 10000, 0, POLARIZATION_H },
			{ 10950, 11280, 10000, 0, POLARIZATION_H },
		},
	}, {
		.desc = {
			.name = N_("BrasilSat custom GVT"),
			.alias = "GVT-BRASILSAT",
			// No legacy fields - as old API doesn't allow 4 LO
		},
		.freqrange = {
			{ 11010.5, 11067.5, 12860, 0, POLARIZATION_V },
			{ 11704.0, 11941.0, 13435, 0, POLARIZATION_V },
			{ 10962.5, 11199.5, 13112, 0, POLARIZATION_H },
			{ 11704.0, 12188.0, 13138, 0, POLARIZATION_H },
		},
	},
};

int dvb_sat_search_lnb(const char *name)
{
	int i = 0;

	for (i = 0; i < ARRAY_SIZE(lnb_array); i++) {
		if (!strcasecmp(name, lnb_array[i].desc.alias))
			return i;
	}
	return -1;
}

static char *pol_name[] = {
	[POLARIZATION_OFF] = N_("Freqs     : "),
	[POLARIZATION_H]   = N_("Horizontal: "),
	[POLARIZATION_V]   = N_("Vertical  : "),
	[POLARIZATION_L]   = N_("Left      : "),
	[POLARIZATION_R]   = N_("Right     : "),
};

int dvb_print_lnb(int i)
{
	int j;

	if (i < 0 || i >= ARRAY_SIZE(lnb_array))
		return -1;

	printf("%s\n\t%s%s\n", lnb_array[i].desc.alias, dvb_sat_get_lnb_name(i),
	       lnb_array[i].freqrange[0].pol ? _(" (bandstacking)") : "");

	for (j = 0; j < ARRAY_SIZE(lnb_array[i].freqrange) && lnb_array[i].freqrange[j].low; j++) {
		printf(_("\t%s%d to %d MHz, LO: %d MHz\n"),
			_(pol_name[lnb_array[i].freqrange[j].pol]),
			lnb_array[i].freqrange[j].low,
			lnb_array[i].freqrange[j].high,
			lnb_array[i].freqrange[j].int_freq);
	}

	return 0;
}

void dvb_print_all_lnb(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(lnb_array); i++) {
		dvb_print_lnb(i);
		printf("\n");
	}
}

const struct dvb_sat_lnb *dvb_sat_get_lnb(int i)
{
	if (i < 0 || i >= ARRAY_SIZE(lnb_array))
		return NULL;

	return (void *)&lnb_array[i];
}

const char *dvb_sat_get_lnb_name(int i)
{
	if (i < 0 || i >= ARRAY_SIZE(lnb_array))
		return NULL;

	return _(lnb_array[i].desc.name);
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
static int dvbsat_diseqc_write_to_port_group(struct dvb_v5_fe_parms_priv *parms,
					     struct diseqc_cmd *cmd,
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
	cmd->data0 |= (sat_number & 0x3) << 2;

	return dvb_fe_diseqc_cmd(&parms->p, cmd->len, cmd->msg);
}

static int dvbsat_scr_odu_channel_change(struct dvb_v5_fe_parms_priv *parms,
					 struct diseqc_cmd *cmd,
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
	cmd->data0 |= (sat_number & 0x7) << 5;
	pos_b =  (sat_number & 0x8) ? 1 : 0;

	/* Fill the LNB number */
	cmd->data0 |= high_band ? 0 : 4;
	cmd->data0 |= pol_v ? 8 : 0;
	cmd->data0 |= pos_b ? 16 : 0;

	return dvb_fe_diseqc_cmd(&parms->p, cmd->len, cmd->msg);
}

static int dvbsat_diseqc_set_input(struct dvb_v5_fe_parms_priv *parms,
				   uint16_t t)
{
	int rc;
	enum dvb_sat_polarization pol;
	int pol_v;
	int high_band = parms->high_band;
	int sat_number = parms->p.sat_number;
	int vol_high = 0;
	int tone_on = 0;
	struct diseqc_cmd cmd;
	const struct dvb_sat_lnb_priv *lnb = (void *)parms->p.lnb;

	if (sat_number < 0 && t) {
		dvb_logwarn(_("DiSEqC disabled. Can't tune using SCR/Unicable."));
		return 0;
	}

	dvb_fe_retrieve_parm(&parms->p, DTV_POLARIZATION, &pol);
	pol_v = (pol == POLARIZATION_V) || (pol == POLARIZATION_R);

	if (!lnb->freqrange[0].rangeswitch) {
		/*
		 * Bandstacking switches don't use 2 bands nor use
		 * DISEqC for setting the polarization. It also doesn't
		 * use any tone/tone burst
		 */
		pol_v = 0;
		high_band = 1;
		if (parms->p.current_sys == SYS_ISDBS)
			vol_high = 1;
	} else {
		/* Adjust voltage/tone accordingly */
		if (sat_number < 2) {
			vol_high = pol_v ? 0 : 1;
			tone_on = high_band;
		}
	}

	rc = dvb_fe_sec_voltage(&parms->p, 1, vol_high);
	if (rc)
		return rc;

	rc = dvb_fe_sec_tone(&parms->p, SEC_TONE_OFF);
	if (rc)
		return rc;

	if (sat_number >= 0) {
		/* DiSEqC is enabled. Send DiSEqC commands */
		usleep(15 * 1000);

		if (!t)
			rc = dvbsat_diseqc_write_to_port_group(parms, &cmd, high_band,
								pol_v, sat_number);
		else
			rc = dvbsat_scr_odu_channel_change(parms, &cmd, high_band,
								pol_v, sat_number, t);

		if (rc) {
			dvb_logerr(_("sending diseq failed"));
			return rc;
		}
		usleep((15 + parms->p.diseqc_wait) * 1000);

		/* miniDiSEqC/Toneburst commands are defined only for up to 2 sattelites */
		if (parms->p.sat_number < 2) {
			rc = dvb_fe_diseqc_burst(&parms->p, parms->p.sat_number);
			if (rc)
				return rc;
		}
		usleep(15 * 1000);
	}

	rc = dvb_fe_sec_tone(&parms->p, tone_on ? SEC_TONE_ON : SEC_TONE_OFF);

	return rc;
}

int dvb_sat_real_freq(struct dvb_v5_fe_parms *p, int freq)
{
	struct dvb_v5_fe_parms_priv *parms = (void *)p;
	const struct dvb_sat_lnb_priv *lnb = (void *)p->lnb;
	int new_freq, i;

	if (!lnb || !dvb_fe_is_satellite(p->current_sys))
		return freq;

	new_freq = freq + parms->freq_offset;

	for (i = 0; i < ARRAY_SIZE(lnb->freqrange) && lnb->freqrange[i].low; i++) {
		if (new_freq / 1000 < lnb->freqrange[i].low  || new_freq / 1000 > lnb->freqrange[i].high)
			continue;
		return new_freq;
	}

	/* Weird: frequency is out of LNBf range */
	dvb_logerr(_("frequency %.2fMHz (tune freq %.2fMHz) is out of LNBf %s range"),
		   new_freq/ 1000., freq / 1000., lnb->desc.name);
	return 0;
}


/*
 * DVB satellite get/set params hooks
 */


static int dvb_sat_get_freq(struct dvb_v5_fe_parms *p, uint16_t *t)
{
	struct dvb_v5_fe_parms_priv *parms = (void *)p;
	const struct dvb_sat_lnb_priv *lnb = (void *)p->lnb;
	enum dvb_sat_polarization pol;
	uint32_t freq;
	int j;

	if (!lnb) {
		dvb_logerr(_("Need a LNBf to work"));
		return 0;
	}

	parms->high_band = 0;
	parms->freq_offset = 0;

	dvb_fe_retrieve_parm(&parms->p, DTV_FREQUENCY, &freq);

	if (!lnb->freqrange[1].low) {
		if (parms->p.verbose)
			dvb_log("LNBf with a single LO at %.2f MHz", parms->freq_offset/1000.);

		/* Trivial case: LNBf with a single local oscilator(LO) */
		parms->freq_offset = lnb->freqrange[0].int_freq * 1000;
		return freq;
	}

	if (lnb->freqrange[0].pol) {
		if (parms->p.verbose > 1)
			dvb_log("LNBf polarity driven");

		/* polarization-controlled multi-LO multipoint LNBf (bandstacking) */
		dvb_fe_retrieve_parm(&parms->p, DTV_POLARIZATION, &pol);

		for (j = 0; j < ARRAY_SIZE(lnb->freqrange) && lnb->freqrange[j].low; j++) {
			if (freq < lnb->freqrange[j].low * 1000 ||
			    freq > lnb->freqrange[j].high * 1000 ||
			    pol != lnb->freqrange[j].pol)
				continue;

			parms->freq_offset = lnb->freqrange[j].int_freq * 1000;
			return freq;
		}
	} else {
		if (parms->p.verbose > 1)
			dvb_log("Seeking for LO for %.2f MHz frequency", freq / 1000000.);
		/* Multi-LO (dual-band) LNBf using DiSEqC */
		for (j = 0; j < ARRAY_SIZE(lnb->freqrange) && lnb->freqrange[j].low; j++) {
			if (parms->p.verbose > 1)
				dvb_log("LO setting %i: %.2f MHz to %.2f MHz", j,
					lnb->freqrange[j].low / 1000., lnb->freqrange[j].high / 1000.);

			if (freq < lnb->freqrange[j].low * 1000 || freq > lnb->freqrange[j].high * 1000)
				continue;
			if (lnb->freqrange[j].rangeswitch && freq > lnb->freqrange[j].rangeswitch * 1000) {
				if (j + 1 < ARRAY_SIZE(lnb->freqrange) && lnb->freqrange[j + 1].low)
					j++;
			}

			/* Sets DiSEqC to high_band if not low band */
			if (j)
				parms->high_band = 1;

			if (parms->p.freq_bpf) {
				/* For SCR/Unicable setups */
				*t = (((freq / 1000) + parms->p.freq_bpf + 2) / 4) - 350;
				parms->freq_offset += ((*t + 350) * 4) * 1000;
				if (parms->p.verbose)
					dvb_log("BPF: %d KHz", parms->p.freq_bpf);
			} else {
				parms->freq_offset = lnb->freqrange[j].int_freq * 1000;
				if (parms->p.verbose > 1)
					dvb_log("Multi-LO LNBf. using LO setting %i at %.2f MHz", j, parms->freq_offset / 1000.);
			}
			return freq;
		}
	}
	dvb_logerr("frequency: %.2f MHz is out of LNBf range\n",
		   freq / 1000.);
	return 0;
}

int dvb_sat_set_parms(struct dvb_v5_fe_parms *p)
{
	struct dvb_v5_fe_parms_priv *parms = (void *)p;
	uint32_t freq;
	uint16_t t = 0;
	int rc;

	freq = dvb_sat_get_freq(p, &t);
	if (!freq)
		return -EINVAL;

	if (parms->p.verbose)
		dvb_log("frequency: %.2f MHz, high_band: %d", freq / 1000., parms->high_band);

	rc = dvbsat_diseqc_set_input(parms, t);

	freq = abs(freq - parms->freq_offset);

	if (parms->p.verbose)
		dvb_log("L-Band frequency: %.2f MHz (offset = %.2f MHz)", freq / 1000., parms->freq_offset/1000.);

	dvb_fe_store_parm(&parms->p, DTV_FREQUENCY, freq);

	return rc;
}

