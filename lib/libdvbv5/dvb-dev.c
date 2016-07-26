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
#include <libdvbv5/dvb-dev.h>

#ifdef ENABLE_NLS
# include "gettext.h"
# include <libintl.h>
# define _(string) dgettext(LIBDVBV5_DOMAIN, string)
#else
# define _(string) string
#endif

struct dvb_device_priv {
	struct dvb_device d;

	volatile int monitor;

	/* udev control fields */
	struct udev *udev;
	struct udev_monitor *mon;
};

static void free_dvb_dev(struct dvb_dev_list *dvb_dev)
{
	if (dvb_dev->path)
		free (dvb_dev->path);
	if (dvb_dev->syspath)
		free (dvb_dev->syspath);
	if (dvb_dev->sysname)
		free(dvb_dev->sysname);
	if (dvb_dev->bus_addr)
		free(dvb_dev->bus_addr);
	if (dvb_dev->manufacturer)
		free(dvb_dev->manufacturer);
	if (dvb_dev->product)
		free(dvb_dev->product);
	if (dvb_dev->serial)
		free(dvb_dev->serial);
}

struct dvb_device *dvb_dev_alloc(void)
{
	struct dvb_device *dvb;

	dvb = calloc(1, sizeof(struct dvb_device_priv));
	if (!dvb)
		return NULL;

	dvb->fe_parms = dvb_fe_dummy();
	if (!dvb->fe_parms) {
		dvb_dev_free(dvb);
		return NULL;
	}

	return dvb;
}

static void dvb_dev_free_devices(struct dvb_device_priv *dvb)
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

	dvb_dev_free_devices(dvb);

	/* Wait for dvb_dev_find() to stop */
	while (dvb->udev) {
		dvb->monitor = 0;
		usleep(1000);
	}
	dvb_fe_close(dvb->d.fe_parms);
	free(dvb);
}

static const char * const dev_type_names[] = {
        "frontend", "demux", "dvr", "net", "ca", "sec"
};

static void dump_device(char *msg,
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
	struct dvb_v5_fe_parms_priv *parms = (void *)dvb->d.fe_parms;
	int ret, i;
	char *p;

	if (type > sizeof(dev_type_names)/sizeof(*dev_type_names)){
		dvb_logerr(_("Unexpected device type found!"));
		return NULL;
	}

	ret = asprintf(&p, "dvb%d.%s%d", adapter, dev_type_names[type], num);
	if (ret < 0) {
		dvb_logerr(_("error %d when seeking for device's filename"), errno);
		return NULL;
	}
	for (i = 0; i < dvb->d.num_devices; i++) {
		if (!strcmp(p, dvb->d.devices[i].sysname)) {
			free(p);
			dump_device(_("Selected dvb %s device: %s"), parms,
				    &dvb->d.devices[i]);
			return &dvb->d.devices[i];
		}
	}

	dvb_logwarn(_("device %s not found"), p);
	return NULL;
}

static int handle_device_change(struct dvb_device_priv *dvb,
				struct udev_device *dev,
				const char *syspath,
				const char *action)
{
	struct dvb_v5_fe_parms_priv *parms = (void *)dvb->d.fe_parms;
	struct udev_device *parent = NULL;
	struct dvb_dev_list dev_list, *dvb_dev;
	const char *bus_type, *p;
	char *buf;
	int i, ret, len;

	/* remove, change, move should all remove the device first */
	if (strcmp(action,"add")) {
		p = udev_device_get_sysname(dev);
		if (!p) {
			dvb_logerr(_("udev_device_get_sysname failed"));
			return -1;
		}

		for (i = 0; i < dvb->d.num_devices; i++) {
			if (!strcmp(p, dvb->d.devices[i].sysname)) {
				memmove(&dvb->d.devices[i],
					&dvb->d.devices[i + 1],
					sizeof(*dvb->d.devices) * (dvb->d.num_devices - i));
				dvb->d.num_devices--;

				p = realloc(dvb->d.devices,
					    sizeof(*dvb->d.devices) * dvb->d.num_devices);
				if (!p) {
					dvb_logerr(_("Can't remove a device from the list of DVB devices"));
					return -2;
				}
				break;
			}
		}

		/* Return, if the device was removed */
		if (!strcmp(action,"remove"))
			return 0;
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
	if (!p) {
		dvb_logwarn(_("Can't get device node filename"));
		goto err;
	}
	dvb_dev->path = strdup(p);

	p = udev_device_get_property_value(dev, "DVB_DEVICE_TYPE");
	if (!p)
		goto err;
	len = sizeof(dev_type_names)/sizeof(*dev_type_names);
	for (i = 0; i < len; i++) {
		if (!strcmp(p, dev_type_names[i])) {
			dvb_dev->dvb_type = i;
			break;
		}
	}
	if (i == len) {
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
	dump_device(_("Found dvb %s device: %s"), parms, dvb_dev);

	return 0;

err:
	free_dvb_dev(dvb_dev);
	return -1;
}

int dvb_dev_find(struct dvb_device *d, int enable_monitor)
{
	struct dvb_device_priv *dvb = (void *)d;
	struct dvb_v5_fe_parms_priv *parms = (void *)dvb->d.fe_parms;
	struct udev_enumerate *enumerate;
	struct udev_list_entry *devices, *dev_list_entry;
	struct udev_device *dev;
	int fd = -1;

	/* Free a previous list of devices */
	if (dvb->d.num_devices)
		dvb_dev_free_devices(dvb);

	/* Create the udev object */
	dvb->udev = udev_new();
	if (!dvb->udev) {
		dvb_logerr(_("Can't create an udev object\n"));
		return -1;
	}

	dvb->monitor = enable_monitor;
	if (dvb->monitor) {
		/* Set up a monitor to monitor dvb devices */
		dvb->mon = udev_monitor_new_from_netlink(dvb->udev, "udev");
		udev_monitor_filter_add_match_subsystem_devtype(dvb->mon, "dvb", NULL);
		udev_monitor_enable_receiving(dvb->mon);
		fd = udev_monitor_get_fd(dvb->mon);
	}

	/* Create a list of the devices in the 'dvb' subsystem. */
	enumerate = udev_enumerate_new(dvb->udev);
	udev_enumerate_add_match_subsystem(enumerate, "dvb");
	udev_enumerate_scan_devices(enumerate);
	devices = udev_enumerate_get_list_entry(enumerate);

	udev_list_entry_foreach(dev_list_entry, devices) {
		const char *path;

		path = udev_list_entry_get_name(dev_list_entry);
		dev = udev_device_new_from_syspath(dvb->udev, path);

		handle_device_change(dvb, dev, path, "add");
		udev_device_unref(dev);
	}

	/* Free the enumerator object */
	udev_enumerate_unref(enumerate);

	/* Begin monitoring udev events */
	while (dvb->monitor) {
		fd_set fds;
		struct timeval tv;
		int ret;

		FD_ZERO(&fds);
		FD_SET(fd, &fds);
		tv.tv_sec = 1;
		tv.tv_usec = 0;

		ret = select(fd + 1, &fds, NULL, NULL, &tv);

		/* Check if our file descriptor has received data. */
		if (ret > 0 && FD_ISSET(fd, &fds)) {
			dev = udev_monitor_receive_device(dvb->mon);
			if (dev) {
				const char *action = udev_device_get_action(dev);
				handle_device_change(dvb, dev, NULL, action);
			}
		}
	}
	udev_unref(dvb->udev);
	dvb->udev = NULL;

	return 0;
}

void dvb_dev_stop_monitor(struct dvb_device *d)
{
	struct dvb_device_priv *dvb = (void *)d;

	dvb->monitor = 0;
}

void dvb_dev_set_log(struct dvb_device *dvb, unsigned verbose,
		     dvb_logfunc logfunc)
{
	struct dvb_v5_fe_parms_priv *parms = (void *)dvb->fe_parms;

	parms->p.verbose = verbose;

	if (logfunc != NULL)
			parms->p.logfunc = logfunc;
}

int dvb_dev_open(struct dvb_device *d, char *sysname, int flags)
{
	struct dvb_device_priv *dvb = (void *)d;
	struct dvb_v5_fe_parms_priv *parms = (void *)d->fe_parms;
	struct dvb_dev_list *dev = NULL;
	int ret, i;

	if (!sysname) {
		dvb_logerr(_("Device not specified"));
		return -ENOENT;
	}

	for (i = 0; i < dvb->d.num_devices; i++) {
		if (!strcmp(sysname, dvb->d.devices[i].sysname)) {
			dev = &dvb->d.devices[i];
			break;
		}
	}
	if (!dev) {
		dvb_logerr(_("Can't find device %s"), sysname);
		return -ENOENT;
	}

	if (dev->dvb_type == DVB_DEVICE_FRONTEND) {
		ret = dvb_fe_open_fname(parms, dev->path, flags);
		if (ret)
			return -ret;
		return parms->fd;
	}

	/* We don't need special handling for other DVB device types */
	ret = open(dev->path, flags);
	if (ret == -1)
		return -errno;

	return ret;
}
