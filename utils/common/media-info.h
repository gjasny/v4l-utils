/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Copyright 2018 Cisco Systems, Inc. and/or its affiliates. All rights reserved.
 */

#ifndef _MEDIA_INFO_H
#define _MEDIA_INFO_H

enum media_type {
	MEDIA_TYPE_CANT_STAT,
	MEDIA_TYPE_UNKNOWN,
	MEDIA_TYPE_VIDEO,
	MEDIA_TYPE_VBI,
	MEDIA_TYPE_RADIO,
	MEDIA_TYPE_SDR,
	MEDIA_TYPE_TOUCH,
	MEDIA_TYPE_SUBDEV,
	MEDIA_TYPE_DVB_FRONTEND,
	MEDIA_TYPE_DVB_DEMUX,
	MEDIA_TYPE_DVB_DVR,
	MEDIA_TYPE_DVB_NET,
	MEDIA_TYPE_DTV_CA,
	MEDIA_TYPE_MEDIA,
};

/*
 * Detect what type the device is.
 */
media_type mi_media_detect_type(const char *device);

/*
 * Return the device name given the major and minor numbers.
 */
std::string mi_media_get_device(__u32 major, __u32 minor);

/*
 * For a given device fd retrieve the dev_t (major/minor) value.
 * Returns 0 if successful (and fills in *dev) and -1 on failure.
 */
int mi_get_dev_t_from_fd(int fd, dev_t *dev);

/*
 * For a given dev_t value (major/minor), find the corresponding
 * device name.
 * Returns the /dev/... path if successful, or an empty string on
 * failure.
 */
std::string mi_get_devpath_from_dev_t(dev_t dev);

/*
 * For a given device fd return the corresponding media device
 * or -1 if there is none.
 *
 * If bus_info is not NULL, then find the media device that
 * matches the given bus_info.
 */
int mi_get_media_fd(int fd, const char *bus_info = NULL);

/* Return entity flags description */
std::string mi_entflags2s(__u32 flags);

/* Return interface flags description */
std::string mi_ifacetype2s(__u32 type);

/* Return true if this function requires an interface */
bool mi_func_requires_intf(__u32 function);

/* Return function description */
std::string mi_entfunction2s(__u32 function, bool *is_invalid = NULL);

/* Return pad flags description */
std::string mi_padflags2s(__u32 flags);

/* Return link flags description */
std::string mi_linkflags2s(__u32 flags);

/*
 * Show media controller information media_fd and (if >= 0) the
 * corresponding entity/interface information for the fd.
 *
 * If is_invalid != NULL, then set it to true if errors are detected
 * in the media information.
 *
 * If function != NULL, then set it to the function of the entity to
 * which the interface is connected.
 *
 * Return 0 if the driver doesn't support MEDIA_IOC_G_TOPOLOGY.
 * Return MEDIA_ENT_F_UNKNOWN if it does support this but there were
 * errors reading the topology. Otherwise return the entity ID of fd.
 */
__u32 mi_media_info_for_fd(int media_fd, int fd, bool *is_invalid = NULL, __u32 *function = NULL);

#endif
