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
#include <sys/types.h>

#include "dvb-v5.h"
#include "dvb-v5-std.h"
#include "dvb-fe.h"

#include <inttypes.h>
#include <math.h>
#include <stddef.h>
#include <unistd.h>

static void dvb_v5_free(struct dvb_v5_fe_parms *parms)
{
	if (parms->fname)
		free(parms->fname);

	free(parms);
}

struct dvb_v5_fe_parms *dvb_fe_open(int adapter, int frontend, unsigned verbose,
				    unsigned use_legacy_call)
{
  return dvb_fe_open2(adapter, frontend, verbose, use_legacy_call,
		      dvb_default_log);
}

struct dvb_v5_fe_parms *dvb_fe_open2(int adapter, int frontend, unsigned verbose,
				    unsigned use_legacy_call, dvb_logfunc logfunc)
{
	int fd, i;
	char *fname;
	struct dtv_properties dtv_prop;
	struct dvb_v5_fe_parms *parms = NULL;

	asprintf(&fname, "/dev/dvb/adapter%i/frontend%i", adapter, frontend);
	if (!fname) {
		logfunc(LOG_ERR, "fname calloc: %s", strerror(errno));
		return NULL;
	}

	fd = open(fname, O_RDWR, 0);
	if (fd == -1) {
		logfunc(LOG_ERR, "%s while opening %s", strerror(errno), fname);
		return NULL;
	}
	parms = calloc(sizeof(*parms), 1);
	if (!parms) {
		logfunc(LOG_ERR, "parms calloc: %s", strerror(errno));
		close(fd);
		return NULL;
	}
	parms->fname = fname;
	parms->verbose = verbose;
	parms->fd = fd;
	parms->sat_number = -1;
	parms->abort = 0;
	parms->logfunc = logfunc;

	if (ioctl(fd, FE_GET_INFO, &parms->info) == -1) {
		dvb_perror("FE_GET_INFO");
		dvb_v5_free(parms);
		close(fd);
		return NULL;
	}

	if (verbose) {
		fe_caps_t caps = parms->info.caps;

		dvb_log("Device %s (%s) capabilities:",
			parms->info.name, fname);
		for (i = 0; i < ARRAY_SIZE(fe_caps_name); i++) {
			if (caps & fe_caps_name[i].idx)
				dvb_log ("     %s", fe_caps_name[i].name);
		}
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
		dvb_log ("DVB API Version %d.%d%s, Current v5 delivery system: %s",
			parms->version / 256,
			parms->version % 256,
			use_legacy_call ? " (forcing DVBv3 calls)" : "",
			delivery_system_name[parms->current_sys]);

	if (parms->version < 0x500)
		use_legacy_call = 1;

	if (parms->version >= 0x50a)
		parms->has_v5_stats = 1;
	else
		parms->has_v5_stats = 0;

	if (use_legacy_call || parms->version < 0x505) {
		parms->legacy_fe = 1;
		switch(parms->info.type) {
		case FE_QPSK:
			parms->current_sys = SYS_DVBS;
			parms->systems[parms->num_systems++] = parms->current_sys;
			if (parms->version < 0x0500)
				break;
			if (parms->info.caps & FE_CAN_2G_MODULATION)
				parms->systems[parms->num_systems++] = SYS_DVBS2;
			if (parms->info.caps & FE_CAN_TURBO_FEC)
				parms->systems[parms->num_systems++] = SYS_TURBO;
			break;
		case FE_QAM:
			parms->current_sys = SYS_DVBC_ANNEX_A;
			parms->systems[parms->num_systems++] = parms->current_sys;
			break;
		case FE_OFDM:
			parms->current_sys = SYS_DVBT;
			parms->systems[parms->num_systems++] = parms->current_sys;
			if (parms->version < 0x0500)
				break;
			if (parms->info.caps & FE_CAN_2G_MODULATION)
				parms->systems[parms->num_systems++] = SYS_DVBT2;
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
			dvb_logerr("delivery system not detected");
			dvb_v5_free(parms);
			close(fd);
			return NULL;
		}
	} else {
		parms->dvb_prop[0].cmd = DTV_ENUM_DELSYS;
		parms->n_props = 1;
		dtv_prop.num = 1;
		dtv_prop.props = parms->dvb_prop;
		if (ioctl(fd, FE_GET_PROPERTY, &dtv_prop) == -1) {
			dvb_perror("FE_GET_PROPERTY");
			dvb_v5_free(parms);
			close(fd);
			return NULL;
		}
		parms->num_systems = parms->dvb_prop[0].u.buffer.len;
		for (i = 0; i < parms->num_systems; i++)
			parms->systems[i] = parms->dvb_prop[0].u.buffer.data[i];

		if (parms->num_systems == 0) {
			dvb_logerr("driver died while trying to set the delivery system");
			dvb_v5_free(parms);
			close(fd);
			return NULL;
		}
	}

	if (verbose) {
		dvb_log("Supported delivery system%s: ",
		       (parms->num_systems > 1) ? "s" : "");
		for (i = 0; i < parms->num_systems; i++) {
			if (parms->systems[i] == parms->current_sys)
				dvb_log ("    [%s]",
					delivery_system_name[parms->systems[i]]);
			else
				dvb_log ("     %s",
					delivery_system_name[parms->systems[i]]);
		}
		if (use_legacy_call || parms->version < 0x505)
			dvb_log("Warning: new delivery systems like ISDB-T, ISDB-S, DMB-TH, DSS, ATSC-MH will be miss-detected by a DVBv5.4 or earlier API call");
	}

	/*
	 * Fix a bug at some DVB drivers
	 */
	if (parms->current_sys == SYS_UNDEFINED)
		parms->current_sys = parms->systems[0];

	/* Prepare to use the delivery system */
	dvb_set_sys(parms, parms->current_sys);

	/*
	 * Prepare the status struct - DVBv5.10 parameters should
	 * come first, as they'll be read together.
	 */
	parms->stats.prop[0].cmd = DTV_STAT_SIGNAL_STRENGTH;
	parms->stats.prop[1].cmd = DTV_STAT_CNR;
	parms->stats.prop[2].cmd = DTV_STAT_PRE_ERROR_BIT_COUNT;
	parms->stats.prop[3].cmd = DTV_STAT_PRE_TOTAL_BIT_COUNT;
	parms->stats.prop[4].cmd = DTV_STAT_POST_ERROR_BIT_COUNT;
	parms->stats.prop[5].cmd = DTV_STAT_POST_TOTAL_BIT_COUNT;
	parms->stats.prop[6].cmd = DTV_STAT_ERROR_BLOCK_COUNT;
	parms->stats.prop[7].cmd = DTV_STAT_TOTAL_BLOCK_COUNT;

	/* Now, status and the calculated stats */
	parms->stats.prop[8].cmd = DTV_STATUS;
	parms->stats.prop[9].cmd = DTV_BER;
	parms->stats.prop[10].cmd = DTV_PER;
	parms->stats.prop[11].cmd = DTV_QUALITY;
	parms->stats.prop[12].cmd = DTV_PRE_BER;

	return parms;
}


int dvb_fe_is_satellite(uint32_t delivery_system)
{
	switch (delivery_system) {
	case SYS_DVBS:
	case SYS_DVBS2:
	case SYS_TURBO:
	case SYS_ISDBS:
		return 1;
	default:
		return 0;

	}
}


void dvb_fe_close(struct dvb_v5_fe_parms *parms)
{
	if (!parms)
		return;

	if (parms->fd < 0)
		return;

	/* Disable LNBf power */
	if (dvb_fe_is_satellite(parms->current_sys))
		dvb_fe_sec_voltage(parms, 0, 0);

	close(parms->fd);

	dvb_v5_free(parms);
}


int dvb_add_parms_for_sys(struct dtv_property *dvb_prop,
			  unsigned max_size,
			  fe_delivery_system_t sys)
{
	const unsigned int *sys_props;
	int n;

	/* Make dvb properties reflect the current standard */

	sys_props = dvb_v5_delivery_system[sys];
	if (!sys_props)
		return EINVAL;

	n = 0;
	while (sys_props[n]) {
		dvb_prop[n].cmd = sys_props[n];
		dvb_prop[n].u.data = 0;
		n++;
	}
	dvb_prop[n].cmd = DTV_DELIVERY_SYSTEM;
	dvb_prop[n].u.data = sys;
	n++;

	return n;
}

int dvb_set_sys(struct dvb_v5_fe_parms *parms,
			  fe_delivery_system_t sys)
{
	struct dtv_property dvb_prop[1];
	struct dtv_properties prop;
	int rc;

	if (sys != parms->current_sys) {
		/* Disable LNBf power */
		if (dvb_fe_is_satellite(parms->current_sys) &&
		    !dvb_fe_is_satellite(sys))
			dvb_fe_sec_voltage(parms, 0, 0);

		/* Can't change standard with the legacy FE support */
		if (parms->legacy_fe)
			return EINVAL;

		dvb_prop[0].cmd = DTV_DELIVERY_SYSTEM;
		dvb_prop[0].u.data = sys;
		prop.num = 1;
		prop.props = dvb_prop;

		if (ioctl(parms->fd, FE_SET_PROPERTY, &prop) == -1) {
			dvb_perror("Set delivery system");
			return errno;
		}
	}

	rc = dvb_add_parms_for_sys(parms->dvb_prop,
				   ARRAY_SIZE(parms->dvb_prop), sys);
	if (rc < 0)
		return EINVAL;

	parms->current_sys = sys;
	parms->n_props = rc;

	return 0;
}

static enum dvbv3_emulation_type dvbv3_type(uint32_t delivery_system)
{
	switch (delivery_system) {
	case SYS_DVBC_ANNEX_A:
	case SYS_DVBC_ANNEX_C:
		return DVBV3_QAM;
	case SYS_DVBS:
	case SYS_DVBS2:
	case SYS_TURBO:
	case SYS_ISDBS:
	case SYS_DSS:
		return DVBV3_QPSK;
	case SYS_DVBT:
	case SYS_DVBT2:
	case SYS_ISDBT:
	case SYS_DMBTH:
		return DVBV3_OFDM;
	case SYS_ATSC:
	case SYS_ATSCMH:
	case SYS_DVBC_ANNEX_B:
		return DVBV3_ATSC;
	default:
		return DVBV3_UNKNOWN;
	}
};

static int is_dvbv3_delsys(uint32_t delsys)
{
	int status;

	status = (delsys == SYS_DVBT) || (delsys == SYS_DVBC_ANNEX_A) ||
		 (delsys == SYS_DVBS) || (delsys == SYS_ATSC);

	return status;
}

int dvb_set_compat_delivery_system(struct dvb_v5_fe_parms *parms,
				   uint32_t desired_system)
{
	int i;
	uint32_t delsys = SYS_UNDEFINED;
	enum dvbv3_emulation_type type;


	/* Check if the desired delivery system is supported */
	for (i = 0; i < parms->num_systems; i++) {
		if (parms->systems[i] == desired_system) {
			dvb_set_sys(parms, desired_system);
			return 0;
		}
	}

	/*
	 * Find the closest DVBv3 system that matches the delivery
	 * system.
	 */
	type = dvbv3_type(desired_system);

	/*
	 * Get the last non-DVBv3 delivery system that has the same type
	 * of the desired system
	 */
	for (i = 0; i < parms->num_systems; i++) {
		if ((dvbv3_type(parms->systems[i]) == type) &&
		    !is_dvbv3_delsys(parms->systems[i]))
			delsys = parms->systems[i];
	}

	if (delsys == SYS_UNDEFINED)
		return -1;

	dvb_set_sys(parms, desired_system);

	/* Put ISDB-T into auto mode */
	if (desired_system == SYS_ISDBT) {
		dvb_fe_store_parm(parms, DTV_BANDWIDTH_HZ, 6000000);
		dvb_fe_store_parm(parms, DTV_ISDBT_PARTIAL_RECEPTION, 0);
		dvb_fe_store_parm(parms, DTV_ISDBT_SOUND_BROADCASTING, 0);
		dvb_fe_store_parm(parms, DTV_ISDBT_SB_SUBCHANNEL_ID, 0);
		dvb_fe_store_parm(parms, DTV_ISDBT_SB_SEGMENT_IDX, 0);
		dvb_fe_store_parm(parms, DTV_ISDBT_SB_SEGMENT_COUNT, 0);
		dvb_fe_store_parm(parms, DTV_ISDBT_LAYER_ENABLED, 7);
		dvb_fe_store_parm(parms, DTV_ISDBT_LAYERA_FEC, FEC_AUTO);
		dvb_fe_store_parm(parms, DTV_ISDBT_LAYERB_FEC, FEC_AUTO);
		dvb_fe_store_parm(parms, DTV_ISDBT_LAYERC_FEC, FEC_AUTO);
		dvb_fe_store_parm(parms, DTV_ISDBT_LAYERA_MODULATION, QAM_AUTO);
		dvb_fe_store_parm(parms, DTV_ISDBT_LAYERB_MODULATION, QAM_AUTO);
		dvb_fe_store_parm(parms, DTV_ISDBT_LAYERC_MODULATION, QAM_AUTO);
		dvb_fe_store_parm(parms, DTV_ISDBT_LAYERA_SEGMENT_COUNT, 0);
		dvb_fe_store_parm(parms, DTV_ISDBT_LAYERA_TIME_INTERLEAVING, 0);
		dvb_fe_store_parm(parms, DTV_ISDBT_LAYERB_SEGMENT_COUNT, 0);
		dvb_fe_store_parm(parms, DTV_ISDBT_LAYERB_TIME_INTERLEAVING, 0);
		dvb_fe_store_parm(parms, DTV_ISDBT_LAYERC_SEGMENT_COUNT, 0);
		dvb_fe_store_parm(parms, DTV_ISDBT_LAYERC_TIME_INTERLEAVING, 0);
	}
	return 0;
}

const char *dvb_cmd_name(int cmd)
{
	if (cmd >= 0 && cmd < DTV_USER_COMMAND_START)
		return dvb_v5_name[cmd];
	else if (cmd >= 0 && cmd <= DTV_MAX_STAT_COMMAND)
		return dvb_user_name[cmd - DTV_USER_COMMAND_START];
	return NULL;
}

const char *const *dvb_attr_names(int cmd)
{
	if (cmd >= 0 && cmd < DTV_USER_COMMAND_START)
		return dvb_v5_attr_names[cmd];
	else if (cmd >= 0 && cmd <= DTV_MAX_STAT_COMMAND)
		return dvb_user_attr_names[cmd - DTV_USER_COMMAND_START];
	return NULL;
}

void dvb_fe_prt_parms(const struct dvb_v5_fe_parms *parms)
{
	int i;

	for (i = 0; i < parms->n_props; i++) {
		const char * const *attr_name = dvb_attr_names(parms->dvb_prop[i].cmd);
		if (attr_name) {
			int j;

			for (j = 0; j < parms->dvb_prop[i].u.data; j++) {
				if (!*attr_name)
					break;
				attr_name++;
			}
		}

		if (!attr_name || !*attr_name)
			dvb_log("%s = %u",
				dvb_cmd_name(parms->dvb_prop[i].cmd),
				parms->dvb_prop[i].u.data);
		else
			dvb_log("%s = %s",
				dvb_cmd_name(parms->dvb_prop[i].cmd),
				*attr_name);
	}
};

int dvb_fe_retrieve_parm(const struct dvb_v5_fe_parms *parms,
				unsigned cmd, uint32_t *value)
{
	int i;
	for (i = 0; i < parms->n_props; i++) {
		if (parms->dvb_prop[i].cmd != cmd)
			continue;
		*value = parms->dvb_prop[i].u.data;
		return 0;
	}
	dvb_logerr("command %s (%d) not found during retrieve",
		dvb_cmd_name(cmd), cmd);

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
	dvb_logerr("command %s (%d) not found during store",
		dvb_cmd_name(cmd), cmd);

	return EINVAL;
}

static int dvb_copy_fe_props(const struct dtv_property *from, int n, struct dtv_property *to)
{
	int i, j;
	for (i = 0, j = 0; i < n; i++)
		if (from[i].cmd < DTV_USER_COMMAND_START)
			to[j++] = from[i];
	return j;
}

int dvb_fe_get_parms(struct dvb_v5_fe_parms *parms)
{
	int n = 0;
	const unsigned int *sys_props;
	struct dtv_properties prop;
	struct dvb_frontend_parameters v3_parms;
	uint32_t bw;

	sys_props = dvb_v5_delivery_system[parms->current_sys];
	if (!sys_props)
		return EINVAL;

	while (sys_props[n]) {
		parms->dvb_prop[n].cmd = sys_props[n];
		n++;
	}
	parms->dvb_prop[n].cmd = DTV_DELIVERY_SYSTEM;
	parms->dvb_prop[n].u.data = parms->current_sys;
	n++;

	/* Keep it ready for set */
	parms->dvb_prop[n].cmd = DTV_TUNE;
	parms->n_props = n;

	struct dtv_property fe_prop[DTV_MAX_COMMAND];
	n = dvb_copy_fe_props(parms->dvb_prop, n, fe_prop);

	prop.props = fe_prop;
	prop.num = n;
	if (!parms->legacy_fe) {
		if (ioctl(parms->fd, FE_GET_PROPERTY, &prop) == -1) {
			dvb_perror("FE_GET_PROPERTY");
			return errno;
		}
		if (parms->verbose) {
			dvb_log("Got parameters for %s:",
			       delivery_system_name[parms->current_sys]);
			dvb_fe_prt_parms(parms);
		}
		return 0;
	}
	/* DVBv3 call */
	if (ioctl(parms->fd, FE_GET_FRONTEND, &v3_parms) == -1) {
		dvb_perror("FE_GET_FRONTEND");
		return -1;
	}

	dvb_fe_store_parm(parms, DTV_FREQUENCY, v3_parms.frequency);
	dvb_fe_store_parm(parms, DTV_INVERSION, v3_parms.inversion);
	switch (parms->current_sys) {
	case SYS_DVBS:
		dvb_fe_store_parm(parms, DTV_SYMBOL_RATE, v3_parms.u.qpsk.symbol_rate);
		dvb_fe_store_parm(parms, DTV_INNER_FEC, v3_parms.u.qpsk.fec_inner);
		break;
	case SYS_DVBC_ANNEX_A:
		dvb_fe_store_parm(parms, DTV_SYMBOL_RATE, v3_parms.u.qam.symbol_rate);
		dvb_fe_store_parm(parms, DTV_INNER_FEC, v3_parms.u.qam.fec_inner);
		dvb_fe_store_parm(parms, DTV_MODULATION, v3_parms.u.qam.modulation);
		break;
	case SYS_ATSC:
	case SYS_ATSCMH:
	case SYS_DVBC_ANNEX_B:
		dvb_fe_store_parm(parms, DTV_MODULATION, v3_parms.u.vsb.modulation);
		break;
	case SYS_DVBT:
		if (v3_parms.u.ofdm.bandwidth < ARRAY_SIZE(fe_bandwidth_name) -1)
			bw = fe_bandwidth_name[v3_parms.u.ofdm.bandwidth];
		else bw = 0;
		dvb_fe_store_parm(parms, DTV_BANDWIDTH_HZ, bw);
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

	return 0;
}

int dvb_fe_set_parms(struct dvb_v5_fe_parms *parms)
{
	/* Use a temporary copy of the parameters so we can safely perform
	 * adjustments for satellite */
	struct dvb_v5_fe_parms tmp_parms = *parms;

	struct dtv_properties prop;
	struct dvb_frontend_parameters v3_parms;
	uint32_t bw;

	if (dvb_fe_is_satellite(tmp_parms.current_sys))
		dvb_sat_set_parms(&tmp_parms);

	/* Filter out any user DTV_foo property such as DTV_POLARIZATION */
	tmp_parms.n_props = dvb_copy_fe_props(tmp_parms.dvb_prop, tmp_parms.n_props, tmp_parms.dvb_prop);

	prop.props = tmp_parms.dvb_prop;
	prop.num = tmp_parms.n_props;
	prop.props[prop.num].cmd = DTV_TUNE;
	prop.num++;

	if (!parms->legacy_fe) {
		if (ioctl(parms->fd, FE_SET_PROPERTY, &prop) == -1) {
			dvb_perror("FE_SET_PROPERTY");
			if (parms->verbose)
				dvb_fe_prt_parms(parms);
			return -1;
		}
		return 0;
	}
	/* DVBv3 call */

	dvb_fe_retrieve_parm(&tmp_parms, DTV_FREQUENCY, &v3_parms.frequency);
	dvb_fe_retrieve_parm(&tmp_parms, DTV_INVERSION, &v3_parms.inversion);
	switch (tmp_parms.current_sys) {
	case SYS_DVBS:
		dvb_fe_retrieve_parm(&tmp_parms, DTV_SYMBOL_RATE, &v3_parms.u.qpsk.symbol_rate);
		dvb_fe_retrieve_parm(&tmp_parms, DTV_INNER_FEC, &v3_parms.u.qpsk.fec_inner);
		break;
	case SYS_DVBC_ANNEX_AC:
		dvb_fe_retrieve_parm(&tmp_parms, DTV_SYMBOL_RATE, &v3_parms.u.qam.symbol_rate);
		dvb_fe_retrieve_parm(&tmp_parms, DTV_INNER_FEC, &v3_parms.u.qam.fec_inner);
		dvb_fe_retrieve_parm(&tmp_parms, DTV_MODULATION, &v3_parms.u.qam.modulation);
		break;
	case SYS_ATSC:
	case SYS_ATSCMH:
	case SYS_DVBC_ANNEX_B:
		dvb_fe_retrieve_parm(&tmp_parms, DTV_MODULATION, &v3_parms.u.vsb.modulation);
		break;
	case SYS_DVBT:
		for (bw = 0; fe_bandwidth_name[bw] != 0; bw++) {
			if (fe_bandwidth_name[bw] == v3_parms.u.ofdm.bandwidth)
				break;
		}
		dvb_fe_retrieve_parm(&tmp_parms, DTV_BANDWIDTH_HZ, &bw);
		dvb_fe_retrieve_parm(&tmp_parms, DTV_CODE_RATE_HP, &v3_parms.u.ofdm.code_rate_HP);
		dvb_fe_retrieve_parm(&tmp_parms, DTV_CODE_RATE_LP, &v3_parms.u.ofdm.code_rate_LP);
		dvb_fe_retrieve_parm(&tmp_parms, DTV_MODULATION, &v3_parms.u.ofdm.constellation);
		dvb_fe_retrieve_parm(&tmp_parms, DTV_TRANSMISSION_MODE, &v3_parms.u.ofdm.transmission_mode);
		dvb_fe_retrieve_parm(&tmp_parms, DTV_GUARD_INTERVAL, &v3_parms.u.ofdm.guard_interval);
		dvb_fe_retrieve_parm(&tmp_parms, DTV_HIERARCHY, &v3_parms.u.ofdm.hierarchy_information);
		break;
	default:
		return -EINVAL;
	}
	if (ioctl(tmp_parms.fd, FE_SET_FRONTEND, &v3_parms) == -1) {
		dvb_perror("FE_SET_FRONTEND");
		if (tmp_parms.verbose)
			dvb_fe_prt_parms(&tmp_parms);
		return -1;
	}

	return 0;
}

static struct dtv_stats *dvb_fe_store_stats(struct dvb_v5_fe_parms *parms,
			      unsigned cmd,
			      enum fecap_scale_params scale,
			      unsigned layer,
			      uint32_t value)
{
	int i;
	for (i = 0; i < DTV_NUM_STATS_PROPS; i++) {
		if (parms->stats.prop[i].cmd != cmd)
			continue;
		parms->stats.prop[i].u.st.stat[layer].scale = scale;
		parms->stats.prop[i].u.st.stat[layer].uvalue = value;
		if (parms->stats.prop[i].u.st.len < layer + 1)
			parms->stats.prop[i].u.st.len = layer + 1;
		return &parms->stats.prop[i].u.st.stat[layer];
	}
	dvb_logerr("%s not found on store", dvb_cmd_name(cmd));

	return NULL;
}

static float calculate_postBER(struct dvb_v5_fe_parms *parms, unsigned layer)
{
	uint64_t n, d;

	if (!parms->stats.has_post_ber[layer])
		return -1;

	d = parms->stats.cur[layer].post_bit_count - parms->stats.prev[layer].post_bit_count;
	if (!d)
		return -1;

	n = parms->stats.cur[layer].post_bit_error - parms->stats.prev[layer].post_bit_error;

	return ((float)n)/d;
}

static float calculate_preBER(struct dvb_v5_fe_parms *parms, unsigned layer)
{
	uint64_t n, d;

	if (!parms->stats.has_pre_ber[layer])
		return -1;

	d = parms->stats.cur[layer].pre_bit_count - parms->stats.prev[layer].pre_bit_count;
	if (!d)
		return -1;

	n = parms->stats.cur[layer].pre_bit_error - parms->stats.prev[layer].pre_bit_error;

	return ((float)n)/d;
}

static struct dtv_stats *dvb_fe_retrieve_v5_BER(struct dvb_v5_fe_parms *parms,
					        unsigned layer)
{
	float ber;
	uint64_t ber64;

	ber = calculate_postBER(parms, layer);
	if (ber < 0)
		return NULL;

	/*
	 * Put BER into some DVBv3 compat scale. The thing is that DVBv3 has no
	 * defined scale for BER. So, let's use 10^-7.
	 */

	ber64 = 10000000 * ber;
	return dvb_fe_store_stats(parms, DTV_BER, FE_SCALE_COUNTER, layer, ber64);
}

struct dtv_stats *dvb_fe_retrieve_stats_layer(struct dvb_v5_fe_parms *parms,
					      unsigned cmd, unsigned layer)
{
	int i;

	if (cmd == DTV_BER && parms->has_v5_stats)
		return dvb_fe_retrieve_v5_BER(parms, layer);

	for (i = 0; i < DTV_NUM_STATS_PROPS; i++) {
		if (parms->stats.prop[i].cmd != cmd)
			continue;
		if (layer >= parms->stats.prop[i].u.st.len)
			return NULL;
		return &parms->stats.prop[i].u.st.stat[layer];
	}
	dvb_logerr("%s not found on retrieve", dvb_cmd_name(cmd));

	return NULL;
}

int dvb_fe_retrieve_stats(struct dvb_v5_fe_parms *parms,
			  unsigned cmd, uint32_t *value)
{
	struct dtv_stats *stat;
	enum fecap_scale_params scale;

	stat = dvb_fe_retrieve_stats_layer(parms, cmd, 0);
	if (!stat) {
		if (parms->verbose)
			dvb_logdbg("%s not found on retrieve", dvb_cmd_name(cmd));
		return EINVAL;
	}

	scale = stat->scale;
	if (scale == FE_SCALE_NOT_AVAILABLE) {
		if (parms->verbose)
			dvb_logdbg("%s not available", dvb_cmd_name(cmd));
		return EINVAL;
	}

	*value = stat->uvalue;

	if (parms->verbose)
		dvb_logdbg("Stats for %s = %d", dvb_cmd_name(cmd), *value);

	return 0;
}

float dvb_fe_retrieve_ber(struct dvb_v5_fe_parms *parms, unsigned layer,
			  enum fecap_scale_params *scale)
{
	float ber;
	uint32_t ber32;

	if (parms->has_v5_stats) {
		ber = calculate_postBER(parms, layer);
		if (ber >= 0)
			*scale = FE_SCALE_COUNTER;
		else
			*scale = FE_SCALE_NOT_AVAILABLE;
		return ber;
	}

	if (layer) {
		*scale = FE_SCALE_NOT_AVAILABLE;
		return -1;
	}

	if (dvb_fe_retrieve_stats(parms, DTV_BER, &ber32))
		*scale = FE_SCALE_NOT_AVAILABLE;
	else
		*scale = FE_SCALE_RELATIVE;

	return ber32;
}

float dvb_fe_retrieve_per(struct dvb_v5_fe_parms *parms, unsigned layer)
{
	uint64_t n, d;

	if (!parms->stats.has_per[layer]) {
		return -1;
	}

	d = parms->stats.cur[layer].block_count - parms->stats.prev[layer].block_count;
	if (!d) {
		return -1;
	}

	n = parms->stats.cur[layer].block_error - parms->stats.prev[layer].block_error;

	return ((float)n)/d;
}

struct cnr_to_qual_s {
	uint32_t modulation;		/* use QAM_AUTO if it doesn't matter */
	uint32_t fec;			/* Use FEC_NONE if it doesn't matter */
	float cnr_ok, cnr_good;
};

static enum dvb_quality cnr_arr_to_qual(uint32_t modulation,
					 uint32_t fec,
					 float cnr,
					 struct cnr_to_qual_s *arr,
					 unsigned len)
{
	int i;

	for (i = 0; i < len; i++) {
		if (modulation == arr[i].modulation) {
			if (cnr < arr[i].cnr_ok)
				return DVB_QUAL_POOR;
			else if (cnr < arr[i].cnr_good)
				return DVB_QUAL_OK;
			else
				return DVB_QUAL_GOOD;

		}
	}

	return DVB_QUAL_UNKNOWN;
}

/* Source: http://www.maxpeak.tv/articles/Maxpeak_Article_2.pdf */
struct cnr_to_qual_s dvb_c_cnr_2_qual[] = {
	{ QAM_256, FEC_NONE,  34., 38.},
	{ QAM_64,  FEC_NONE,  30., 34.},
};

/*
 * Base reference: http://www.maxpeak.tv/articles/Maxpeak_Article_2.pdf
 * Used http://www.nws.noaa.gov/noaaport/html/DVB%20S2%20Satellite%20Receiver%20Specs.pdf
 * to estimate the missing FEC's
 */
struct cnr_to_qual_s dvb_s_cnr_2_qual[] = {
	{ QPSK, FEC_1_2,  7., 10.},

	{ QPSK, FEC_2_3,  9., 12.},
	{ QPSK, FEC_3_4, 10., 13.},
	{ QPSK, FEC_5_6, 11., 14.},

	{ QPSK, FEC_7_8, 12., 15.},
};

struct cnr_to_qual_s dvb_s2_cnr_2_qual[] = {
	{ QPSK,  FEC_1_2,   9.,  12.},
	{ QPSK,  FEC_2_3,  11.,  14.},
	{ QPSK,  FEC_3_4,  12.,  15.},
	{ QPSK,  FEC_5_6,  12.,  15.},
	{ QPSK,  FEC_8_9,  13.,  16.},
	{ QPSK,  FEC_9_10, 13.5, 16.5},
	{ PSK_8, FEC_2_3,  14.5, 17.5},
	{ PSK_8, FEC_3_4,  16.,  19.},
	{ PSK_8, FEC_5_6,  17.5, 20.5},
	{ PSK_8, FEC_8_9,  19.,  22.},
};

/*
 * Minimum values from ARIB STD-B21 for DVB_QUAL_OK.
 * As ARIB doesn't define a max value, assume +2dB for DVB_QUAL_GOOD
 */
struct cnr_to_qual_s isdb_t_cnr_2_qual[] = {
	{  DQPSK, FEC_1_2,  6.2,  8.2},
	{  DQPSK, FEC_2_3,  7.7,  9.7},
	{  DQPSK, FEC_3_4,  8.7, 10.7},
	{  DQPSK, FEC_5_6,  9.6, 11.6},
	{  DQPSK, FEC_7_8, 10.4, 12.4},

	{   QPSK, FEC_1_2,  4.9,  6.9},
	{   QPSK, FEC_2_3,  6.6,  8.6},
	{   QPSK, FEC_3_4,  7.5,  9.5},
	{   QPSK, FEC_5_6,  8.5, 10.5},
	{   QPSK, FEC_7_8,  9.1, 11.5},

	{ QAM_16, FEC_1_2, 11.5, 13.5},
	{ QAM_16, FEC_2_3, 13.5, 15.5},
	{ QAM_16, FEC_3_4, 14.6, 16.6},
	{ QAM_16, FEC_5_6, 15.6, 17.6},
	{ QAM_16, FEC_7_8, 16.2, 18.2},

	{ QAM_64, FEC_1_2, 16.5, 18.5},
	{ QAM_64, FEC_2_3, 18.7, 21.7},
	{ QAM_64, FEC_3_4, 20.1, 22.1},
	{ QAM_64, FEC_5_6, 21.3, 23.3},
	{ QAM_64, FEC_7_8, 22.0, 24.0},
};

/*
 * Values obtained from table A.1 of ETSI EN 300 744 v1.6.1
 * OK corresponds to Ricean fading; Good to Rayleigh fading
 */
struct cnr_to_qual_s dvb_t_cnr_2_qual[] = {
	{   QPSK, FEC_1_2,  4.1,  5.9},
	{   QPSK, FEC_2_3,  6.1,  9.6},
	{   QPSK, FEC_3_4,  7.2, 12.4},
	{   QPSK, FEC_5_6,  8.5, 15.6},
	{   QPSK, FEC_7_8,  9.2, 17.5},

	{ QAM_16, FEC_1_2,  9.8, 11.8},
	{ QAM_16, FEC_2_3, 12.1, 15.3},
	{ QAM_16, FEC_3_4, 13.4, 18.1},
	{ QAM_16, FEC_5_6, 14.8, 21.3},
	{ QAM_16, FEC_7_8, 15.7, 23.6},

	{ QAM_64, FEC_1_2, 14.0, 16.0},
	{ QAM_64, FEC_2_3, 19.9, 25.4},
	{ QAM_64, FEC_3_4, 24.9, 27.9},
	{ QAM_64, FEC_5_6, 21.3, 23.3},
	{ QAM_64, FEC_7_8, 22.0, 24.0},
};

static enum dvb_quality dvbv_fe_cnr_to_quality(struct dvb_v5_fe_parms *parms,
					       struct dtv_stats *cnr)
{
	uint32_t modulation, fec;
	enum dvb_quality qual = DVB_QUAL_UNKNOWN;

	switch (cnr->scale) {
	case FE_SCALE_RELATIVE:
		if (cnr->uvalue == 65535)
			return DVB_QUAL_GOOD;
		else if (cnr->uvalue >= 65535 / 2)
			return DVB_QUAL_OK;
		else
			return DVB_QUAL_POOR;
		return qual;
	case FE_SCALE_DECIBEL:
		break;
	default:
		return DVB_QUAL_UNKNOWN;
	}

	switch (parms->current_sys) {
	case SYS_DVBC_ANNEX_A:
	case SYS_DVBC_ANNEX_C:
		dvb_fe_retrieve_parm(parms, DTV_MODULATION, &modulation);
		if (modulation == QAM_AUTO)
			modulation = QAM_64;	/* Assume worse case */
		qual = cnr_arr_to_qual(modulation, FEC_NONE, cnr->svalue,
				       dvb_c_cnr_2_qual,
				       ARRAY_SIZE(dvb_c_cnr_2_qual));
		break;
	case SYS_DVBS:
		dvb_fe_retrieve_parm(parms, DTV_INNER_FEC, &fec);
		qual = cnr_arr_to_qual(QPSK, fec, cnr->svalue,
				       dvb_s_cnr_2_qual,
				       ARRAY_SIZE(dvb_s_cnr_2_qual));
		break;
	case SYS_DVBS2:
		dvb_fe_retrieve_parm(parms, DTV_MODULATION, &modulation);
		dvb_fe_retrieve_parm(parms, DTV_INNER_FEC, &fec);
		qual = cnr_arr_to_qual(modulation, fec, cnr->svalue,
			               dvb_s2_cnr_2_qual,
				       ARRAY_SIZE(dvb_s_cnr_2_qual));
		break;
	case SYS_ISDBT:
		dvb_fe_retrieve_parm(parms, DTV_MODULATION, &modulation);
		dvb_fe_retrieve_parm(parms, DTV_INNER_FEC, &fec);
		if (modulation == QAM_AUTO)
			modulation = QAM_64;	/* Assume worse case */
		qual = cnr_arr_to_qual(modulation, fec, cnr->svalue,
			               isdb_t_cnr_2_qual,
				       ARRAY_SIZE(isdb_t_cnr_2_qual));
		break;
	case SYS_DVBT:
	case SYS_DVBT2:
	case SYS_TURBO:
	case SYS_ISDBS:
	case SYS_DSS:
	case SYS_DMBTH:
	case SYS_ATSC:
	case SYS_ATSCMH:
	case SYS_DVBC_ANNEX_B:
	default:
		/* Quality unknown */
		break;
	}

	return qual;
};

static enum dvb_quality dvb_fe_retrieve_quality(struct dvb_v5_fe_parms *parms,
						unsigned layer)
{
	float ber, per;
	struct dtv_stats *cnr;
	enum dvb_quality qual = DVB_QUAL_UNKNOWN;

	per = dvb_fe_retrieve_per(parms, layer);
	if (per >= 0) {
		if (per > 1e-6)
			qual = DVB_QUAL_POOR;
		else if (per > 1e-7)
			return DVB_QUAL_OK;
		else
			return DVB_QUAL_GOOD;
	}

	ber = dvb_fe_retrieve_per(parms, layer);
	if (ber >= 0) {

		if (ber > 1e-3)	/* FIXME: good enough???? */
			return DVB_QUAL_POOR;
		if (ber <= 2e-4)		/* BER = 10^-11 at TS */
			return DVB_QUAL_GOOD;
		else
			qual = DVB_QUAL_OK;	/* OK or good */
	}

	cnr = dvb_fe_retrieve_stats_layer(parms, DTV_STAT_CNR, layer);
	if (cnr)
		dvbv_fe_cnr_to_quality(parms, cnr);

	return qual;
}

static void dvb_fe_update_counters(struct dvb_v5_fe_parms *parms)
{
	struct dtv_stats *error, *count;
	int i;

	for (i = 0; i < MAX_DTV_STATS; i++) {
		count = dvb_fe_retrieve_stats_layer(parms, DTV_STAT_POST_TOTAL_BIT_COUNT, i);
		if (count && count->scale != FE_SCALE_NOT_AVAILABLE) {
			error = dvb_fe_retrieve_stats_layer(parms, DTV_STAT_POST_ERROR_BIT_COUNT, i);
			if (!error || error->scale == FE_SCALE_NOT_AVAILABLE) {
				parms->stats.has_post_ber[i] = 0;
			} else if(count->uvalue != parms->stats.cur[i].post_bit_count) {
				parms->stats.prev[i].post_bit_count = parms->stats.cur[i].post_bit_count;
				parms->stats.cur[i].post_bit_count = count->uvalue;

				parms->stats.prev[i].post_bit_error = parms->stats.cur[i].post_bit_error;
				parms->stats.cur[i].post_bit_error = error->uvalue;

				parms->stats.has_post_ber[i] = 1;
			}
		} else
			parms->stats.has_post_ber[i] = 0;
		count = dvb_fe_retrieve_stats_layer(parms, DTV_STAT_PRE_TOTAL_BIT_COUNT, i);
		if (count && count->scale != FE_SCALE_NOT_AVAILABLE) {
			error = dvb_fe_retrieve_stats_layer(parms, DTV_STAT_PRE_ERROR_BIT_COUNT, i);
			if (!error || error->scale == FE_SCALE_NOT_AVAILABLE) {
				parms->stats.has_pre_ber[i] = 0;
			} else if(count->uvalue != parms->stats.cur[i].pre_bit_count) {
				parms->stats.prev[i].pre_bit_count = parms->stats.cur[i].pre_bit_count;
				parms->stats.cur[i].pre_bit_count = count->uvalue;

				parms->stats.prev[i].pre_bit_error = parms->stats.cur[i].pre_bit_error;
				parms->stats.cur[i].pre_bit_error = error->uvalue;

				parms->stats.has_pre_ber[i] = 1;
			}
		} else
			parms->stats.has_pre_ber[i] = 0;
		count = dvb_fe_retrieve_stats_layer(parms, DTV_STAT_TOTAL_BLOCK_COUNT, i);
		if (count && count->scale != FE_SCALE_NOT_AVAILABLE) {
			error = dvb_fe_retrieve_stats_layer(parms, DTV_STAT_ERROR_BLOCK_COUNT, i);
			if (!error || error->scale == FE_SCALE_NOT_AVAILABLE) {
				parms->stats.has_per[i] = 0;
			} else if (count->uvalue != parms->stats.cur[i].block_count) {
				parms->stats.prev[i].block_count = parms->stats.cur[i].block_count;
				parms->stats.cur[i].block_count = count->uvalue;

				parms->stats.prev[i].block_error = parms->stats.cur[i].block_error;
				parms->stats.cur[i].block_error = error->uvalue;

				parms->stats.has_per[i] = 1;
			}
		} else
			parms->stats.has_per[i] = 0;
	}
}

int dvb_fe_get_stats(struct dvb_v5_fe_parms *parms)
{
	fe_status_t status = 0;
	uint32_t ber= 0, ucb = 0;
	uint16_t strength = 0, snr = 0;
	int i;
	enum fecap_scale_params scale;

	if (ioctl(parms->fd, FE_READ_STATUS, &status) == -1) {
		dvb_perror("FE_READ_STATUS");
		return EINVAL;
	}
	dvb_fe_store_stats(parms, DTV_STATUS, FE_SCALE_RELATIVE, 0, status);

	/* if lock has obtained, get DVB parameters */
	if (status != parms->stats.prev_status) {
		if ((status & FE_HAS_LOCK) &&
		    parms->stats.prev_status != status)
			dvb_fe_get_parms(parms);
		parms->stats.prev_status = status;
	}

	if (parms->has_v5_stats) {
		struct dtv_properties props;

		props.num = DTV_NUM_KERNEL_STATS;
		props.props = parms->stats.prop;

		/* Do a DVBv5.10 stats call */
		if (ioctl(parms->fd, FE_GET_PROPERTY, &props) == -1)
			return errno;

		/*
		 * All props with len=0 mean that this device doesn't have any
		 * dvbv5 stats. Try the legacy stats instead.
		 */
		for (i = 0; i < props.num; i++)
			if (parms->stats.prop[i].u.st.len)
				break;
		if (i == props.num)
			goto dvbv3_fallback;

		dvb_fe_update_counters(parms);

		return 0;
	}

dvbv3_fallback:
	/* DVB v3 stats */
	parms->has_v5_stats = 0;

	if (ioctl(parms->fd, FE_READ_BER, &ber) == -1)
		scale = FE_SCALE_NOT_AVAILABLE;
	else
		scale = FE_SCALE_RELATIVE;

	/*
	 * BER scale on DVBv3 is not defined - different drivers use
	 * different scales, even weird ones, like multiples of 1/65280
	 */
	dvb_fe_store_stats(parms, DTV_BER, scale, 0, ber);

	if (ioctl(parms->fd, FE_READ_SIGNAL_STRENGTH, &strength) == -1)
		scale = FE_SCALE_NOT_AVAILABLE;
	else
		scale = FE_SCALE_RELATIVE;

	dvb_fe_store_stats(parms, DTV_STAT_SIGNAL_STRENGTH, scale, 0, strength);

	if (ioctl(parms->fd, FE_READ_SNR, &snr) == -1)
		scale = FE_SCALE_NOT_AVAILABLE;
	else
		scale = FE_SCALE_RELATIVE;
	dvb_fe_store_stats(parms, DTV_STAT_CNR, scale, 0, snr);

	if (ioctl(parms->fd, FE_READ_UNCORRECTED_BLOCKS, &ucb) == -1)
		scale = FE_SCALE_NOT_AVAILABLE;
	else
		scale = FE_SCALE_COUNTER;
	dvb_fe_store_stats(parms, DTV_STAT_ERROR_BLOCK_COUNT, scale, 0, snr);

	if (parms->verbose > 1) {
		dvb_log("Status: ");
		for (i = 0; i < ARRAY_SIZE(fe_status_name); i++) {
			if (status & fe_status_name[i].idx)
				dvb_log ("    %s", fe_status_name[i].name);
		}
		dvb_log("BER: %d, Strength: %d, SNR: %d, UCB: %d",
		       ber, strength, snr, ucb);
	}
	return 0;
}


int dvb_fe_get_event(struct dvb_v5_fe_parms *parms)
{
	struct dvb_frontend_event event;
	fe_status_t status;
	int i;

	if (!parms->legacy_fe) {
		dvb_fe_get_parms(parms);
		return dvb_fe_get_stats(parms);
	}

	if (ioctl(parms->fd, FE_GET_EVENT, &event) == -1) {
		dvb_perror("FE_GET_EVENT");
		return -1;
	}
	status = event.status;
	if (parms->verbose > 1) {
		dvb_log("Status: ");
		for (i = 0; i < ARRAY_SIZE(fe_status_name); i++) {
			if (status & fe_status_name[i].idx)
				dvb_log ("    %s", fe_status_name[i].name);
		}
	}
	dvb_fe_store_stats(parms, DTV_STATUS, FE_SCALE_RELATIVE, 0, status);

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
	case SYS_ATSCMH:
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

	return dvb_fe_get_stats(parms);
}

int dvb_fe_snprintf_eng(char *buf, int len, float val)
{
	int digits = 3;
	int exp, signal = 1;

	/* If value is zero, nothing to do */
	if (val == 0.)
		return snprintf(buf, len, " 0");

	/* Take the absolute value */
	if (val < 0.) {
		signal = -1;
		val = -val;
	}

	/*
	 * Converts the number into an expoent and a
	 * value between 0 and 1000, exclusive
	 */
	exp = (int)log10(val);
	if (exp > 0)
		exp = (exp / 3) * 3;
	else
		exp = (-exp + 3) / 3 * (-3);

	val *= pow(10, -exp);

	if (val >= 1000.) {
		val /= 1000.0;
		exp += 3;
	} else if(val >= 100.0)
		digits -= 2;
	else if(val >= 10.0)
		digits -= 1;

	if (exp) {
		if (signal > 0)
			return snprintf(buf, len, " %.*fx10^%d", digits - 1,
					val, exp);
		else
			return snprintf(buf, len, " -%.*fx10^%d", digits - 1,
					val, exp);
	} else {
		if (signal > 0)
			return snprintf(buf, len, " %.*f", digits - 1, val);
		else
			return snprintf(buf, len, " -%.*f", digits - 1, val);
	}
}

static char *sig_bits[7] = {
	[0] = "RF",
	[1] = "Carrier",
	[2] = "Viterbi",
	[3] = "Sync",
	[4] = "Lock",
	[5] = "Timeout",
	[6] = "Reinit",
};

static char *qual_name[] = {
	[DVB_QUAL_POOR] = "Poor",
	[DVB_QUAL_OK]   = "Ok",
	[DVB_QUAL_GOOD] = "Good",
};

int dvb_fe_snprintf_stat(struct dvb_v5_fe_parms *parms, uint32_t cmd,
			  char *display_name, int layer,
		          char **buf, int *len, int *show_layer_name)
{
	struct dtv_stats *stat = NULL;
	enum dvb_quality qual = DVB_QUAL_UNKNOWN;
	enum fecap_scale_params scale;
	float val = -1;
	int initial_len = *len;
	int size, i;

	/* Print status, if layer == 0, as there is only global status */
	if (cmd == DTV_STATUS) {
		fe_status_t status;

		if (layer)
			return 0;

		if (dvb_fe_retrieve_stats(parms, DTV_STATUS, &status)) {
			dvb_logerr ("Error: no adapter status");
			return -1;
		}
		if (display_name) {
			size = snprintf(*buf, *len, " %s=", display_name);
			*buf += size;
			*len -= size;
		}

		/* Get the name of the highest status bit */
		for (i = ARRAY_SIZE(sig_bits) - 1; i >= 0 ; i--) {
			if ((1 << i) & status) {
				size = snprintf(*buf, *len, "%-7s", sig_bits[i]);
				break;
			}
		}
		if (i < 0)
			size = snprintf(*buf, *len, "%7s", "");
		*buf += size;
		len -= size;

		/* Add the status bits */
		size = snprintf(*buf, *len, "(0x%02x)", status);
		*buf += size;
		len -= size;

		return initial_len - *len;
	}

	/* Retrieve the statistics */
	switch (cmd) {
	case DTV_PRE_BER:
		val = calculate_preBER(parms, layer);
		if (val < 0)
			return 0;
		scale = FE_SCALE_COUNTER;
		break;
	case DTV_BER:
		val = dvb_fe_retrieve_ber(parms, layer, &scale);
		if (scale == FE_SCALE_NOT_AVAILABLE)
			return 0;
		break;
	case DTV_PER:
		val = dvb_fe_retrieve_per(parms, layer);
		if (val < 0)
			return 0;
		scale = FE_SCALE_COUNTER;
		break;
	case DTV_QUALITY:
		qual = dvb_fe_retrieve_quality(parms, layer);
		if (qual == DVB_QUAL_UNKNOWN)
			return 0;
		break;
	default:
		stat = dvb_fe_retrieve_stats_layer(parms, cmd, layer);
		if (!stat || stat->scale == FE_SCALE_NOT_AVAILABLE)
			return 0;
	}

	/* If requested, prints the layer name */
	if (*show_layer_name && layer) {
		size = snprintf(*buf, *len, "  Layer %c:", 'A' + layer - 1);
		*buf += size;
		*len -= size;
		*show_layer_name = 0;
	}
	if (display_name) {
		size = snprintf(*buf, *len, " %s=", display_name);
		*buf += size;
		*len -= size;
	}

	/* Quality measure */
	if (qual != DVB_QUAL_UNKNOWN) {
		size = snprintf(*buf, *len, " %-4s", qual_name[qual]);
		*buf += size;
		*len -= size;
		return initial_len - *len;
	}


	/* Special case: float point measures like BER/PER */
	if (!stat) {
		switch (scale) {
		case FE_SCALE_RELATIVE:
			size = snprintf(*buf, *len, " %u", (unsigned int)val);
			break;
		case FE_SCALE_COUNTER:
			size = dvb_fe_snprintf_eng(*buf, *len, val);
			break;
		default:
			size = 0;
		}
		*buf += size;
		*len -= size;
		return initial_len - *len;
	}

	/* Prints the scale */
	switch (stat->scale) {
	case FE_SCALE_DECIBEL:
		if (cmd == DTV_STAT_SIGNAL_STRENGTH)
			size = snprintf(*buf, *len, " %.2fdBm", stat->svalue / 1000.);
		else
			size = snprintf(*buf, *len, " %.2fdB", stat->svalue / 1000.);
		break;
	case FE_SCALE_RELATIVE:
		size = snprintf(*buf, *len, " %3.2f%%", (100 * stat->uvalue) / 65535.);
		break;
	case FE_SCALE_COUNTER:
		size = snprintf(*buf, *len, " %" PRIu64, (uint64_t)stat->uvalue);
		break;
	default:
		size = 0;
	}
	*buf += size;
	*len -= size;

	return initial_len - *len;
}

/*
 * Implement SEC/LNB/DISEqC specific functions
 * For now, DVBv5 API doesn't support those commands. So, use the DVBv3
 * version.
 */

int dvb_fe_sec_voltage(struct dvb_v5_fe_parms *parms, int on, int v18)
{
	fe_sec_voltage_t v;
	int rc;

	if (!on) {
		v = SEC_VOLTAGE_OFF;
		if (parms->verbose)
			dvb_log("DiSEqC VOLTAGE: OFF");
	} else {
		v = v18 ? SEC_VOLTAGE_18 : SEC_VOLTAGE_13;
		if (parms->verbose)
			dvb_log("DiSEqC VOLTAGE: %s", v18 ? "18" : "13");
	}
	rc = ioctl(parms->fd, FE_SET_VOLTAGE, v);
	if (rc == -1)
		dvb_perror("FE_SET_VOLTAGE");
	return rc;
}

int dvb_fe_sec_tone(struct dvb_v5_fe_parms *parms, fe_sec_tone_mode_t tone)
{
	int rc;
	if (parms->verbose)
		dvb_log( "DiSEqC TONE: %s", fe_tone_name[tone] );
	rc = ioctl(parms->fd, FE_SET_TONE, tone);
	if (rc == -1)
		dvb_perror("FE_SET_TONE");
	return rc;
}

int dvb_fe_lnb_high_voltage(struct dvb_v5_fe_parms *parms, int on)
{
	int rc;

	if (on) on = 1;
	if (parms->verbose)
		dvb_log( "DiSEqC HIGH LNB VOLTAGE: %s", on ? "ON" : "OFF" );
	rc = ioctl(parms->fd, FE_ENABLE_HIGH_LNB_VOLTAGE, on);
	if (rc == -1)
		dvb_perror("FE_ENABLE_HIGH_LNB_VOLTAGE");
	return rc;
}

int dvb_fe_diseqc_burst(struct dvb_v5_fe_parms *parms, int mini_b)
{
	fe_sec_mini_cmd_t mini;
	int rc;

	mini = mini_b ? SEC_MINI_B : SEC_MINI_A;

	if (parms->verbose)
		dvb_log( "DiSEqC BURST: %s", mini_b ? "SEC_MINI_B" : "SEC_MINI_A" );
	rc = ioctl(parms->fd, FE_DISEQC_SEND_BURST, mini);
	if (rc == -1)
		dvb_perror("FE_DISEQC_SEND_BURST");
	return rc;
}

int dvb_fe_diseqc_cmd(struct dvb_v5_fe_parms *parms, const unsigned len,
		      const unsigned char *buf)
{
	struct dvb_diseqc_master_cmd msg;
	int rc;

	if (len > 6)
		return -EINVAL;

	msg.msg_len = len;
	memcpy(msg.msg, buf, len);

	if (parms->verbose) {
		int i;
		char log[len * 3 + 20], *p = log;

		p += sprintf(p, "DiSEqC command: ");
		for (i = 0; i < len; i++)
			p += sprintf (p, "%02x ", buf[i]);
		dvb_log("%s", log);
	}

	rc = ioctl(parms->fd, FE_DISEQC_SEND_MASTER_CMD, &msg);
	if (rc == -1)
		dvb_perror("FE_DISEQC_SEND_MASTER_CMD");
	return rc;
}

int dvb_fe_diseqc_reply(struct dvb_v5_fe_parms *parms, unsigned *len, char *buf,
		       int timeout)
{
	struct dvb_diseqc_slave_reply reply;
	int rc;

	if (*len > 4)
		*len = 4;

	reply.timeout = timeout;
	reply.msg_len = *len;

	if (parms->verbose)
		dvb_log("DiSEqC FE_DISEQC_RECV_SLAVE_REPLY");

	rc = ioctl(parms->fd, FE_DISEQC_RECV_SLAVE_REPLY, reply);
	if (rc == -1) {
		dvb_perror("FE_DISEQC_RECV_SLAVE_REPLY");
		return rc;
	}

	*len = reply.msg_len;
	memcpy(buf, reply.msg, reply.msg_len);

	return 0;
}
