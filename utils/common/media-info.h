/*
 * Copyright 2018 Cisco Systems, Inc. and/or its affiliates. All rights reserved.
 *
 * This program is free software; you may redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
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

/*
 * Show media controller information media_fd and (if >= 0) the
 * corresponsing entity/interface information for the fd.
 * Return the entity ID of fd.
 */
__u32 mi_media_info_for_fd(int media_fd, int fd);

#endif
