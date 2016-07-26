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

#ifndef _DVB_DEV_H
#define _DVB_DEV_H

#include "dvb-fe.h"

/**
 * @file dvb-dev.h
 * @ingroup dvb_device
 * @brief Provides interfaces to handle Digital TV devices.
 * @copyright GNU Lesser General Public License version 2.1 (LGPLv2.1)
 * @author Mauro Carvalho Chehab
 *
 * Digital TV device node file names depend on udev configuration. For
 * example, while frontends are typically found at/dev/dvb/adapter?/frontend?,
 * the actual file name can vary from system to system, depending on the
 * udev ruleset.
 *
 * The libdvbv5 provides a set of functions to allow detecting and getting
 * the device paths in a sane way, via libudev.
 *
 * @par Bug Report
 * Please submit bug reports and patches to linux-media@vger.kernel.org
 */

/**
 * @enum dvb_dev_type
 *	@brief Type of a device entry to search
 * @ingroup dvb_device
 *
 * @param DVB_DEVICE_FRONTEND	Digital TV frontend
 * @param DVB_DEVICE_DEMUX	Digital TV demux
 * @param DVB_DEVICE_DVR	Digital TV Digital Video Record
 * @param DVB_DEVICE_NET	Digital TV network interface control
 * @param DVB_DEVICE_CA		Digital TV Conditional Access
 * @param DVB_DEVICE_CA_SEC	Digital TV Conditional Access serial
 */
enum dvb_dev_type {
	DVB_DEVICE_FRONTEND,
	DVB_DEVICE_DEMUX,
	DVB_DEVICE_DVR,
	DVB_DEVICE_NET,
	DVB_DEVICE_CA,
	DVB_DEVICE_CA_SEC,
};

/**
 * @struct dvb_dev_list
 *	@brief Digital TV device node properties
 * @ingroup dvb_device
 *
 * @param path		path for the /dev file handler
 * @param sysname	Kernel's system name for the device (dvb?.frontend?,
 *			for example)
 * @param dvb_type	type of the DVB device, as defined by enum dvb_dev_type
 * @param bus_addr	address of the device at the bus. For USB devices,
 *			it will be like: usb:3-1.1.4; for PCI devices:
 *			pci:0000:01:00.0)
 * @param bus_id	Id of the device at the bus (optional, PCI ID or USB ID)
 * @param manufacturer	Device's manufacturer name (optional, only on USB)
 * @param product	Device's product name (optional, only on USB)
 * @param serial	Device's serial name (optional, only on USB)
 */
struct dvb_dev_list {
	char *syspath;
	char *path;
	char *sysname;
	enum dvb_dev_type dvb_type;
	char *bus_addr;
	char *bus_id;
	char *manufacturer;
	char *product;
	char *serial;
};

/**
 * @struct dvb_device
 *	@brief Digital TV list of devices
 * @ingroup dvb_device
 *
 * @param devices	Array with a dvb_dev_list of devices. Each device
 *			node is a different entry at the list.
 * @param num_devices	number of elements at the devices array.
 */
struct dvb_device {
	/* Digital TV device lists */
	struct dvb_dev_list *devices;
	int num_devices;

	/* Digital TV frontend access */
	struct dvb_v5_fe_parms *fe_parms;
};

/**
 * @brief Allocate a struct dvb_device
 * @ingroup dvb_device
 *
 * @note Before using the dvb device function calls, the struct dvb_device should
 * be allocated via this function call.
 *
 * @return on success, returns a pointer to the allocated struct dvb_device or
 *	NULL if not enough memory to allocate the struct.
 */
struct dvb_device *dvb_dev_alloc(void);

/**
 * @brief free a struct dvb_device
 * @ingroup dvb_device
 *
 * @param dvb pointer to struct dvb_device to be freed
 */
void dvb_dev_free(struct dvb_device *dvb);

/**
 * @brief finds all DVB devices on the local machine
 * @ingroup dvb_device
 *
 * @param dvb			pointer to struct dvb_device to be filled
 * @param enable_monitor	if different than zero put the routine into
 *				monitor mode
 *
 * This routine can be called on two modes: normal or monitor mode
 *
 * In normal mode, it will seek for the local Digital TV devices, store them
 * at the struct dvb_device and return.
 *
 * In monitor mode, it will not only enumerate all devices, but it will also
 * keep waiting for device changes. The device seek loop will only be
 * interrupted after calling dvb_dev_stop_monitor().
 *
 * Please notice that, in such mode, the function will wait forever. So, it
 * is up to the application to put start a separate thread to handle it in
 * monitor mode, and add the needed mutexes to make it thread safe.
 */
int dvb_dev_find(struct dvb_device *dvb, int enable_monitor);

/**
 * @brief Find a device that matches the search criteria given by this
 *	functions's parameters.
 *
 * @param dvb		pointer to struct dvb_device to be used
 * @param adapter	Adapter number, as defined internally at the Kernel.
 *			Always start with 0;
 * @param num		Digital TV device number (e. g. frontend0, net0, etc);
 * @param type		Type of the device, as given by enum dvb_dev_type;
 */
struct dvb_dev_list *dvb_dev_seek_by_sysname(struct dvb_device *dvb,
					     unsigned int adapter,
					     unsigned int num,
					     enum dvb_dev_type type);

/**
 * @brief Stop the dvb_dev_find loop
 *
 * @param dvb pointer to struct dvb_device to be used
 *
 * This function stops dvb_dev_find() if it is running in monitor
 * mode. It does nothing on other modes. Can be called even if the
 * monitor mode was already stopped.
 */
void dvb_dev_stop_monitor(struct dvb_device *dvb);

/**
 * @brief Sets the DVB verbosity and log function
 *
 * @param dvb		pointer to struct dvb_device to be used
 * @param verbose	Verbosity level of the messages that will be printed
 * @param logfunc	Callback function to be called when a log event
 *			happens. Can either store the event into a file or
 *			to print it at the TUI/GUI. Can be null.
 *
 * @details Sets the function to report log errors and to set the verbosity
 *	level of debug report messages. If not called, or if logfunc is
 *	NULL, the libdvbv5 will report error and debug messages via stderr,
 *	and will use colors for the debug messages.
 *
 */
void dvb_dev_set_log(struct dvb_device *dvb,
		     unsigned verbose,
		     dvb_logfunc logfunc);

#endif
