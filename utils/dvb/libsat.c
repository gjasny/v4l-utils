#include <stdio.h>
#include <stdlib.h>

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

static int dvb_satellite_switch_band(struct dvb_v5_fe_parms *parms)
{
	/* FIXME: Add diseqc code */
//	usleep(50000);
	return 0;
}

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