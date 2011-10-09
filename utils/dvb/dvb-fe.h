/*
 * Copyright (c) 2011 - Mauro Carvalho Chehab <mchehab@redhat.com>
 */

#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <malloc.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <string.h>
#include "dvb_frontend.h"

#define ARRAY_SIZE(x)	(sizeof(x)/sizeof((x)[0]))

#define MAX_DELIVERY_SYSTEMS	20

struct dvb_v5_fe_parms {
	int				fd;
	char				*fname;
	unsigned			verbose;
	struct dvb_frontend_info	info;
	uint32_t			version;
	fe_delivery_system_t		current_sys;
	int				num_systems;
	fe_delivery_system_t		systems[MAX_DELIVERY_SYSTEMS];
	int				n_props;
	struct dtv_property		dvb_prop[DTV_MAX_COMMAND];
	int				legacy_fe;
};

void dvb_fe_prt(struct dvb_v5_fe_parms *parms);
int dvb_fe_retrieve_parm(struct dvb_v5_fe_parms *parms,
			unsigned cmd, uint32_t *value);
int dvb_fe_store_parm(struct dvb_v5_fe_parms *parms,
		      unsigned cmd, uint32_t value);
int dvb_change_sys(struct dvb_v5_fe_parms *parms,
		   fe_delivery_system_t sys);
int dvb_fe_get_parms(struct dvb_v5_fe_parms *parms);
int dvb_fe_set_parms(struct dvb_v5_fe_parms *parms);
struct dvb_v5_fe_parms *dvb_fe_open(int adapter, int frontend,
				    unsigned verbose);
void dvb_fe_close(struct dvb_v5_fe_parms *parms);
