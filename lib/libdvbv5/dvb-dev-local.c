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

#define _FILE_OFFSET_BITS 64
#define _LARGEFILE_SOURCE 1
#define _LARGEFILE64_SOURCE 1

#include <libudev.h>
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <pthread.h>
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

struct dvb_dev_local_priv {
	dvb_dev_change_t notify_dev_change;

	pthread_t dev_change_id;

	/* udev control fields */
	int udev_fd;
	struct udev *udev;
	struct udev_monitor *mon;
};

static int handle_device_change(struct dvb_device_priv *dvb,
				struct udev_device *dev,
				const char *syspath,
				const char *action)
{
	struct dvb_dev_local_priv *priv = dvb->priv;
	struct dvb_v5_fe_parms_priv *parms = (void *)dvb->d.fe_parms;
	struct udev_device *parent = NULL;
	struct dvb_dev_list dev_list, *dvb_dev;
	enum dvb_dev_change_type type;
	const char *bus_type, *p, *sysname;
	char *buf;
	int i, ret;

	/* remove, change, move should all remove the device first */
	if (!strcmp(action,"add")) {
		type = DVB_DEV_ADD;
	} else {
		sysname = udev_device_get_sysname(dev);
		if (!sysname) {
			dvb_logerr(_("udev_device_get_sysname failed"));
			return -ENODEV;
		}

		for (i = 0; i < dvb->d.num_devices; i++) {
			if (!strcmp(sysname, dvb->d.devices[i].sysname)) {
				memmove(&dvb->d.devices[i],
					&dvb->d.devices[i + 1],
					sizeof(*dvb->d.devices) * (dvb->d.num_devices - i));
				dvb->d.num_devices--;

				if (!dvb->d.num_devices) {
					free(dvb->d.devices);
					dvb->d.devices = NULL;
				} else {
					p = realloc(dvb->d.devices,
						sizeof(*dvb->d.devices) * dvb->d.num_devices);
					if (!p) {
						dvb_logerr(_("Can't remove a device from the list of DVB devices"));
						return -ENODEV;
					}
				}
				break;
			}
		}

		/* Return, if the device was removed */
		if (!strcmp(action,"remove")) {
			if (priv->notify_dev_change)
				priv->notify_dev_change(strdup(sysname),
							DVB_DEV_REMOVE);
			return 0;
		}
		type = DVB_DEV_CHANGE;
	}

	/* Fill mandatory fields: path, sysname, dvb_type, bus_type */
	dvb_dev = &dev_list;
	memset(dvb_dev, 0, sizeof(*dvb_dev));

	if (!syspath) {
		syspath = udev_device_get_devnode(dev);
		if (!syspath) {
			dvb_logwarn(_("Can't get device node filename"));
			goto err;
		}
	}
	dvb_dev->syspath = strdup(syspath);

	p = udev_device_get_devnode(dev);
	if (!p || !*p) {
		dvb_logwarn(_("Can't get device node filename"));
		goto err;
	}
	dvb_dev->path = strdup(p);

	p = udev_device_get_property_value(dev, "DVB_DEVICE_TYPE");
	if (!p)
		goto err;
	for (i = 0; i < dev_type_names_size; i++) {
		if (!strcmp(p, dev_type_names[i])) {
			dvb_dev->dvb_type = i;
			break;
		}
	}
	if (i == dev_type_names_size) {
		dvb_logwarn(_("Ignoring device %s"), dvb_dev->path);
		goto err;
	}

	p = udev_device_get_sysname(dev);
	if (!p) {
		dvb_logwarn(_("Can't get sysname for device %s"), dvb_dev->path);
		goto err;
	}
	dvb_dev->sysname = strdup(p);

	parent = udev_device_get_parent(dev);
	if (!parent)
		goto added;

	bus_type = udev_device_get_subsystem(parent);
	if (!bus_type) {
		dvb_logwarn(_("Can't get bus type for device %s"), dvb_dev->path);
		goto added;
	}

	ret = asprintf(&buf, "%s:%s", bus_type, udev_device_get_sysname(parent));
	if (ret < 0) {
		dvb_logerr(_("error %d when storing bus address"), errno);
		goto err;
	}

	dvb_dev->bus_addr = buf;

	/* Add new element */
	dvb->d.num_devices++;
	dvb_dev = realloc(dvb->d.devices, sizeof(*dvb->d.devices) * dvb->d.num_devices);
	if (!dvb_dev) {
		dvb_logerr(_("Not enough memory to store the list of DVB devices"));
		goto err;
	}

	dvb->d.devices = dvb_dev;
	dvb->d.devices[dvb->d.num_devices - 1] = dev_list;
	dvb_dev = &dvb->d.devices[dvb->d.num_devices - 1];

	/* Get optional per-bus fields associated with the device parent */
	if (!strcmp(bus_type, "pci")) {
		const char *pci_dev, *pci_vend;
		char *p;

		pci_dev = udev_device_get_sysattr_value(parent, "subsystem_device");
		pci_vend = udev_device_get_sysattr_value(parent, "subsystem_vendor");

		if (!pci_dev || !pci_vend)
			goto added;

		p = strstr(pci_dev, "0x");
		if (p)
			pci_dev = p + 2;

		p = strstr(pci_vend, "0x");
		if (p)
			pci_vend = p + 2;

		ret = asprintf(&dvb_dev->bus_id, "%s:%s", pci_dev, pci_vend);
	} else if (!strcmp(bus_type, "usb") || !strcmp(bus_type, "usbdevice")) {
		const char *vend, *prod;

		vend = udev_device_get_sysattr_value(parent, "idVendor");
		prod = udev_device_get_sysattr_value(parent, "idProduct");

		if (vend && prod)
			ret = asprintf(&dvb_dev->bus_id, "%s:%s", vend, prod);

		p = udev_device_get_sysattr_value(parent,"manufacturer");
		if (p)
			dvb_dev->manufacturer = strdup(p);

		p = udev_device_get_sysattr_value(parent,"product");
		if (p)
			dvb_dev->product = strdup(p);

		p = udev_device_get_sysattr_value(parent, "serial");
		if (p)
			dvb_dev->serial = strdup(p);
	}
added:
	if (priv->notify_dev_change)
		priv->notify_dev_change(strdup(dvb_dev->sysname), type);
	dvb_dev_dump_device(_("Found dvb %s device: %s"), parms, dvb_dev);

	return 0;

err:
	free_dvb_dev(dvb_dev);
	return -ENODEV;
}

#ifdef HAVE_PTHREAD
static void *monitor_device_changes(void *privdata)
{
	struct dvb_device_priv *dvb = privdata;
	struct dvb_dev_local_priv *priv = dvb->priv;
	struct udev_device *dev;

	while (1) {
		fd_set fds;
		struct timeval tv;
		int ret;

		FD_ZERO(&fds);
		FD_SET(priv->udev_fd, &fds);
		tv.tv_sec = 1;
		tv.tv_usec = 0;

		ret = select(priv->udev_fd + 1, &fds, NULL, NULL, &tv);

		/* Check if our file descriptor has received data. */
		if (ret > 0 && FD_ISSET(priv->udev_fd, &fds)) {
			dev = udev_monitor_receive_device(priv->mon);
			if (dev) {
				const char *action = udev_device_get_action(dev);
				handle_device_change(dvb, dev, NULL, action);
			}
		}
	}
	return NULL;
}
#endif

static int dvb_local_find(struct dvb_device_priv *dvb,
			  dvb_dev_change_t handler)
{
	struct dvb_v5_fe_parms_priv *parms = (void *)dvb->d.fe_parms;
	struct dvb_dev_local_priv *priv = dvb->priv;
	struct udev_enumerate *enumerate;
	struct udev_list_entry *devices, *dev_list_entry;
	struct udev_device *dev;
	int ret;

	/* Free a previous list of devices */
	if (dvb->d.num_devices)
		dvb_dev_free_devices(dvb);

	/* Create the udev object */
	priv->udev = udev_new();
	if (!priv->udev) {
		dvb_logerr(_("Can't create an udev object\n"));
		return -ENOMEM;
	}

	priv->notify_dev_change = handler;
	if (priv->notify_dev_change) {
#ifndef HAVE_PTHREAD
		dvb_logerr("Need to be compiled with pthreads for monitor");
		return -EINVAL;
#endif
		/* Set up a monitor to monitor dvb devices */
		priv->mon = udev_monitor_new_from_netlink(priv->udev, "udev");
		udev_monitor_filter_add_match_subsystem_devtype(priv->mon, "dvb", NULL);
		udev_monitor_enable_receiving(priv->mon);
		priv->udev_fd = udev_monitor_get_fd(priv->mon);
	}

	/* Create a list of the devices in the 'dvb' subsystem. */
	enumerate = udev_enumerate_new(priv->udev);
	udev_enumerate_add_match_subsystem(enumerate, "dvb");
	udev_enumerate_scan_devices(enumerate);
	devices = udev_enumerate_get_list_entry(enumerate);

	udev_list_entry_foreach(dev_list_entry, devices) {
		const char *syspath;

		syspath = udev_list_entry_get_name(dev_list_entry);

		dev = udev_device_new_from_syspath(priv->udev, syspath);
		handle_device_change(dvb, dev, syspath, "add");
		udev_device_unref(dev);
	}

	/* Free the enumerator object */
	udev_enumerate_unref(enumerate);

	/* Begin monitoring udev events */
#ifdef HAVE_PTHREAD
	if (priv->notify_dev_change) {
		ret = pthread_create(&priv->dev_change_id, NULL,
				     monitor_device_changes, dvb);
		if (ret < 0) {
			dvb_perror("pthread_create");
			return -1;
		}
	}
#endif
	if (!priv->notify_dev_change) {
		udev_unref(priv->udev);
		priv->udev = NULL;
	}

	return 0;
}

static int dvb_local_stop_monitor(struct dvb_device_priv *dvb)
{
	struct dvb_dev_local_priv *priv = dvb->priv;

#ifdef HAVE_PTHREAD
	if (priv->notify_dev_change) {
		pthread_cancel(priv->dev_change_id);
		udev_unref(priv->udev);
	}
#endif

	return 0;
}

struct dvb_dev_list *dvb_local_seek_by_sysname(struct dvb_device_priv *dvb,
					       unsigned int adapter,
					       unsigned int num,
					       enum dvb_dev_type type)
{
	struct dvb_v5_fe_parms_priv *parms = (void *)dvb->d.fe_parms;
	int ret, i;
	char *p;

	if (type > dev_type_names_size){
		dvb_logerr(_("Unexpected device type found!"));
		return NULL;
	}

	ret = asprintf(&p, "dvb%d.%s%d", adapter, dev_type_names[type], num);
	if (ret < 0) {
		dvb_logerr(_("error %d when seeking for device's filename"),
			   errno);
		return NULL;
	}
	for (i = 0; i < dvb->d.num_devices; i++) {
		if (!strcmp(p, dvb->d.devices[i].sysname)) {
			free(p);
			dvb_dev_dump_device(_("Selected dvb %s device: %s"),
					    parms, &dvb->d.devices[i]);
			return &dvb->d.devices[i];
		}
	}

	dvb_logwarn(_("device %s not found"), p);
	return NULL;
}

static struct dvb_open_descriptor
*dvb_local_open(struct dvb_device_priv *dvb,
		const char *sysname, int flags)
{
	struct dvb_v5_fe_parms_priv *parms = (void *)dvb->d.fe_parms;
	struct dvb_dev_list *dev = NULL;
	struct dvb_open_descriptor *open_dev, *cur;
	int ret, i;

	open_dev = calloc(1, sizeof(*open_dev));
	if (!open_dev) {
		dvb_perror("Can't create file descriptor");
		return NULL;
	}

	if (!sysname) {
		dvb_logerr(_("Device not specified"));
		free(open_dev);
		return NULL;
	}

	for (i = 0; i < dvb->d.num_devices; i++) {
		if (!strcmp(sysname, dvb->d.devices[i].sysname)) {
			dev = &dvb->d.devices[i];
			break;
		}
	}
	if (!dev) {
		dvb_logerr(_("Can't find device %s"), sysname);
		free(open_dev);
		return NULL;
	}

	if (dev->dvb_type == DVB_DEVICE_FRONTEND) {
		/*
		 * The frontend API was designed for sync frontend access.
		 * It is not ready to handle async frontend access.
		 */
		flags &= ~O_NONBLOCK;

		ret = dvb_fe_open_fname(parms, dev->path, flags);
		if (ret) {
			free(open_dev);
			return NULL;
		}
		ret = parms->fd;
	} else {
		/* We don't need special handling for other DVB device types */
		ret = open(dev->path, flags);
		if (ret == -1) {
			dvb_logerr(_("Can't open %s with flags %d: %d %m"),
				   dev->path, flags, errno);
			free(open_dev);
			return NULL;
		}
	}

	/* Add the fd to the open descriptor's list */
	open_dev->fd = ret;
	open_dev->dev = dev;
	open_dev->dvb = dvb;

	cur = &dvb->open_list;
	while (cur->next)
		cur = cur->next;
	cur->next = open_dev;

	return open_dev;
}

static int dvb_local_close(struct dvb_open_descriptor *open_dev)
{
	struct dvb_dev_list *dev = open_dev->dev;
	struct dvb_device_priv *dvb = open_dev->dvb;
	struct dvb_v5_fe_parms_priv *parms = (void *)dvb->d.fe_parms;
	struct dvb_open_descriptor *cur;

	if (dev->dvb_type == DVB_DEVICE_FRONTEND)
		__dvb_fe_close(parms);
	else {
		if (dev->dvb_type == DVB_DEVICE_DEMUX)
			dvb_dev_dmx_stop(open_dev);

		close(open_dev->fd);
	}

	for (cur = &dvb->open_list; cur->next; cur = cur->next) {
		if (cur->next == open_dev) {
			cur->next = open_dev->next;
			free(open_dev);
			return 0;
		}
	}

	/* Should never happen */
	dvb_logerr(_("Couldn't free device\n"));

	return -ENODEV;
}

#define MAX_TIME		10	/* 1.0 seconds */

#define xioctl(fh, request, arg...) ({					\
	int __rc;							\
	struct timespec __start, __end;					\
									\
	clock_gettime(CLOCK_MONOTONIC, &__start);			\
	do {								\
		__rc = ioctl(fh, request, ##arg);			\
		if (__rc != -1)						\
			break;						\
		if ((errno != EINTR) && (errno != EAGAIN))		\
			break;						\
		clock_gettime(CLOCK_MONOTONIC, &__end);			\
		if (__end.tv_sec * 10 + __end.tv_nsec / 100000000 >	\
		    __start.tv_sec * 10 + __start.tv_nsec / 100000000 +	\
		    MAX_TIME)						\
			break;						\
	} while (1);							\
									\
	__rc;								\
})

static int dvb_local_dmx_stop(struct dvb_open_descriptor *open_dev)
{
	struct dvb_dev_list *dev = open_dev->dev;
	struct dvb_device_priv *dvb = open_dev->dvb;
	struct dvb_v5_fe_parms_priv *parms = (void *)dvb->d.fe_parms;
	int ret, fd = open_dev->fd;

	if (dev->dvb_type != DVB_DEVICE_DEMUX)
		return -EINVAL;

	ret = xioctl(fd, DMX_STOP);
	if (ret == -1) {
		dvb_perror(_("DMX_STOP failed"));
		return -errno;
	}

	return 0;
}

static int dvb_local_set_bufsize(struct dvb_open_descriptor *open_dev,
			int buffersize)
{
	struct dvb_dev_list *dev = open_dev->dev;
	struct dvb_device_priv *dvb = open_dev->dvb;
	struct dvb_v5_fe_parms_priv *parms = (void *)dvb->d.fe_parms;
	int fd = open_dev->fd;

	if (dev->dvb_type != DVB_DEVICE_DEMUX && dev->dvb_type != DVB_DEVICE_DVR)
		return -EINVAL;

	if (xioctl(fd, DMX_SET_BUFFER_SIZE, buffersize) == -1) {
		dvb_perror(_("DMX_SET_BUFFER_SIZE failed"));
		return -errno;
	}

	return 0;
}

static ssize_t dvb_local_read(struct dvb_open_descriptor *open_dev,
		     void *buf, size_t count)
{
	struct dvb_dev_list *dev = open_dev->dev;
	struct dvb_device_priv *dvb = open_dev->dvb;
	struct dvb_v5_fe_parms_priv *parms = (void *)dvb->d.fe_parms;
	int fd = open_dev->fd;
	ssize_t ret;

	if (dev->dvb_type != DVB_DEVICE_DEMUX && dev->dvb_type != DVB_DEVICE_DVR) {
		dvb_logerr("Trying to read from an invalid device type on fd #%d", fd);
		return -EINVAL;
	}

	ret = read(fd, buf, count);
	if (ret == -1) {
		if (errno != EOVERFLOW)
			dvb_perror("read()");
		return -errno;
	}

	return ret;
}

static int dvb_local_dmx_set_pesfilter(struct dvb_open_descriptor *open_dev,
			      int pid, dmx_pes_type_t type,
			      dmx_output_t output, int bufsize)
{
	struct dvb_dev_list *dev = open_dev->dev;
	struct dvb_device_priv *dvb = open_dev->dvb;
	struct dvb_v5_fe_parms_priv *parms = (void *)dvb->d.fe_parms;
	struct dmx_pes_filter_params pesfilter;
	int fd = open_dev->fd;

	if (dev->dvb_type != DVB_DEVICE_DEMUX)
		return -EINVAL;

	/* Failing here is not fatal, so no need to handle error condition */
	if (bufsize)
		dvb_dev_set_bufsize(open_dev, bufsize);

	memset(&pesfilter, 0, sizeof(pesfilter));

	pesfilter.pid = pid;
	pesfilter.input = DMX_IN_FRONTEND;
	pesfilter.output = output;
	pesfilter.pes_type = type;
	pesfilter.flags = DMX_IMMEDIATE_START;

	if (xioctl(fd, DMX_SET_PES_FILTER, &pesfilter) == -1) {
		dvb_logerr(_("DMX_SET_PES_FILTER failed (PID = 0x%04x): %d %m"),
			   pid, errno);
		return -errno;
	}

	return 0;
}

static int dvb_local_dmx_set_section_filter(struct dvb_open_descriptor *open_dev,
				   int pid, unsigned filtsize,
				   unsigned char *filter,
				   unsigned char *mask,
				   unsigned char *mode,
				   unsigned int flags)
{
	struct dvb_dev_list *dev = open_dev->dev;
	struct dvb_device_priv *dvb = open_dev->dvb;
	struct dvb_v5_fe_parms_priv *parms = (void *)dvb->d.fe_parms;
	struct dmx_sct_filter_params sctfilter;
	int fd = open_dev->fd;

	if (dev->dvb_type != DVB_DEVICE_DEMUX)
		return -EINVAL;

	if (filtsize > DMX_FILTER_SIZE)
		filtsize = DMX_FILTER_SIZE;

	memset(&sctfilter, 0, sizeof(sctfilter));

	sctfilter.pid = pid;

	if (filter)
		memcpy(sctfilter.filter.filter, filter, filtsize);
	if (mask)
		memcpy(sctfilter.filter.mask, mask, filtsize);
	if (mode)
		memcpy(sctfilter.filter.mode, mode, filtsize);

	sctfilter.flags = flags;

	if (xioctl(fd, DMX_SET_FILTER, &sctfilter) == -1) {
		dvb_logerr(_("DMX_SET_FILTER failed (PID = 0x%04x): %d %m"),
			pid, errno);
		return -errno;
	}

	return 0;
}

static int dvb_local_dmx_get_pmt_pid(struct dvb_open_descriptor *open_dev, int sid)
{
	struct dvb_dev_list *dev = open_dev->dev;
	struct dvb_device_priv *dvb = open_dev->dvb;
	struct dvb_v5_fe_parms_priv *parms = (void *)dvb->d.fe_parms;
	struct dmx_sct_filter_params f;
	unsigned char buft[4096];
	unsigned char *buf = buft;
	int fd = open_dev->fd;
	int count;
	int pmt_pid = 0;
	int patread = 0;
	int section_length;

	if (dev->dvb_type != DVB_DEVICE_DEMUX)
		return -EINVAL;

	memset(&f, 0, sizeof(f));
	f.pid = 0;
	f.filter.filter[0] = 0x00;
	f.filter.mask[0] = 0xff;
	f.timeout = 0;
	f.flags = DMX_IMMEDIATE_START | DMX_CHECK_CRC;

	if (xioctl(fd, DMX_SET_FILTER, &f) == -1) {
		dvb_perror("ioctl DMX_SET_FILTER failed");
		return -errno;
	}

	while (!patread){
		if (((count = read(fd, buf, sizeof(buft))) < 0) && errno == EOVERFLOW)
		count = read(fd, buf, sizeof(buft));
		if (count < 0) {
		dvb_perror("read_sections: read error");
		return -errno;
		}

		section_length = ((buf[1] & 0x0f) << 8) | buf[2];
		if (count != section_length + 3)
		continue;

		buf += 8;
		section_length -= 8;

		patread = 1; /* assumes one section contains the whole pat */
		while (section_length > 0) {
		int service_id = (buf[0] << 8) | buf[1];
		if (service_id == sid) {
			pmt_pid = ((buf[2] & 0x1f) << 8) | buf[3];
			section_length = 0;
		}
		buf += 4;
		section_length -= 4;
		}
	}

	return pmt_pid;
}

static struct dvb_v5_descriptors *dvb_local_scan(struct dvb_open_descriptor *open_dev,
					struct dvb_entry *entry,
					check_frontend_t *check_frontend,
					void *args,
					unsigned other_nit,
					unsigned timeout_multiply)
{
	struct dvb_dev_list *dev = open_dev->dev;
	struct dvb_device_priv *dvb = open_dev->dvb;
	struct dvb_v5_fe_parms_priv *parms = (void *)dvb->d.fe_parms;
	struct dvb_v5_descriptors *desc;
	int fd = open_dev->fd;

	if (dev->dvb_type != DVB_DEVICE_DEMUX) {
		dvb_logerr(_("dvb_dev_scan: expecting a demux descriptor"));
		return NULL;
	}

	desc = dvb_scan_transponder(dvb->d.fe_parms, entry, fd, check_frontend,
				    args, other_nit, timeout_multiply);

	return desc;
}

/* Frontend functions that can be overriden */

int dvb_local_fe_set_sys(struct dvb_v5_fe_parms *p, fe_delivery_system_t sys)
{
	return __dvb_set_sys(p, sys);
}

int dvb_local_fe_get_parms(struct dvb_v5_fe_parms *p)
{
	return __dvb_fe_get_parms(p);
}

int dvb_local_fe_set_parms(struct dvb_v5_fe_parms *p)
{
	return __dvb_fe_set_parms(p);
}

int dvb_local_fe_get_stats(struct dvb_v5_fe_parms *p)
{
	return __dvb_fe_get_stats(p);
}


static void dvb_dev_local_free(struct dvb_device_priv *dvb)
{
	struct dvb_dev_local_priv *priv = dvb->priv;

	dvb_local_stop_monitor(dvb);

	free(priv);
}

/* Initialize for local usage */
void dvb_dev_local_init(struct dvb_device_priv *dvb)
{
	struct dvb_dev_ops *ops = &dvb->ops;

	dvb->priv = calloc(1, sizeof(struct dvb_dev_local_priv));

	ops->find = dvb_local_find;
	ops->seek_by_sysname = dvb_local_seek_by_sysname;
	ops->stop_monitor = dvb_local_stop_monitor;
	ops->open = dvb_local_open;
	ops->close = dvb_local_close;

	ops->dmx_stop = dvb_local_dmx_stop;
	ops->set_bufsize = dvb_local_set_bufsize;
	ops->read = dvb_local_read;
	ops->dmx_set_pesfilter = dvb_local_dmx_set_pesfilter;
	ops->dmx_set_section_filter = dvb_local_dmx_set_section_filter;
	ops->dmx_get_pmt_pid = dvb_local_dmx_get_pmt_pid;

	ops->scan = dvb_local_scan;

	ops->fe_set_sys = dvb_local_fe_set_sys;
	ops->fe_get_parms = dvb_local_fe_get_parms;
	ops->fe_set_parms = dvb_local_fe_set_parms;
	ops->fe_get_stats = dvb_local_fe_get_stats;

	ops->free = dvb_dev_local_free;
}
