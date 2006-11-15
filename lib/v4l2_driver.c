/*
   Copyright (C) 2006 Mauro Carvalho Chehab <mchehab@infradead.org>

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

  */

#include <errno.h>
#include <fcntl.h>
#include <malloc.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "v4l2_driver.h"

/****************************************************************************
	Auxiliary routines
 ****************************************************************************/
static void free_list(struct drv_list **list_ptr)
{
	struct drv_list *prev,*cur;

	if (list_ptr==NULL)
		return;

	prev=*list_ptr;
	if (prev==NULL)
		return;

	do {
		cur=prev->next;
		if (prev->curr)
			free (prev->curr);	// Free data
		free (prev);			// Free list
		prev=cur;
	} while (prev);

	*list_ptr=NULL;
}

/****************************************************************************
	Open/Close V4L2 devices
 ****************************************************************************/
int v4l2_open (char *device, int debug, struct v4l2_driver *drv)
{
	int ret;

	memset(drv,0,sizeof(*drv));

	drv->debug=debug;

	if ((drv->fd = open(device, O_RDONLY)) < 0) {
		perror("Couldn't open video0");
		return(errno);
	}

	ret=ioctl(drv->fd,VIDIOC_QUERYCAP,(void *) &drv->cap);
	if (ret>=0 && drv->debug) {
		printf ("driver=%s, card=%s, bus=%s, version=0x%08x, "
			"capabilities=0x%08x\n",
			drv->cap.driver,drv->cap.card,drv->cap.bus_info,
			drv->cap.version,drv->cap.capabilities);
	}
	return ret;
}

int v4l2_close (struct v4l2_driver *drv)
{
	free_list(&drv->stds);
	free_list(&drv->inputs);
	free_list(&drv->fmt_caps);

	return (close(drv->fd));
}

/****************************************************************************
	V4L2 Eumberations
 ****************************************************************************/
int v4l2_enum_stds (struct v4l2_driver *drv)
{
	struct v4l2_standard	*p=NULL;
	struct drv_list		*list;
	int			ok=0,ret,i;
	v4l2_std_id		id;

	free_list(&drv->stds);

	list=drv->stds=calloc(1,sizeof(drv->stds));

	for (i=0; ok==0; i++) {
		p=calloc(1,sizeof(*p));
		p->index=i;
		ok=ioctl(drv->fd,VIDIOC_ENUMSTD,p);
		if (ok<0) {
			ok=errno;
			free(p);
			break;
		}
		if (drv->debug) {
			printf ("STANDARD: index=%d, id=0x%08x, name=%s, fps=%.3f, "
				"framelines=%d\n", p->index,
				(unsigned int)p->id, p->name,
				1.*p->frameperiod.denominator/p->frameperiod.numerator,
				p->framelines);
		}
		if (list->curr) {
			list->next=calloc(1,sizeof(*list->next));
			list=list->next;
		}
		list->curr=p;
	}
	if (i>0 && ok==-EINVAL)
		return 0;

	return ok;
}

int v4l2_enum_input (struct v4l2_driver *drv)
{
	struct v4l2_input	*p=NULL;
	struct drv_list		*list;
	int			ok=0,ret,i;
	v4l2_std_id		id;

	free_list(&drv->inputs);

	list=drv->inputs=calloc(1,sizeof(drv->inputs));

	for (i=0; ok==0; i++) {
		p=calloc(1,sizeof(*p));
		p->index=i;
		ok=ioctl(drv->fd,VIDIOC_ENUMINPUT,p);
		if (ok<0) {
			ok=errno;
			free(p);
			break;
		}
		if (drv->debug) {
			printf ("INPUT: index=%d, name=%s, type=%d, audioset=%d, "
				"tuner=%d, std=%08x, status=%d\n",
				p->index,p->name,p->type,p->audioset, p->tuner,
				(unsigned int)p->std, p->status);
		}
		if (list->curr) {
			list->next=calloc(1,sizeof(*list->next));
			list=list->next;
		}
		list->curr=p;
	}
	if (i>0 && ok==-EINVAL)
		return 0;
	return ok;
}

int v4l2_enum_fmt_cap (struct v4l2_driver *drv)
{
	struct v4l2_fmtdesc 	*p=NULL;
	struct v4l2_format	fmt;
	struct drv_list		*list;
	int			ok=0,ret,i;
	v4l2_std_id		id;

	free_list(&drv->fmt_caps);

	list=drv->fmt_caps=calloc(1,sizeof(drv->fmt_caps));

	for (i=0; ok==0; i++) {
		p=calloc(1,sizeof(*p));
		p->index=i;
		p->type =V4L2_BUF_TYPE_VIDEO_CAPTURE;

		ok=ioctl(drv->fd,VIDIOC_ENUM_FMT,p);
		if (ok<0) {
			ok=errno;
			free(p);
			break;
		}
		if (drv->debug) {
			printf ("FORMAT: index=%d, type=%d, flags=%d, description=%s\n\t"
				"pixelformat=0x%08x\n",
				p->index, p->type, p->flags,p->description,
				p->pixelformat);
		}
		if (list->curr) {
			list->next=calloc(1,sizeof(*list->next));
			list=list->next;
		}
		list->curr=p;
	}
	if (i>0 && ok==-EINVAL)
		return 0;
	return ok;
}

/****************************************************************************
	Set routines - currently, it also checks results with Get
 ****************************************************************************/
int v4l2_setget_std (struct v4l2_driver *drv, enum v4l2_direction dir, v4l2_std_id *id)
{
	v4l2_std_id		s_id=*id;
	int			ret=0;
	char			s[256];

	if (dir & V4L2_SET) {
		ret=ioctl(drv->fd,VIDIOC_S_STD,&s_id);
		if (ret<0) {
			ret=errno;

			sprintf (s,"while trying to set STD to %08x",
								(unsigned int) id);
			perror(s);
		}
	}

	if (dir & V4L2_GET) {
		ret=ioctl(drv->fd,VIDIOC_G_STD,&s_id);
		if (ret<0) {
			ret=errno;
			perror ("while trying to get STD id");
		}
	}

	if (dir == V4L2_SET_GET) {
		if (*id & s_id) {
			if (*id != s_id) {
				printf ("Warning: Received a std subset (%08x"
					" std) while trying to adjust to %08x\n",
					(unsigned int) s_id,(unsigned int) *id);
			}
		} else {
			fprintf (stderr,"Error: Received %08x std while trying"
				" to adjust to %08x\n",
				(unsigned int) s_id,(unsigned int) *id);
		}
	}
	return ret;
}

int v4l2_setget_input (struct v4l2_driver *drv, enum v4l2_direction dir, struct v4l2_input *input)
{
	int			ok=0,ret,i;
	unsigned int		inp=input->index;
	char			s[256];

	if (dir & V4L2_SET) {
		ret=ioctl(drv->fd,VIDIOC_S_INPUT,input);
		if (ret<0) {
			ret=errno;
			sprintf (s,"while trying to set INPUT to %d\n", inp);
			perror(s);
		}
	}

	if (dir & V4L2_GET) {
		ret=ioctl(drv->fd,VIDIOC_G_INPUT,input);
		if (ret<0) {
			perror ("while trying to get INPUT id\n");
		}
	}

	if (dir & V4L2_SET_GET) {
		if (input->index != inp) {
			printf ("Input is different than expected (received %i, set %i)\n",
						inp, input->index);
		}
	}

	return ok;
}

/****************************************************************************
	Get routines
 ****************************************************************************/
int v4l2_get_parm (struct v4l2_driver *drv)
{
	int ret;
	struct v4l2_captureparm *c;

	drv->parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if ((ret=ioctl(drv->fd,VIDIOC_G_PARM,&drv->parm))>=0) {
		c=&drv->parm.parm.capture;
		printf ("PARM: capability=%d, capturemode=%d, frame time =%.3f ns "
			"ext=%x, readbuf=%d\n",
			c->capability,
			c->capturemode,
			100.*c->timeperframe.numerator/c->timeperframe.denominator,
			c->extendedmode, c->readbuffers);
	} else {
		ret=errno;

		perror ("VIDIOC_G_PARM");
	}

	return ret;
}
