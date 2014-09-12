/*
 * Copyright (c) 2011-2012 - Mauro Carvalho Chehab
 * Copyright (c) 2012-2014 - Andre Roth <neolynx@gmail.com>
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 * Or, point your browser to http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
 *
 */

/**
 * @file crc32.h
 * @ingroup ancillary
 * @brief Provides ancillary code to calculate DVB crc32 checksum
 * @copyright GNU General Public License version 2 (GPLv2)
 * @author Mauro Carvalho Chehab
 * @author Andre Roth
 *
 * @par Bug Report
 * Please submit bug reports and patches to linux-media@vger.kernel.org
 */

#ifndef _CRC32_H
#define _CRC32_H

#include <stdint.h>
#include <unistd.h> /* size_t */

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Calculates the crc-32 as defined at the MPEG-TS specs
 * @ingroup ancillary
 *
 * @param data		Pointer to the buffer to be checked
 * @param datalen	Length of the buffer
 * @param crc		Initial value for the crc checksum. To calculate the
 *			checksum of the entire packet at once, use 0xFFFFFFFF
 */
uint32_t dvb_crc32(uint8_t *data, size_t datalen, uint32_t crc);

#ifdef __cplusplus
}
#endif

#endif

