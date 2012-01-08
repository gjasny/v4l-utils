#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "dvb-fe.h"

struct dvb_satellite_lnb lnb[] = {
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

int search_lnb(char *name)
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
	if (i >= ARRAY_SIZE(lnb))
		return -1;

	printf("%s\n\t%s\n", lnb[i].alias, lnb[i].name);
	printf("\t%d to %d MHz",
	       lnb[i].freqrange[0].low, lnb[i].freqrange[0].high);
	if (lnb[i].freqrange[1].low)
		printf(" and %d to %d MHz",
		       lnb[i].freqrange[1].low, lnb[i].freqrange[1].high);
	printf("\n\t%s LO, ", lnb[i].rangeswitch ? "Single" : "Dual");
	if (!lnb[i].highfreq) {
		printf("IF = %d MHz\n", lnb[i].lowfreq);
		return 0;
	}
	if (!lnb[i].rangeswitch) {
		printf("IF = %d/%d MHz\n", lnb[i].lowfreq, lnb[i].highfreq);
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

struct dvb_satellite_lnb *get_lnb(int i)
{
	if (i >= ARRAY_SIZE(lnb))
		return NULL;

	return &lnb[i];
}

/*
 * DVB satellite Diseqc specifics
 */

static int dvbsat_diseqc_send_msg(struct dvb_v5_fe_parms *parms,
				   int vol_on,
				   int vol_high,
				   int tone_on,
				   int mini_a,
				   int wait,
				   char *cmd,
				   int len)
{
	int rc;

	rc = dvb_fe_sec_tone(parms, SEC_TONE_OFF);
	if (rc)
		return rc;
	rc = dvb_fe_sec_voltage(parms, vol_on, vol_high);
	if (rc)
		return rc;
	usleep(15 * 1000);
	rc = dvb_fe_diseqc_cmd(parms, len, cmd);
	if (rc)
		return rc;
	usleep(wait * 1000);
	usleep(15 * 1000);
	rc = dvb_fe_diseqc_burst(parms, mini_a);
	if (rc)
		return rc;
	usleep(15 * 1000);
	rc = dvb_fe_sec_tone(parms, tone_on);

	return rc;
}

static int dvb_satellite_switch_band(struct dvb_v5_fe_parms *parms)
{
	char cmd[] = {0xe0, 0x10, 0x38, 0xf0, 0x00, 0x00 };
	char *p = &cmd[3];
        enum polarization pol = parms->pol;
	int is_pol_v = (pol == POLARIZATION_V) || (pol == POLARIZATION_R);

	*p |= (parms->sat_number << 2) & 0x0f;
	*p |= parms->high_band;
	*p |= ((pol == POLARIZATION_V) || (pol == POLARIZATION_R)) ? 0 : 2;

	dvbsat_diseqc_send_msg(parms, 1, !is_pol_v, parms->high_band,
			       !(parms->sat_number % 2),
			       4, cmd, ARRAY_SIZE(cmd));

	return 0;
}

/*
 * DVB satellite get/set params hooks
 */


int dvb_satellite_set_parms(struct dvb_v5_fe_parms *parms)
{
	struct dvb_satellite_lnb *lnb = parms->lnb;
        enum polarization pol = parms->pol;
	uint32_t freq;
	uint32_t voltage = SEC_VOLTAGE_13;

	dvb_fe_retrieve_parm(parms, DTV_FREQUENCY, &freq);

	if (!lnb) {
		fprintf(stderr, "Need a LNBf to work\n");
		return -EINVAL;
	}

	/* Simple case: LNBf with just Single LO */
	if (!lnb->highfreq) {
		freq = abs(freq - lnb->lowfreq);
		goto ret;
	}

	/* polarization-controlled multi LNBf */
	if (!lnb->rangeswitch) {
		if ((pol == POLARIZATION_V) || (pol == POLARIZATION_R))
			freq = abs(freq - lnb->lowfreq);
		else
			freq = abs(freq - lnb->highfreq);
		goto ret;
	}

	/* Voltage-controlled multiband switch */
	parms->high_band = (freq > lnb->rangeswitch) ? 1 : 0;

	dvb_satellite_switch_band(parms);

	/* Adjust frequency */
	if (parms->high_band)
		freq = abs(freq - lnb->highfreq);
	else
		freq = abs(freq - lnb->lowfreq);

ret:
	dvb_fe_store_parm(parms, DTV_FREQUENCY, voltage);
	dvb_fe_store_parm(parms, DTV_FREQUENCY, freq);
	return 0;
}

int dvb_satellite_get_parms(struct dvb_v5_fe_parms *parms)
{
	struct dvb_satellite_lnb *lnb = parms->lnb;
        enum polarization pol = parms->pol;
	uint32_t freq;

	dvb_fe_retrieve_parm(parms, DTV_FREQUENCY, &freq);

	if (!lnb) {
		fprintf(stderr, "Need a LNBf to work\n");
		return -EINVAL;
	}

	/* Simple case: LNBf with just Single LO */
	if (!lnb->highfreq) {
		freq = abs(freq + lnb->lowfreq);
		goto ret;
	}

	/* polarization-controlled multi LNBf */
	if (!lnb->rangeswitch) {
		if ((pol == POLARIZATION_V) || (pol == POLARIZATION_R))
			freq = abs(freq + lnb->lowfreq);
		else
			freq = abs(freq + lnb->highfreq);
		goto ret;
	}

	/* Voltage-controlled multiband switch */
	if (parms->high_band)
		freq = abs(freq + lnb->highfreq);
	else
		freq = abs(freq + lnb->lowfreq);

ret:
	dvb_fe_store_parm(parms, DTV_FREQUENCY, freq);

	return 0;
}