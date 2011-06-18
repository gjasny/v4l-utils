/*
   Copyright © 2011 by Mauro Carvalho Chehab <mchehab@redhat.com>

   The get_media_devices is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The libv4l2util Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the libv4l2util Library; if not, write to the Free
   Software Foundation, Inc., 51 Franklin Street, Suite 500, Boston, MA
   02110-1335 USA.
 */

/*
 * Version of the API
 */
#define GET_MEDIA_DEVICES_VERSION	0x0105

/**
 * enum device_type - Enumerates the type for each device
 *
 * The device_type is used to sort the media devices array.
 * So, the order is relevant. The first device should be
 * MEDIA_V4L_VIDEO.
 */
enum device_type {
	UNKNOWN = 65535,
	NONE    = 65534,
	MEDIA_V4L_VIDEO = 0,
	MEDIA_V4L_VBI,
	MEDIA_V4L_RADIO,
	MEDIA_V4L_SUBDEV,

	MEDIA_DVB_VIDEO = 100,
	MEDIA_DVB_AUDIO,
	MEDIA_DVB_SEC,
	MEDIA_DVB_FRONTEND,
	MEDIA_DVB_DEMUX,
	MEDIA_DVB_DVR,
	MEDIA_DVB_CA,
	MEDIA_DVB_NET,
	MEDIA_DVB_OSD,

	MEDIA_SND_CARD = 200,
	MEDIA_SND_CAP,
	MEDIA_SND_OUT,
	MEDIA_SND_CONTROL,
	MEDIA_SND_HW,
	MEDIA_SND_TIMER,
	MEDIA_SND_SEQ,
	/*
	 * FIXME: not all alsa devices were mapped. missing things like
	 *	midi, midiC%iD%i and timer interfaces
	 */
};

enum bus_type {
	MEDIA_BUS_UNKNOWN,
	MEDIA_BUS_VIRTUAL,
	MEDIA_BUS_PCI,
	MEDIA_BUS_USB,
};

/**
 * discover_media_devices() - Returns a list of the media devices
 * @md_size:	Returns the size of the media devices found
 *
 * This function reads the /sys/class nodes for V4L, DVB and sound,
 * and returns an opaque desciptor that keeps a list of the devices.
 * The fields on this list is opaque, as they can be changed on newer
 * releases of this library. So, all access to it should be done via
 * a function provided by the API. The devices are ordered by device,
 * type and node. At return, md_size is updated.
 */
void *discover_media_devices(void);

/**
 * free_media_devices() - Frees the media devices array
 *
 * @opaque:	media devices opaque descriptor
 *
 * As discover_media_devices() dynamically allocate space for the
 * strings, feeing the list requires also to free those data. So,
 * the safest and recommended way is to call this function.
 */
void free_media_devices(void *opaque);

/**
 * media_device_type() - returns a string with the name of a given type
 *
 * @type:	media device type
 */
const char *media_device_type(const enum device_type type);

/**
 * display_media_devices() - prints a list of media devices
 *
 * @opaque:	media devices opaque descriptor
 */
void display_media_devices(void *opaque);

/**
 * get_associated_device() - Return the next device associated with another one
 *
 * @opaque:		media devices opaque descriptor
 * @last_seek:		last seek result. Use NULL to get the first result
 * @desired_type:	type of the desired device
 * @seek_device:	name of the device with you want to get an association.
 *@ seek_type:		type of the seek device. Using NONE produces the same
 *			result of using NULL for the seek_device.
 *
 * This function seeks inside the media_devices struct for the next device
 * that it is associated with a seek parameter.
 * It can be used to get an alsa device associated with a video device. If
 * the seek_device is NULL or seek_type is NONE, it will just search for
 * devices of the desired_type.
 */
const char *get_associated_device(void *opaque,
				  const char *last_seek,
				  const enum device_type desired_type,
				  const char *seek_device,
				  const enum device_type seek_type);

/**
 * fget_associated_device() - Return the next device associated with another one
 *
 * @opaque:		media devices opaque descriptor
 * @last_seek:		last seek result. Use NULL to get the first result
 * @desired_type:	type of the desired device
 * @fd_seek_device:	file handler for the device where the association will
			be made
 *@ seek_type:		type of the seek device. Using NONE produces the same
 *			result of using NULL for the seek_device.
 *
 * This function seeks inside the media_devices struct for the next device
 * that it is associated with a seek parameter.
 * It can be used to get an alsa device associated with an open file descriptor
 */
const char *fget_associated_device(void *opaque,
				   const char *last_seek,
				   const enum device_type desired_type,
				   const int fd_seek_device,
				   const enum device_type seek_type);

/**
 * get_not_associated_device() - Return the next device not associated with
 *			     an specific device type.
 *
 * @opaque:		media devices opaque descriptor
 * @last_seek:		last seek result. Use NULL to get the first result
 * @desired_type:	type of the desired device
 * @not_desired_type:	type of the seek device
 *
 * This function seeks inside the media_devices struct for the next physical
 * device that doesn't support a non_desired type.
 * This method is useful for example to return the audio devices that are
 * provided by the motherboard.
 */
const char *get_not_associated_device(void *opaque,
				      const char *last_seek,
				      const enum device_type desired_type,
				      const enum device_type not_desired_type);
