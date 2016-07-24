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

/**
 * @file dvb-dev.h
 * @ingroup dvb_device
 * @brief Provides interfaces to handle Digital TV devices enumeration.
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
 * @struct dvb_device_list
 *	@brief Digital TV device node properties
 * @ingroup dvb_device
 *
 * @param path		path for the /dev file handler
 * @param sysname	Kernel's system name for the device (dvb?.frontend?,
 *			for example)
 * @param dvb_type	type of the DVB device (sec, frontend, demux, dvr, ca
 *			or net)
 * @param bus_addr	address of the device at the bus. For USB devices,
 *			it will be like: usb:3-1.1.4; for PCI devices:
 *			pci:0000:01:00.0)
 * @param bus_id	Id of the device at the bus (optional, PCI ID or USB ID)
 * @param manufacturer	Device's manufacturer name (optional, only on USB)
 * @param product	Device's product name (optional, only on USB)
 * @param serial	Device's serial name (optional, only on USB)
 */
struct dvb_device_list {
	char *path;
	char *sysname;
	char *dvb_type;
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
 * @param devices	Array with a dvb_device_list of devices. Each device
 *			node is a different entry at the list.
 * @param num_devices	number of elements at the devices array.
 */
struct dvb_device {
	struct dvb_device_list *devices;
	int num_devices;
};

/**
 * @enum dvb_type
 *	@brief Type of a device entry to search
 * @ingroup dvb_device
 *
 * @param DVB_DEVICE_FRONTEND	Digital TV frontend
 * @param DVB_DEVICE_DEMUX	Digital TV demux
 * @param DVB_DEVICE_DVR	Digital TV Digital Video Record
 * @param DVB_DEVICE_NET	Digital TV network interface control
 * @param DVB_DEVICE_CA		Digital TV Conditional Access
 */
enum dvb_type {
	DVB_DEVICE_FRONTEND,
	DVB_DEVICE_DEMUX,
	DVB_DEVICE_DVR,
	DVB_DEVICE_NET,
	DVB_DEVICE_CA,
};

/**
 * @brief Allocate a struct dvb_device
 * @ingroup dvb_device
 *
 * @note Before using the dvb device function calls, the struct dvb_device should
 * be allocated via this function call.
 */
struct dvb_device *alloc_dvb_device(void);

/**
 * @brief free a struct dvb_device
 * @ingroup dvb_device
 *
 * @param dvb pointer to struct dvb_device to be freed
 */
void free_dvb_device(struct dvb_device *dvb);

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
 * interrupted after calling stop_monitor_mode().
 *
 * Please notice that, in such mode, the function will wait forever. So, it
 * is up to the application to put start a separate thread to handle it in
 * monitor mode, and add the needed mutexes to make it thread safe.
 */
int find_dvb_devices(struct dvb_device *dvb, int enable_monitor);

/**
 * @brief Find a device that matches the search criteria given by this
 *	functions's parameters.
 *
 * @param dvb		pointer to struct dvb_device to be used
 *
 * @param adapter	Adapter number, as defined internally at the Kernel.
 *			Always start with 0;
 * @param num		Digital TV device number (e. g. frontend0, net0, etc);
 * @param type		Type of the device, as given by enum dvb_type;
 */
struct dvb_device_list *get_device_by_sysname(struct dvb_device *dvb,
					   unsigned int adapter,
					   unsigned int num,
					   enum dvb_type type);

/**
 * @brief Stop the find_dvb_devices loop
 *
 * @param dvb pointer to struct dvb_device to be used
 *
 * This function stops find_dvb_devices() if it is running in monitor
 * mode. It does nothing on other modes. Can be called even if the
 * monitor mode was already stopped.
 */
void stop_monitor_mode(struct dvb_device *dvb);
