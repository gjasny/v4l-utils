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
#include "dvb-scan.h"

#include <linux/dvb/dmx.h>

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
	DVB_DEVICE_VIDEO,
	DVB_DEVICE_AUDIO,
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
 * @enum dvb_dev_change_type
 *	@brief Describes the type of change to be notifier_delay
 *
 * @param DVB_DEV_ADD		New device detected
 * @param DVB_DEV_CHANGE	Device has changed something
 * @param DVB_DEV_REMOVE	A hot-pluggable device was removed
 */
enum dvb_dev_change_type {
	DVB_DEV_ADD,
	DVB_DEV_CHANGE,
	DVB_DEV_REMOVE,
};

/**
 * @brief Describes a callback for dvb_dev_find()
 *
 * sysname:	Kernel's system name for the device (dvb?.frontend?,
 *			for example)
 * type:	type of change, as defined by enum dvb_dev_change_type
 *
 * @note: the returned string should be freed with free().
 */
typedef int (*dvb_dev_change_t)(char *sysname,
				enum dvb_dev_change_type type, void *priv);

/**
 * @struct dvb_open_descriptor
 *
 * Opaque struct with a DVB open file descriptor
 */
struct dvb_open_descriptor;

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
 * @param handler		if not NULL, then this is called whenever
 *				something changed
 *				monitor mode
 * @param user_priv		pointer to user private data
 *
 * This routine can be called on two modes: normal or monitor mode
 *
 * In normal mode, it will seek for the local Digital TV devices, store them
 * at the struct dvb_device and return.
 *
 * In monitor mode, it will not only enumerate all devices, but it will also
 * keep waiting for device changes. The handler callback is called for the
 * changed device.
 *
 * @return returns 0 on success, a negative value otherwise.
 */
int dvb_dev_find(struct dvb_device *dvb, dvb_dev_change_t handler,
		 void *user_priv);

/**
 * @brief Find a device that matches the search criteria given by this
 *	functions's parameters.
 *
 * @param dvb		pointer to struct dvb_device to be used
 * @param adapter	Adapter number, as defined internally at the Kernel.
 *			Always start with 0;
 * @param num		Digital TV device number (e. g. frontend0, net0, etc);
 * @param type		Type of the device, as given by enum dvb_dev_type;
 *
 * @return returns a pointer to a struct dvb_dev_list object or NULL if the
 *	desired device was not found.
 */
struct dvb_dev_list *dvb_dev_seek_by_adapter(struct dvb_device *dvb,
					     unsigned int adapter,
					     unsigned int num,
					     enum dvb_dev_type type);

/**
 * @brief Return data about a device from its sysname
 *
 * @param dvb		pointer to struct dvb_device to be used
 * @param sysname	Kernel's name of the device to be opened, as obtained
 *			via dvb_dev_seek_by_adapter() or via dvb_dev_find().
 *
 * @return returns a pointer to a struct dvb_dev_list object or NULL if the
 *	desired device was not found.
 */
struct dvb_dev_list *dvb_get_dev_info(struct dvb_device *dvb,
				      const char *sysname);

/**
 * @brief Stop the dvb_dev_find loop
 * @ingroup dvb_device
 *
 * @param dvb pointer to struct dvb_device to be used
 *
 * This function stops dvb_dev_find() if it is running in monitor
 * mode. It does nothing on other modes. Can be called even if the
 * monitor mode was already stopped.
 */
void dvb_dev_stop_monitor(struct dvb_device *dvb);

/**
 * @brief Sets the DVB verbosity and log function with context private data
 * @ingroup dvb_device
 *
 * @param dvb		pointer to struct dvb_device to be used
 * @param verbose	Verbosity level of the messages that will be printed
 * @param logfunc	Callback function to be called when a log event
 *			happens. Can either store the event into a file or
 *			to print it at the TUI/GUI. Can be null.
 * @param logpriv   Private data for log function
 *
 * @details Sets the function to report log errors and to set the verbosity
 *	level of debug report messages. If not called, or if logfunc is
 *	NULL, the libdvbv5 will report error and debug messages via stderr,
 *	and will use colors for the debug messages.
 *
 */
void dvb_dev_set_logpriv(struct dvb_device *dvb,
		     unsigned verbose,
		     dvb_logfunc_priv logfunc, void *logpriv);

/**
 * @brief Sets the DVB verbosity and log function
 * @ingroup dvb_device
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

/**
 * @brief Opens a dvb device
 * @ingroup dvb_device
 *
 * @param dvb		pointer to struct dvb_device to be used
 * @param sysname	Kernel's name of the device to be opened, as obtained
 *			via dvb_dev_seek_by_adapter() or via dvb_dev_find().
 * @param flags		Flags to be passed to open: O_RDONLY, O_RDWR and/or
 *			O_NONBLOCK
 *
 *
 * @note Please notice that O_NONBLOCK is not supported for frontend devices,
 *	and will be silently ignored.
 *
 * @note the sysname will only work if a previous call to dvb_dev_find()
 * 	is issued.
 *
 * @details This function is equivalent to open(2) system call: it opens a
 *	Digital TV given by the dev parameter, using the flags.
 *
 * @return returns a pointer to the dvb_open_descriptor that should be used
 * 	on further calls if sucess. NULL otherwise.
 */
struct dvb_open_descriptor *dvb_dev_open(struct dvb_device *dvb,
					 const char *sysname, int flags);

/**
 * @brief Closes a dvb device
 * @ingroup dvb_device
 *
 * @param open_dev	Points to the struct dvb_open_descriptor to be
 * closed.
 */
void dvb_dev_close(struct dvb_open_descriptor *open_dev);

/**
 * @brief returns fd from a local device
 * This will not work for remote devices.
 * @ingroup dvb_device
 *
 * @param open_dev	Points to the struct dvb_open_descriptor
 *
 * @return On success, returns the fd.
 * Returns -1 on error.
 */
int dvb_dev_get_fd(struct dvb_open_descriptor *open_dev);

/**
 * @brief read from a dvb demux or dvr file
 * @ingroup dvb_device
 *
 * @param open_dev	Points to the struct dvb_open_descriptor to be
 * closed.
 * @param buf		Buffer to store the data
 * @param count		number of bytes to read
 *
 * @return On success, returns the number of bytes read. Returns -1 on
 * error.
 */
ssize_t dvb_dev_read(struct dvb_open_descriptor *open_dev,
		     void *buf, size_t count);

/**
 * @brief Stops the demux filter for a given file descriptor
 * @ingroup dvb_device
 *
 * @param open_dev	Points to the struct dvb_open_descriptor
 *
 * This is a wrapper function for DMX_STOP ioctl.
 *
 * See http://linuxtv.org/downloads/v4l-dvb-apis/dvb_demux.html
 * for more details.
 *
 * @note valid only for DVB_DEVICE_DEMUX.
 */
void dvb_dev_dmx_stop(struct dvb_open_descriptor *open_dev);

/**
 * @brief Start a demux or dvr buffer size
 * @ingroup dvb_device
 *
 * @param open_dev	Points to the struct dvb_open_descriptor
 * @param buffersize	Size of the buffer to be allocated to store the filtered data.
 *
 * This is a wrapper function for DMX_SET_BUFFER_SIZE ioctl.
 *
 * See http://linuxtv.org/downloads/v4l-dvb-apis/dvb_demux.html
 * for more details.
 *
 * @return Retuns zero on success, -1 otherwise.
 *
 * @note valid only for DVB_DEVICE_DEMUX or DVB_DEVICE_DVR.
 */
int dvb_dev_set_bufsize(struct dvb_open_descriptor *open_dev,
			int buffersize);

/**
 * @brief Start a filter for a MPEG-TS Packetized Elementary
 * 		       Stream (PES)
 * @ingroup dvb_device
 *
 * @param open_dev	Points to the struct dvb_open_descriptor
 * @param pid		Program ID to filter. Use 0x2000 to select all PIDs
 * @param type		type of the PID (DMX_PES_VIDEO, DMX_PES_AUDIO,
 *			DMX_PES_OTHER, etc).
 * @param output	Where the data will be output (DMX_OUT_TS_TAP,
 *			DMX_OUT_DECODER, etc).
 * @param buffersize	Size of the buffer to be allocated to store the filtered data.
 *
 * This is a wrapper function for DMX_SET_PES_FILTER and DMX_SET_BUFFER_SIZE
 * ioctls.
 *
 * See http://linuxtv.org/downloads/v4l-dvb-apis/dvb_demux.html
 * for more details.
 *
 * @return Retuns zero on success, -1 otherwise.
 *
 * @note valid only for DVB_DEVICE_DEMUX.
 */
int dvb_dev_dmx_set_pesfilter(struct dvb_open_descriptor *open_dev,
			      int pid, dmx_pes_type_t type,
			      dmx_output_t output, int buffersize);

/**
 * @brief Sets a MPEG-TS section filter
 * @ingroup dvb_device
 *
 * @param open_dev	Points to the struct dvb_open_descriptor
 * @param pid		Program ID to filter. Use 0x2000 to select all PIDs
 * @param filtsize	Size of the filter (up to 18 btyes)
 * @param filter	data to filter. Can be NULL or should have filtsize length
 * @param mask		filter mask. Can be NULL or should have filtsize length
 * @param mode		mode mask. Can be NULL or should have filtsize length
 * @param flags		flags for set filter (DMX_CHECK_CRC,DMX_ONESHOT,
 *			DMX_IMMEDIATE_START).
 *
 * This is a wrapper function for DMX_SET_FILTER ioctl.
 *
 * See http://linuxtv.org/downloads/v4l-dvb-apis/dvb_demux.html
 * for more details.
 *
 * @return Retuns zero on success, -1 otherwise.
 *
 * @note valid only for DVB_DEVICE_DEMUX.
 */
int dvb_dev_dmx_set_section_filter(struct dvb_open_descriptor *open_dev,
				   int pid, unsigned filtsize,
				   unsigned char *filter,
				   unsigned char *mask,
				   unsigned char *mode,
				   unsigned int flags);

/**
 * @brief read the contents of the MPEG-TS PAT table, seeking for
 *		      	an specific service ID
 * @ingroup dvb_device
 *
 * @param open_dev	Points to the struct dvb_open_descriptor
 * @param sid		Session ID to seeking
 *
 * @return At return, it returns a negative value if error or the PID
 * associated with the desired Session ID.
 *
 * @warning This function currently assumes that the PAT fits into one session.
 *
 * @note valid only for DVB_DEVICE_DEMUX.
 */
int dvb_dev_dmx_get_pmt_pid(struct dvb_open_descriptor *open_dev, int sid);

/**
 * @brief Scans a DVB dvb_add_scaned_transponder
 * @ingroup frontend_scan
 *
 * @param entry		DVB file entry that corresponds to a transponder to be
 * 			tuned
 * @param open_dev	Points to the struct dvb_open_descriptor
 * @param check_frontend a pointer to a function that will show the frontend
 *			status while tuning into a transponder
 * @param args		a pointer, opaque to libdvbv5, that will be used when
 *			calling check_frontend. It should contain any parameters
 *			that could be needed by check_frontend.
 * @param other_nit	Use alternate table IDs for NIT and other tables
 * @param timeout_multiply Improves the timeout for each table reception, by
 *
 * This is the function that applications should use when doing a transponders
 * scan. It does everything needed to fill the entries with DVB programs
 * (virtual channels) and detect the PIDs associated with them.
 *
 * This is the dvb_device variant of dvb_scan_transponder().
 */
struct dvb_v5_descriptors *dvb_dev_scan(struct dvb_open_descriptor *open_dev,
					struct dvb_entry *entry,
					check_frontend_t *check_frontend,
					void *args,
					unsigned other_nit,
					unsigned timeout_multiply);

/* From dvb-dev-remote.c */

#ifdef HAVE_DVBV5_REMOTE

#define REMOTE_BUF_SIZE (87 * 188)	/* 16356 bytes */


/**
 * @brief initialize the dvb-dev to use a remote device running the
 *	dvbv5-daemon.
 *
 * @param dvb		pointer to struct dvb_device to be used
 * @param server	server hostname or address
 * @param port		server port
 *
 * @note The protocol between the dvbv5-daemon and the dvb_dev library is
 * highly experimental and is subject to changes in a near future. So,
 * while this is not stable enough, you will only work if both the client
 * and the server are running the same version of the v4l-utils library.
 */
int dvb_dev_remote_init(struct dvb_device *d, char *server, int port);

#else

static inline int dvb_dev_remote_init(struct dvb_device *d, char *server,
				      int port)
{
	return -1;
};

#endif


#endif
