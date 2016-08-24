/*
 * Copyright (c) 2016 - Mauro Carvalho Chehab
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
 */

#include <libudev.h>
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <unistd.h>
#include <string.h>

#include <config.h>

#include "dvb-fe-priv.h"
#include "dvb-dev-priv.h"

#ifdef ENABLE_NLS
# include "gettext.h"
# include <libintl.h>
# define _(string) dgettext(LIBDVBV5_DOMAIN, string)
#else
# define _(string) string
#endif

const char * const dev_type_names[] = {
        "frontend", "demux", "dvr", "net", "ca", "sec"
};

const unsigned int
dev_type_names_size = sizeof(dev_type_names)/sizeof(*dev_type_names);

void free_dvb_dev(struct dvb_dev_list *dvb_dev)
{
	if (dvb_dev->path)
		free(dvb_dev->path);
	if (dvb_dev->syspath)
		free(dvb_dev->syspath);
	if (dvb_dev->sysname)
		free(dvb_dev->sysname);
	if (dvb_dev->bus_addr)
		free(dvb_dev->bus_addr);
	if (dvb_dev->bus_id)
		free(dvb_dev->bus_id);
	if (dvb_dev->manufacturer)
		free(dvb_dev->manufacturer);
	if (dvb_dev->product)
		free(dvb_dev->product);
	if (dvb_dev->serial)
		free(dvb_dev->serial);
}

struct dvb_device *dvb_dev_alloc(void)
{
	struct dvb_device_priv *dvb;
	struct dvb_v5_fe_parms_priv *parms;

	dvb = calloc(1, sizeof(struct dvb_device_priv));
	if (!dvb)
		return NULL;

	dvb->d.fe_parms = dvb_fe_dummy();
	if (!dvb->d.fe_parms) {
		dvb_dev_free(&dvb->d);
		return NULL;
	}
	parms = (void *)dvb->d.fe_parms;
	parms->dvb = dvb;

	/* Initialize it to use the local DVB devices */
	dvb_dev_local_init(dvb);

	return &dvb->d;
}

void dvb_dev_free_devices(struct dvb_device_priv *dvb)
{
	int i;

	for (i = 0; i < dvb->d.num_devices; i++)
		free_dvb_dev(&dvb->d.devices[i]);
	free(dvb->d.devices);

	dvb->d.devices = NULL;
	dvb->d.num_devices = 0;
}

void dvb_dev_free(struct dvb_device *d)
{
	struct dvb_device_priv *dvb = (void *)d;
	struct dvb_open_descriptor *cur, *next;
	struct dvb_dev_ops *ops = &dvb->ops;

	/* Close all devices */
	cur = dvb->open_list.next;
	while (cur) {
		next = cur->next;
		dvb_dev_close(cur);
		cur = next;
	}

	/* Call an implementation-specific free method, if defined */
	if (ops->free)
		ops->free(dvb);

	dvb_fe_close(dvb->d.fe_parms);

	dvb_dev_free_devices(dvb);

	free(dvb);
}

void dvb_dev_dump_device(char *msg,
			struct dvb_v5_fe_parms_priv *parms,
			struct dvb_dev_list *dev)
{
	if (parms->p.verbose < 2)
		return;

	dvb_log(msg, dev_type_names[dev->dvb_type], dev->sysname);

	if (dev->path)
		dvb_log(_("  path: %s"), dev->path);
	if (dev->syspath)
		dvb_log(_("  sysfs path: %s"), dev->syspath);
	if (dev->bus_addr)
		dvb_log(_("  bus addr: %s"), dev->bus_addr);
	if (dev->bus_id)
		dvb_log(_("  bus ID: %s"), dev->bus_id);
	if (dev->manufacturer)
		dvb_log(_("  manufacturer: %s"), dev->manufacturer);
	if (dev->product)
		dvb_log(_("  product: %s"), dev->product);
	if (dev->serial)
		dvb_log(_("  serial: %s"), dev->serial);
}

struct dvb_dev_list *dvb_dev_seek_by_sysname(struct dvb_device *d,
					     unsigned int adapter,
					     unsigned int num,
					     enum dvb_dev_type type)
{
	struct dvb_device_priv *dvb = (void *)d;
	struct dvb_dev_ops *ops = &dvb->ops;

	if (!ops->seek_by_sysname)
		return NULL;

	return ops->seek_by_sysname(dvb, adapter, num, type);
}

void dvb_dev_set_log(struct dvb_device *dvb, unsigned verbose,
		     dvb_logfunc logfunc)
{
	struct dvb_v5_fe_parms_priv *parms = (void *)dvb->fe_parms;

	/* FIXME: how to get remote logs and set verbosity? */
	parms->p.verbose = verbose;

	if (logfunc != NULL)
			parms->p.logfunc = logfunc;
}

int dvb_dev_find(struct dvb_device *d, dvb_dev_change_t handler)
{
	struct dvb_device_priv *dvb = (void *)d;
	struct dvb_dev_ops *ops = &dvb->ops;

	if (!ops->find)
		return -1;

	return ops->find(dvb, handler);
}

void dvb_dev_stop_monitor(struct dvb_device *d)
{
	struct dvb_device_priv *dvb = (void *)d;
	struct dvb_dev_ops *ops = &dvb->ops;

	if (ops->stop_monitor)
		ops->stop_monitor(dvb);
}

struct dvb_open_descriptor *dvb_dev_open(struct dvb_device *d,
					 const char *sysname, int flags)
{
	struct dvb_device_priv *dvb = (void *)d;
	struct dvb_dev_ops *ops = &dvb->ops;

	if (!ops->open)
		return NULL;

	return ops->open(dvb, sysname, flags);
}

void dvb_dev_close(struct dvb_open_descriptor *open_dev)
{
	struct dvb_device_priv *dvb = open_dev->dvb;
	struct dvb_dev_ops *ops = &dvb->ops;

	if (ops->close)
		ops->close(open_dev);
}

void dvb_dev_dmx_stop(struct dvb_open_descriptor *open_dev)
{
	struct dvb_device_priv *dvb = open_dev->dvb;
	struct dvb_dev_ops *ops = &dvb->ops;

	if (ops->dmx_stop)
		ops->dmx_stop(open_dev);
}

int dvb_dev_set_bufsize(struct dvb_open_descriptor *open_dev,
			int buffersize)
{
	struct dvb_device_priv *dvb = open_dev->dvb;
	struct dvb_dev_ops *ops = &dvb->ops;

	if (!ops->set_bufsize)
		return -1;

	return ops->set_bufsize(open_dev, buffersize);
}

ssize_t dvb_dev_read(struct dvb_open_descriptor *open_dev,
		     void *buf, size_t count)
{
	struct dvb_device_priv *dvb = open_dev->dvb;
	struct dvb_dev_ops *ops = &dvb->ops;

	if (!ops->read)
		return -1;

	return ops->read(open_dev, buf, count);
}

int dvb_dev_dmx_set_pesfilter(struct dvb_open_descriptor *open_dev,
			      int pid, dmx_pes_type_t type,
			      dmx_output_t output, int bufsize)
{
	struct dvb_device_priv *dvb = open_dev->dvb;
	struct dvb_dev_ops *ops = &dvb->ops;

	if (!ops->dmx_set_pesfilter)
		return -1;

	return ops->dmx_set_pesfilter(open_dev, pid, type, output, bufsize);
}

int dvb_dev_dmx_set_section_filter(struct dvb_open_descriptor *open_dev,
				   int pid, unsigned filtsize,
				   unsigned char *filter,
				   unsigned char *mask,
				   unsigned char *mode,
				   unsigned int flags)
{
	struct dvb_device_priv *dvb = open_dev->dvb;
	struct dvb_dev_ops *ops = &dvb->ops;

	if (!ops->dmx_set_section_filter)
		return -1;

	return ops->dmx_set_section_filter(open_dev, pid, filtsize, filter,
					   mask, mode, flags);
}

int dvb_dev_dmx_get_pmt_pid(struct dvb_open_descriptor *open_dev, int sid)
{
	struct dvb_device_priv *dvb = open_dev->dvb;
	struct dvb_dev_ops *ops = &dvb->ops;

	if (!ops->dmx_get_pmt_pid)
		return -1;

	return ops->dmx_get_pmt_pid(open_dev, sid);
}

struct dvb_v5_descriptors *dvb_dev_scan(struct dvb_open_descriptor *open_dev,
					struct dvb_entry *entry,
					check_frontend_t *check_frontend,
					void *args,
					unsigned other_nit,
					unsigned timeout_multiply)
{
	struct dvb_device_priv *dvb = open_dev->dvb;
	struct dvb_dev_ops *ops = &dvb->ops;

	if (!ops->scan)
		return NULL;

	return ops->scan(open_dev, entry, check_frontend, args, other_nit,
			 timeout_multiply);
}

/* Frontend functions that can be overriden */

int dvb_set_sys(struct dvb_v5_fe_parms *p, fe_delivery_system_t sys)
{
	struct dvb_v5_fe_parms_priv *parms = (void *)p;
	struct dvb_device_priv *dvb = parms->dvb;

	if (!dvb || !dvb->ops.fe_set_sys)
		return __dvb_set_sys(p, sys);

	return dvb->ops.fe_set_sys(p, sys);
}

int dvb_fe_get_parms(struct dvb_v5_fe_parms *p)
{
	struct dvb_v5_fe_parms_priv *parms = (void *)p;
	struct dvb_device_priv *dvb = parms->dvb;

	if (!dvb || !dvb->ops.fe_get_parms)
		return __dvb_fe_get_parms(p);

	return dvb->ops.fe_get_parms(p);
}

int dvb_fe_set_parms(struct dvb_v5_fe_parms *p)
{
	struct dvb_v5_fe_parms_priv *parms = (void *)p;
	struct dvb_device_priv *dvb = parms->dvb;

	if (!dvb || !dvb->ops.fe_set_parms)
		return __dvb_fe_set_parms(p);

	return dvb->ops.fe_set_parms(p);
}

int dvb_fe_get_stats(struct dvb_v5_fe_parms *p)
{
	struct dvb_v5_fe_parms_priv *parms = (void *)p;
	struct dvb_device_priv *dvb = parms->dvb;

	if (!dvb || !dvb->ops.fe_get_stats)
		return __dvb_fe_get_stats(p);

	return dvb->ops.fe_get_stats(p);
}
