/*
 * Copyright (c) 2011-2014 - Mauro Carvalho Chehab
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation version 2
 * of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * These routines were originally written as part of the dvb-apps, as:
 *	util functions for various ?zap implementations
 *
 *	Copyright (C) 2001 Johannes Stezenbach (js@convergence.de)
 *	for convergence integrated media
 *
 *	Originally licensed as GPLv2 or upper
 */
#ifndef _DVB_DEMUX_H
#define _DVB_DEMUX_H

#include <linux/dvb/dmx.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * dvb_dmx_open() - Opens a DVB demux in read/write mode
 *
 * @adapter:	DVB adapter number to open
 * @demux:	DVB demux number to open
 *
 * This is a wrapper function to open. File is always opened in blocking mode.
 */
int dvb_dmx_open(int adapter, int demux);

/**
 * dvb_dmx_close() - Stops the DMX filter for the file descriptor and closes
 *
 * @dmx_fd:	File descriptor to close
 *
 * This is a wrapper function to open.
 */
void dvb_dmx_close(int dmx_fd);

/**
 * dvb_dmx_stop() - Stops the DMX filter for a given file descriptor
 *
 * @dmx_fd:	File descriptor to close
 *
 * This is a wrapper function to open.
 */
void dvb_dmx_stop(int dmx_fd);

/**
 * dvb_set_pesfilter - Start a filter for a MPEG-TS Packetized Elementary
 * 		       Stream (PES)
 *
 * @dmx_fd:	File descriptor for the demux device
 * @pid:	Program ID to filter. Use 0x2000 to select all PIDs
 * @type:	type of the PID (DMX_PES_VIDEO, DMX_PES_AUDIO, DMX_PES_OTHER,
 *		etc).
 * @output:	Where the data will be output (DMX_OUT_TS_TAP, DMX_OUT_DECODER,
 *		etc).
 * @buffersize:	Size of the buffer to be allocated to store the filtered data.
 *
 * This is a wrapper function for DMX_SET_PES_FILTER ioctl.
 * See http://linuxtv.org/downloads/v4l-dvb-apis/dvb_demux.html
 * for more details.
 */
int dvb_set_pesfilter(int dmxfd, int pid, dmx_pes_type_t type,
		      dmx_output_t output, int buffersize);

/**
 * dvb_set_section_filter - Sets a MPEG-TS section filter
 *
 * @dmx_fd:	File descriptor for the demux device
 * @filtsize:	Size of the filter (up to 18 btyes)
 * @filter:	data to filter. Can be NULL or should have filtsize length
 * @mask:	filter mask. Can be NULL or should have filtsize length
 * @mode:	mode mask. Can be NULL or should have filtsize length
 * @flags:	flags for set filter (DMX_CHECK_CRC,DMX_ONESHOT,
 *		DMX_IMMEDIATE_START).
 *
 * This is a wrapper function for DMX_SET_FILTER ioctl.
 * See http://linuxtv.org/downloads/v4l-dvb-apis/dvb_demux.html
 * for more details.
 */
int dvb_set_section_filter(int dmxfd, int pid, unsigned filtsize,
			   unsigned char *filter,
			   unsigned char *mask,
			   unsigned char *mode,
			   unsigned int flags);

/**
 * dvb_get_pmt_pid - read the contents of the MPEG-TS PAT table, seeking for
 *		     an specific service ID
 *
 * @sid:	Session ID to seeking
 *
 * This function currently assumes that the hope PAT fits into one session.
 * At return, it returns a negative value if error or the PID associated with
 * the desired Session ID.
 */
int dvb_get_pmt_pid(const char *dmxdev, int sid);

#ifdef __cplusplus
}
#endif

#endif
