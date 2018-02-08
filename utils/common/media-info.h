/* SPDX-License-Identifier: GPL-2.0-only */
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
	MEDIA_TYPE_MEDIA,
};

/*
 * Detect what type the device is.
 */
media_type media_detect_type(const char *device);

/*
 * Return the device name given the major and minor numbers.
 */
std::string media_get_device(__u32 major, __u32 minor);

/*
 * For a given device fd return the corresponding media device
 * or -1 if there is none.
 */
int mi_get_media_fd(int fd);

/* Return entity flags description */
std::string entflags2s(__u32 flags);

/* Return interface flags description */
std::string ifacetype2s(__u32 type);

/* Return function description */
std::string entfunction2s(__u32 function, bool *is_invalid = NULL);

/* Return pad flags description */
std::string padflags2s(__u32 flags);

/* Return link flags description */
std::string linkflags2s(__u32 flags);

/*
 * Show media controller information media_fd and (if >= 0) the
 * corresponsing entity/interface information for the fd.
 *
 * If is_invalid != NULL, then set it to true if errors are detected
 * in the media information.
 *
 * Return 0 if the driver doesn't support MEDIA_IOC_G_TOPOLOGY.
 * Return MEDIA_ENT_F_UNKNOWN if it does support this but there were
 * errors reading the topology. Otherwise return the entity ID of fd.
 */
__u32 mi_media_info_for_fd(int media_fd, int fd, bool *is_invalid = NULL);

#endif
