/*
 * Copyright (c) 2013 - Andre Roth <neolynx@gmail.com>
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
 *
 */

#include <libdvbv5/descriptors.h>
#include <libdvbv5/dvb-fe.h>
#include <libdvbv5/dvb-dev.h>

struct dvb_device_priv;

int dvb_desc_service_location_init(struct dvb_v5_fe_parms *parms, const uint8_t *buf, struct dvb_desc *desc)
{
	return -1;
}

void dvb_desc_service_location_print(struct dvb_v5_fe_parms *parms, const struct dvb_desc *desc)
{
}

void dvb_desc_service_location_free(struct dvb_desc *desc)
{
}

int dvb_desc_service_list_init(struct dvb_v5_fe_parms *parms, const uint8_t *buf, struct dvb_desc *desc)
{
	return -1;
}

void dvb_desc_service_list_print(struct dvb_v5_fe_parms *parms, const struct dvb_desc *desc)
{
}

struct dvb_dev_list *dvb_dev_seek_by_sysname(struct dvb_device *d,
                                             unsigned int adapter,
                                             unsigned int num,
                                             enum dvb_dev_type type)
{
	return dvb_dev_seek_by_adapter(d, adapter, num, type);
}

struct dvb_dev_list *dvb_local_seek_by_adapter(struct dvb_device_priv *dvb,
                                               unsigned int adapter,
                                               unsigned int num,
                                               enum dvb_dev_type type);

struct dvb_dev_list *dvb_local_seek_by_sysname(struct dvb_device_priv *dvb,
                                               unsigned int adapter,
                                               unsigned int num,
                                               enum dvb_dev_type type)
{
	return dvb_local_seek_by_adapter(dvb, adapter, num, type);
}

struct dvb_dev_list *dvb_remote_seek_by_adapter(struct dvb_device_priv *dvb,
                                                unsigned int adapter,
                                                unsigned int num,
						enum dvb_dev_type type);

struct dvb_dev_list *dvb_remote_seek_by_sysname(struct dvb_device_priv *dvb,
                                                unsigned int adapter,
                                                unsigned int num,
                                                enum dvb_dev_type type)
{
	return dvb_remote_seek_by_adapter(dvb, adapter, num, type);
}
