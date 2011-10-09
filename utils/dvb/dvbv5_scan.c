/*
 * Copyright (c) 2011 - Mauro Carvalho Chehab <mchehab@redhat.com>
 */

#include "dvb-fe.h"

int main(void)
{
	struct dvb_v5_fe_parms *parms;

	parms = dvb_fe_open(0, 0, 1);
	dvb_fe_close(parms);

	return 0;
}