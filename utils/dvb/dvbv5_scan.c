/*
 * Copyright (c) 2011 - Mauro Carvalho Chehab <mchehab@redhat.com>
 */

#include "dvb-v5.h"
#include "dvb-v5-std.h"
#define _GNU_SOURCE
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <malloc.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <string.h>

#define ARRAY_SIZE(x)	(sizeof(x)/sizeof((x)[0]))

struct dvb_v5_fe_parms {
	int				fd;
	char				*fname;
	unsigned			verbose;
	struct dvb_frontend_info	info;
	uint32_t			version;
	fe_delivery_system_t		current_sys;
	int				num_systems;
	fe_delivery_system_t		systems[ARRAY_SIZE(dvb_v5_delivery_system)];
	int				n_props;
	struct dtv_property		dvb_prop[DTV_MAX_COMMAND];
	int				legacy_fe;
};

static void dvb_v5_free(struct dvb_v5_fe_parms *parms)
{
	if (parms->fname)
		free(parms->fname);

	free(parms);
}

static struct dvb_v5_fe_parms *dvb_fe_open(int adapter, int frontend, unsigned verbose)
{
	int fd, i;
	char *fname;
	struct dtv_properties dtv_prop;
	struct dvb_v5_fe_parms *parms = NULL;

	asprintf(&fname, "/dev/dvb/adapter%i/frontend%i", adapter, frontend);
	if (!fname) {
		perror("fname malloc");
		return NULL;
	}

	fd = open(fname, O_RDWR, 0);
	if (fd == -1) {
		fprintf(stderr, "%s while opening %s\n", strerror(errno), fname);
		return NULL;
	}
	parms = calloc(sizeof(*parms), 1);
	if (!parms) {
		perror("parms calloc");
		return NULL;
	}
	parms->fname = fname;
	parms->verbose = verbose;

	if (ioctl(fd, FE_GET_INFO, &parms->info)) {
		perror("FE_GET_INFO");
		dvb_v5_free(parms);
		return NULL;
	}

	if (verbose) {
		fe_caps_t caps = parms->info.caps;

		printf("Device %s (%s) capabilities:\n\t",
			parms->info.name, fname);
		for (i = 0; i < ARRAY_SIZE(fe_caps_name); i++) {
			if (caps & fe_caps_name[i].idx)
				printf ("%s ", fe_caps_name[i].name);
		}
		printf("\n");
	}

	parms->dvb_prop[0].cmd = DTV_API_VERSION;
	parms->dvb_prop[1].cmd = DTV_DELIVERY_SYSTEM;

	dtv_prop.num = 2;
	dtv_prop.props = parms->dvb_prop;

	/* Detect a DVBv3 device */
	if (ioctl(fd, FE_GET_PROPERTY, &dtv_prop) == -1) {
		parms->dvb_prop[0].u.data = 0x300;
		parms->dvb_prop[1].u.data = SYS_UNDEFINED;
	}
	parms->version = parms->dvb_prop[0].u.data;
	parms->current_sys = parms->dvb_prop[1].u.data;
	if (verbose)
		printf ("DVB API Version %d.%d, Current v5 delivery system: %s\n",
			parms->version / 256,
			parms->version % 256,
			delivery_system_name[parms->current_sys]);

	if (parms->current_sys == SYS_UNDEFINED) {
		parms->legacy_fe = 1;
		switch(parms->info.type) {
		case FE_QPSK:
			parms->current_sys = SYS_DVBS;
			parms->systems[parms->num_systems++] = parms->current_sys;
			if (parms->info.caps & FE_CAN_2G_MODULATION) {
				parms->systems[parms->num_systems++] = SYS_DVBS2;
			}
			break;
		case FE_QAM:
			parms->current_sys = SYS_DVBC_ANNEX_AC;
			parms->systems[parms->num_systems++] = parms->current_sys;
			break;
		case FE_OFDM:
			parms->current_sys = SYS_DVBT;
			parms->systems[parms->num_systems++] = parms->current_sys;
			if (parms->info.caps & FE_CAN_2G_MODULATION) {
				parms->systems[parms->num_systems++] = SYS_DVBT2;
			}
			break;
		case FE_ATSC:
			if (parms->info.caps & (FE_CAN_8VSB | FE_CAN_16VSB))
				parms->systems[parms->num_systems++] = SYS_ATSC;
			if (parms->info.caps & (FE_CAN_QAM_64 | FE_CAN_QAM_256 | FE_CAN_QAM_AUTO))
				parms->systems[parms->num_systems++] = SYS_DVBC_ANNEX_B;
			parms->current_sys = parms->systems[0];
			break;
		}
		if (!parms->num_systems) {
			fprintf(stderr, "delivery system not detected\n");
			dvb_v5_free(parms);
			return NULL;
		}
	} else {
		for (i = 0; i < ARRAY_SIZE(dvb_v5_delivery_system); i++) {
			if (!dvb_v5_delivery_system[i])
				continue;

			dtv_prop.num = 1;
			parms->dvb_prop[0].cmd = DTV_DELIVERY_SYSTEM;
			parms->dvb_prop[0].u.data = i;
			if (ioctl(fd, FE_SET_PROPERTY, &dtv_prop) == -1)
				continue;
			if (ioctl(fd, FE_GET_PROPERTY, &dtv_prop) == -1)
				continue;
			if (parms->dvb_prop[0].u.data == i)
				parms->systems[parms->num_systems++] = i;
		}
		if (parms->num_systems == 0) {
			fprintf(stderr, "driver died while trying to set the delivery system\n");
			dvb_v5_free(parms);
			return NULL;
		}
		if (parms->num_systems == 1 && parms->systems[0] != parms->current_sys) {
			fprintf(stderr, "failed to detect all delivery systems\n");
			dvb_v5_free(parms);
			return NULL;
		} else {
			/* Return back to the default delivery system */
			dtv_prop.num = 1;
			parms->dvb_prop[0].cmd = DTV_DELIVERY_SYSTEM;
			parms->dvb_prop[0].u.data = parms->current_sys;
			if (ioctl(fd, FE_SET_PROPERTY, &dtv_prop) == -1) {
				fprintf(stderr, "failed to return to default delivery system\n");
				dvb_v5_free(parms);
				return NULL;
			}
			if (parms->dvb_prop[0].u.data == i)
				parms->systems[parms->num_systems++] = i;
		}
	}

	if (verbose) {
		printf("Supported delivery system%s: ",
		       (parms->num_systems > 1) ? "s" : "");
		for (i = 0; i < parms->num_systems; i++) {
			if (parms->systems[i] == parms->current_sys)
				printf ("[%s] ",
					delivery_system_name[parms->systems[i]]);
			else
				printf ("%s ",
					delivery_system_name[parms->systems[i]]);
		}
		printf("\n");
	}

	return parms;
}

static void dvb_fe_close(struct dvb_v5_fe_parms *parms)
{
	if (!parms)
		return;


	if (parms->fd < 0)
		return;
}

int main(void)
{
	struct dvb_v5_fe_parms *parms;

	parms = dvb_fe_open(0, 0, 1);
	dvb_fe_close(parms);

	return 0;
}