/*
 * Copyright (c) 2011-2014 - Mauro Carvalho Chehab
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

#ifndef __DVB_DEV_PRIV_H
#define __DVB_DEV_PRIV_H

#include <libdvbv5/dvb-dev.h>

struct dvb_device_priv;

struct dvb_open_descriptor {
	int fd;
	struct dvb_dev_list *dev;
	struct dvb_device_priv *dvb;
	struct dvb_open_descriptor *next;
};

struct dvb_dev_ops {
	int (*find)(struct dvb_device_priv *dvb, dvb_dev_change_t handler,
		    void *user_priv);
	struct dvb_dev_list * (*seek_by_adapter)(struct dvb_device_priv *dvb,
						 unsigned int adapter,
						 unsigned int num,
						 enum dvb_dev_type type);
	struct dvb_dev_list * (*get_dev_info)(struct dvb_device_priv *dvb,
					      const char *sysname);
	int (*stop_monitor)(struct dvb_device_priv *dvb);
	struct dvb_open_descriptor *(*open)(struct dvb_device_priv *dvb,
					    const char *sysname, int flags);
	int (*close)(struct dvb_open_descriptor *open_dev);
	int (*dmx_stop)(struct dvb_open_descriptor *open_dev);
	int (*set_bufsize)(struct dvb_open_descriptor *open_dev,
			   int buffersize);
	ssize_t (*read)(struct dvb_open_descriptor *open_dev,
			void *buf, size_t count);
	int (*dmx_set_pesfilter)(struct dvb_open_descriptor *open_dev,
				 int pid, dmx_pes_type_t type,
				 dmx_output_t output, int bufsize);
	int (*dmx_set_section_filter)(struct dvb_open_descriptor *open_dev,
				     int pid, unsigned filtsize,
				     unsigned char *filter,
				     unsigned char *mask,
				     unsigned char *mode,
				     unsigned int flags);
	int (*dmx_get_pmt_pid)(struct dvb_open_descriptor *open_dev, int sid);
	struct dvb_v5_descriptors *(*scan)(struct dvb_open_descriptor *open_dev,
					   struct dvb_entry *entry,
					   check_frontend_t *check_frontend,
					   void *args,
					   unsigned other_nit,
					   unsigned timeout_multiply);

	int (*fe_set_sys)(struct dvb_v5_fe_parms *p, fe_delivery_system_t sys);
	int (*fe_get_parms)(struct dvb_v5_fe_parms *p);
	int (*fe_set_parms)(struct dvb_v5_fe_parms *p);
	int (*fe_get_stats)(struct dvb_v5_fe_parms *p);

	void (*free)(struct dvb_device_priv *dvb);
	int (*get_fd)(struct dvb_open_descriptor *dvb);
};

struct dvb_device_priv {
	struct dvb_device d;
	struct dvb_dev_ops ops;

	struct dvb_open_descriptor open_list;

	/* private data to be used by implementation, if needed */
	void *priv;
};


/* From dvb-dev.c */
extern const char * const dev_type_names[];
extern const unsigned int dev_type_names_size;
void dvb_dev_dump_device(char *msg,
			struct dvb_v5_fe_parms_priv *parms,
			struct dvb_dev_list *dev);
void free_dvb_dev(struct dvb_dev_list *dvb_dev);
void dvb_dev_free_devices(struct dvb_device_priv *dvb);

/* From dvb-dev-local.c */
void dvb_dev_local_init(struct dvb_device_priv *dvb);

#endif
