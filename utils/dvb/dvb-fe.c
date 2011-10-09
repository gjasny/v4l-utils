/*
 * Copyright (c) 2011 - Mauro Carvalho Chehab <mchehab@redhat.com>
 */

#include "dvb-v5.h"
#include "dvb-v5-std.h"
#include "dvb-fe.h"

static void dvb_v5_free(struct dvb_v5_fe_parms *parms)
{
	if (parms->fname)
		free(parms->fname);

	free(parms);
}

struct dvb_v5_fe_parms *dvb_fe_open(int adapter, int frontend, unsigned verbose)
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

	/* Prepare to use the delivery system */
	dvb_set_sys(parms, parms->current_sys);

	return parms;
}

void dvb_fe_close(struct dvb_v5_fe_parms *parms)
{
	if (!parms)
		return;


	if (parms->fd < 0)
		return;
}

int dvb_set_sys(struct dvb_v5_fe_parms *parms,
			  fe_delivery_system_t sys)
{
	struct dtv_property dvb_prop[1];
	struct dtv_properties prop;
	const unsigned int *sys_props;
	int n;

	if (sys != parms->current_sys) {
		/* Can't change standard with the legacy FE support */
		if (parms->legacy_fe)
			return EINVAL;

		dvb_prop[0].cmd = DTV_DELIVERY_SYSTEM;
		dvb_prop[0].u.data = sys;
		prop.num = 1;
		prop.props = dvb_prop;

		if (ioctl(parms->fd, FE_SET_PROPERTY, &prop) == -1) {
			perror("Set delivery system");
			return errno;
		}
		parms->current_sys = sys;
	}

	/* Make dvb properties reflect the current standard */

	sys_props = dvb_v5_delivery_system[parms->current_sys];
	if (!sys_props)
		return EINVAL;

	n = 0;
	while (sys_props[n]) {
		parms->dvb_prop[n].cmd = sys_props[n];
		n++;
	}
	parms->n_props = n;

	return 0;
}

void dvb_fe_prt_parms(struct dvb_v5_fe_parms *parms)
{
	int i;

	for (i = 0; i < parms->n_props; i++) {
		printf("%s = %u\n",
		       dvb_v5_name[parms->dvb_prop[i].cmd],
		       parms->dvb_prop[i].u.data);
		i++;
	}
};

int dvb_fe_retrieve_parm(struct dvb_v5_fe_parms *parms,
				unsigned cmd, uint32_t *value)
{
	int i;
	for (i = 0; i < parms->n_props; i++) {
		if (parms->dvb_prop[i].cmd != cmd)
			continue;
		*value = parms->dvb_prop[i].u.data;
		return 0;
	}
	fprintf(stderr, "%s not found on retrieve\n",
		dvb_v5_name[cmd]);

	return EINVAL;
}

int dvb_fe_store_parm(struct dvb_v5_fe_parms *parms,
			     unsigned cmd, uint32_t value)
{
	int i;
	for (i = 0; i < parms->n_props; i++) {
		if (parms->dvb_prop[i].cmd != cmd)
			continue;
		parms->dvb_prop[i].u.data = value;
		return 0;
	}
	fprintf(stderr, "%s not found on store\n",
		dvb_v5_name[cmd]);

	return EINVAL;
}

static int dvb_fe_get_parms(struct dvb_v5_fe_parms *parms)
{
	int n = 0;
	const unsigned int *sys_props;
	struct dtv_properties prop;
	struct dvb_frontend_parameters v3_parms;

	sys_props = dvb_v5_delivery_system[parms->current_sys];
	if (!sys_props)
		return EINVAL;

	while (sys_props[n]) {
		parms->dvb_prop[n].cmd = sys_props[n];
		n++;
	}
	/* Keep it ready for set */
	parms->dvb_prop[n].cmd = DTV_TUNE;
	parms->n_props = n;

	prop.props = parms->dvb_prop;
	prop.num = n;
	if (!parms->legacy_fe) {
		if (ioctl(parms->fd, FE_GET_PROPERTY, &prop) == -1) {
			perror("FE_GET_PROPERTY");
			return errno;
		}
		if (parms->verbose) {
			printf("Got parameters for %s:",
			       delivery_system_name[parms->current_sys]);
			dvb_fe_prt_parms(parms);
		}
		return 0;
	}
	/* DVBv3 call */
	if (ioctl(parms->fd, FE_GET_FRONTEND, &v3_parms) == -1) {
		perror("FE_GET_FRONTEND");
		return errno;
	}

	dvb_fe_retrieve_parm(parms, DTV_FREQUENCY, &v3_parms.frequency);
	dvb_fe_retrieve_parm(parms, DTV_INVERSION, &v3_parms.inversion);
	switch (parms->current_sys) {
	case SYS_DVBS:
		dvb_fe_retrieve_parm(parms, DTV_SYMBOL_RATE, &v3_parms.u.qpsk.symbol_rate);
		dvb_fe_retrieve_parm(parms, DTV_INNER_FEC, &v3_parms.u.qpsk.fec_inner);
		break;
	case SYS_DVBC_ANNEX_AC:
		dvb_fe_retrieve_parm(parms, DTV_SYMBOL_RATE, &v3_parms.u.qam.symbol_rate);
		dvb_fe_retrieve_parm(parms, DTV_INNER_FEC, &v3_parms.u.qam.fec_inner);
		dvb_fe_retrieve_parm(parms, DTV_MODULATION, &v3_parms.u.qam.modulation);
		break;
	case SYS_ATSC:
	case SYS_DVBC_ANNEX_B:
		dvb_fe_retrieve_parm(parms, DTV_MODULATION, &v3_parms.u.vsb.modulation);
		break;
	case SYS_DVBT:
		dvb_fe_retrieve_parm(parms, DTV_BANDWIDTH_HZ, &v3_parms.u.ofdm.bandwidth);
		dvb_fe_retrieve_parm(parms, DTV_CODE_RATE_HP, &v3_parms.u.ofdm.code_rate_HP);
		dvb_fe_retrieve_parm(parms, DTV_CODE_RATE_LP, &v3_parms.u.ofdm.code_rate_LP);
		dvb_fe_retrieve_parm(parms, DTV_MODULATION, &v3_parms.u.ofdm.constellation);
		dvb_fe_retrieve_parm(parms, DTV_TRANSMISSION_MODE, &v3_parms.u.ofdm.transmission_mode);
		dvb_fe_retrieve_parm(parms, DTV_GUARD_INTERVAL, &v3_parms.u.ofdm.guard_interval);
		dvb_fe_retrieve_parm(parms, DTV_HIERARCHY, &v3_parms.u.ofdm.hierarchy_information);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

int dvb_fe_set_parms(struct dvb_v5_fe_parms *parms)
{
	struct dtv_properties prop;
	struct dvb_frontend_parameters v3_parms;

	prop.props = parms->dvb_prop;
	prop.num = parms->n_props + 1;

	if (!parms->legacy_fe) {
		if (ioctl(parms->fd, FE_SET_PROPERTY, &prop) == -1) {
			perror("FE_SET_PROPERTY");
			return errno;
		}
	}
	/* DVBv3 call */
	dvb_fe_store_parm(parms, DTV_FREQUENCY, v3_parms.frequency);
	dvb_fe_store_parm(parms, DTV_INVERSION, v3_parms.inversion);
	switch (parms->current_sys) {
	case SYS_DVBS:
		dvb_fe_store_parm(parms, DTV_SYMBOL_RATE, v3_parms.u.qpsk.symbol_rate);
		dvb_fe_store_parm(parms, DTV_INNER_FEC, v3_parms.u.qpsk.fec_inner);
		break;
	case SYS_DVBC_ANNEX_AC:
		dvb_fe_store_parm(parms, DTV_SYMBOL_RATE, v3_parms.u.qam.symbol_rate);
		dvb_fe_store_parm(parms, DTV_INNER_FEC, v3_parms.u.qam.fec_inner);
		dvb_fe_store_parm(parms, DTV_MODULATION, v3_parms.u.qam.modulation);
		break;
	case SYS_ATSC:
	case SYS_DVBC_ANNEX_B:
		dvb_fe_store_parm(parms, DTV_MODULATION, v3_parms.u.vsb.modulation);
		break;
	case SYS_DVBT:
		dvb_fe_store_parm(parms, DTV_BANDWIDTH_HZ, v3_parms.u.ofdm.bandwidth);
		dvb_fe_store_parm(parms, DTV_CODE_RATE_HP, v3_parms.u.ofdm.code_rate_HP);
		dvb_fe_store_parm(parms, DTV_CODE_RATE_LP, v3_parms.u.ofdm.code_rate_LP);
		dvb_fe_store_parm(parms, DTV_MODULATION, v3_parms.u.ofdm.constellation);
		dvb_fe_store_parm(parms, DTV_TRANSMISSION_MODE, v3_parms.u.ofdm.transmission_mode);
		dvb_fe_store_parm(parms, DTV_GUARD_INTERVAL, v3_parms.u.ofdm.guard_interval);
		dvb_fe_store_parm(parms, DTV_HIERARCHY, v3_parms.u.ofdm.hierarchy_information);
		break;
	default:
		return -EINVAL;
	}
	if (ioctl(parms->fd, FE_SET_FRONTEND, &v3_parms) == -1) {
		perror("FE_SET_FRONTEND");
		return errno;
	}
	return 0;
}

int dvb_fe_get_status(struct dvb_v5_fe_parms *parms)
{
	fe_status_t status;
	int i;

	if (!ioctl(parms->fd, FE_READ_STATUS, &status) == -1) {
		perror("FE_READ_STATUS");
		return -1;
	}
	if (parms->verbose > 1) {
		printf("Status: ");
		for (i = 0; i < ARRAY_SIZE(fe_status_name); i++) {
			if (status & fe_status_name[i].idx)
				printf ("%s ", fe_status_name[i].name);
		}
		printf("\n");
	}
	parms->last_status = status;
	return status;
}

int dvb_fe_get_event(struct dvb_v5_fe_parms *parms)
{
	struct dvb_frontend_event event;
	fe_status_t status;
	int i;

	if (!parms->legacy_fe) {
		dvb_fe_get_parms(parms);
		return dvb_fe_get_status(parms);
	}

	if (!ioctl(parms->fd, FE_GET_EVENT, &event) == -1) {
		perror("FE_GET_EVENT");
		return -1;
	}
	status = event.status;
	if (parms->verbose > 1) {
		printf("Status: ");
		for (i = 0; i < ARRAY_SIZE(fe_status_name); i++) {
			if (status & fe_status_name[i].idx)
				printf ("%s ", fe_status_name[i].name);
		}
		printf("\n");
	}
	parms->last_status = status;

	dvb_fe_retrieve_parm(parms, DTV_FREQUENCY, &event.parameters.frequency);
	dvb_fe_retrieve_parm(parms, DTV_INVERSION, &event.parameters.inversion);
	switch (parms->current_sys) {
	case SYS_DVBS:
		dvb_fe_retrieve_parm(parms, DTV_SYMBOL_RATE, &event.parameters.u.qpsk.symbol_rate);
		dvb_fe_retrieve_parm(parms, DTV_INNER_FEC, &event.parameters.u.qpsk.fec_inner);
		break;
	case SYS_DVBC_ANNEX_AC:
		dvb_fe_retrieve_parm(parms, DTV_SYMBOL_RATE, &event.parameters.u.qam.symbol_rate);
		dvb_fe_retrieve_parm(parms, DTV_INNER_FEC, &event.parameters.u.qam.fec_inner);
		dvb_fe_retrieve_parm(parms, DTV_MODULATION, &event.parameters.u.qam.modulation);
		break;
	case SYS_ATSC:
	case SYS_DVBC_ANNEX_B:
		dvb_fe_retrieve_parm(parms, DTV_MODULATION, &event.parameters.u.vsb.modulation);
		break;
	case SYS_DVBT:
		dvb_fe_retrieve_parm(parms, DTV_BANDWIDTH_HZ, &event.parameters.u.ofdm.bandwidth);
		dvb_fe_retrieve_parm(parms, DTV_CODE_RATE_HP, &event.parameters.u.ofdm.code_rate_HP);
		dvb_fe_retrieve_parm(parms, DTV_CODE_RATE_LP, &event.parameters.u.ofdm.code_rate_LP);
		dvb_fe_retrieve_parm(parms, DTV_MODULATION, &event.parameters.u.ofdm.constellation);
		dvb_fe_retrieve_parm(parms, DTV_TRANSMISSION_MODE, &event.parameters.u.ofdm.transmission_mode);
		dvb_fe_retrieve_parm(parms, DTV_GUARD_INTERVAL, &event.parameters.u.ofdm.guard_interval);
		dvb_fe_retrieve_parm(parms, DTV_HIERARCHY, &event.parameters.u.ofdm.hierarchy_information);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}
