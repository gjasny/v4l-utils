/*
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 * Or, point your browser to http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
 *
 * These routines were written as part of the dvb-apps, as:
 *	util functions for various ?zap implementations
 *
 *	Copyright (C) 2001 Johannes Stezenbach (js@convergence.de)
 *	for convergence integrated media
 *
 *	Originally licensed as GPLv2 or upper
 *
 * All subsequent changes are under GPLv2 only and are:
 *	Copyright (c) 2011-2012 - Mauro Carvalho Chehab <mchehab@redhat.com>
 *
 */
#ifndef _DVB_DEMUX_H
#define _DVB_DEMUX_H

#include <linux/dvb/dmx.h>

#ifdef __cplusplus
extern "C" {
#endif

int dvb_dmx_open(int adapter, int demux);
void dvb_dmx_close(int dmx_fd);
void dvb_dmx_stop(int dmx_fd);

int dvb_set_pesfilter(int dmxfd, int pid, dmx_pes_type_t type, dmx_output_t output, int buffersize);

int get_pmt_pid(const char *dmxdev, int sid);

#ifdef __cplusplus
}
#endif

#endif
