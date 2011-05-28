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
   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307 USA.
 */

/*
 * Version of the API
 */
#define GET_MEDIA_DEVICES_VERSION	0x0102

/*
 A typical usecase for the above API is:

void start_alsa(char *video_dev) {
	void *md;
	char *alsa_playback, *alsa_capture, *p;

	md = discover_media_devices();
	if (!md)
		return;
	alsa_capture = get_first_alsa_cap_device(md, video_dev);
	alsa_playback = get_first_no_video_out_device(md);
	if (alsa_capture && alsa_playback)
		alsa_handler(alsa_playback, alsa_capture);
	free_media_devices(md);
}

start_alsa("/dev/video0");

Where alsa_handler() is some function that will need to handle
 both alsa capture and playback devices.
*/


/**
 * enum device_type - Enumerates the type for each device
 *
 * The device_type is used to sort the media devices array.
 * So, the order is relevant. The first device should be
 * V4L_VIDEO.
 */
enum device_type {
	UNKNOWN = 65535,
	NONE    = 65534,
	V4L_VIDEO = 0,
	V4L_VBI,
	DVB_FRONTEND,
	DVB_DEMUX,
	DVB_DVR,
	DVB_NET,
	DVB_CA,
	/* TODO: Add dvb full-featured nodes */
	SND_CARD,
	SND_CAP,
	SND_OUT,
	SND_CONTROL,
	SND_HW,
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
 * display_media_devices() - prints a list of media devices
 *
 * @opaque:	media devices opaque descriptor
 */
void display_media_devices(void *opaque);

/**
 * get_not_associated_device() - Return the next device not associated with
 * 				 an specific device type.
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
char *get_associated_device(void *opaque,
			    char *last_seek,
			    enum device_type desired_type,
			    char *seek_device,
			    enum device_type seek_type);

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
char *get_not_associated_device(void *opaque,
			    char *last_seek,
			    enum device_type desired_type,
			    enum device_type not_desired_type);
