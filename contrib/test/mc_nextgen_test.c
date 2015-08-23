/*
 * Media controller Next Generation test app
 *
 * Copyright (C) 2015 Mauro Carvalho Chehab <mchehab@osg.samsung.com>
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
 * */


#if 0
/*
 * this assumes that a copy of the new media.h to be at the same
 * directory as this test utility
 */
#define _GNU_SOURCE
#define __user
#include "media.h"
#else
#include <linux/media.h>
#endif


#include <syslog.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>

/*
 * Those came from media-entity.h and bitops.h
 * They should actually be added at the media.h UAPI if we decide to keep
 * both ID and TYPE fields together - as current proposal
 */

enum media_gobj_type {
	MEDIA_GRAPH_ENTITY,
	MEDIA_GRAPH_PAD,
	MEDIA_GRAPH_LINK,
	MEDIA_GRAPH_INTF_DEVNODE,
};

static uint32_t media_type(uint32_t id)
{
	return id >> 24;
}

static inline uint32_t media_localid(uint32_t id)
{
	return id & 0xffffff;
}

static inline const char *gobj_type(uint32_t id)
{
	switch (media_type(id)) {
	case MEDIA_GRAPH_ENTITY:
		return "entity";
	case MEDIA_GRAPH_PAD:
		return "pad";
	case MEDIA_GRAPH_LINK:
		return "link";
	case MEDIA_GRAPH_INTF_DEVNODE:
		return "intf_devnode";
	default:
		return "unknown";
	}
}
static inline const char *intf_type(uint32_t intf_type)
{
	switch (intf_type) {
	case MEDIA_INTF_T_DVB_FE:
		return "frontend";
	case MEDIA_INTF_T_DVB_DEMUX:
		return "demux";
	case MEDIA_INTF_T_DVB_DVR:
		return "DVR";
	case MEDIA_INTF_T_DVB_CA:
		return  "CA";
	case MEDIA_INTF_T_DVB_NET:
		return "dvbnet";
	case MEDIA_INTF_T_V4L_VIDEO:
		return "video";
	case MEDIA_INTF_T_V4L_VBI:
		return "vbi";
	case MEDIA_INTF_T_V4L_RADIO:
		return "radio";
	case MEDIA_INTF_T_V4L_SUBDEV:
		return "v4l2_subdev";
	case MEDIA_INTF_T_V4L_SWRADIO:
		return "swradio";
	case MEDIA_INTF_T_ALSA_PCM_CAPTURE:
		return "pcm_capture";
	case MEDIA_INTF_T_ALSA_PCM_PLAYBACK:
		return "pcm_playback";
	case MEDIA_INTF_T_ALSA_CONTROL:
		return "alsa_control";
	case MEDIA_INTF_T_ALSA_COMPRESS:
		return "compress";
	case MEDIA_INTF_T_ALSA_RAWMIDI:
		return "rawmidi";
	case MEDIA_INTF_T_ALSA_HWDEP:
		return "hwdep";
	default:
		return "unknown_intf";
	}
};

/* Ancilary function to produce an human readable ID for an object */

static char *objname(uint32_t id)
{
	char *name;
	asprintf(&name, "%s#%d", gobj_type(id), media_localid(id));

	return name;
}

/* Shamelessly copied from libdvbv5 dvb-log.c and dvb-log.h */

static const struct loglevel {
	const char *name;
	const char *color;
	int fd;
} loglevels[9] = {
	{"EMERG    ", "\033[31m", STDERR_FILENO },
	{"ALERT    ", "\033[31m", STDERR_FILENO },
	{"CRITICAL ", "\033[31m", STDERR_FILENO },
	{"ERROR    ", "\033[31m", STDERR_FILENO },
	{"WARNING  ", "\033[33m", STDOUT_FILENO },
	{NULL,            "\033[36m", STDOUT_FILENO }, /* NOTICE */
	{NULL,            NULL,       STDOUT_FILENO }, /* INFO */
	{"DEBUG    ", "\033[32m", STDOUT_FILENO },
	{NULL,            "\033[0m",  STDOUT_FILENO }, /* reset*/
};
#define LOG_COLOROFF 8

void __log(int level, const char *fmt, ...)
{
	if(level > sizeof(loglevels) / sizeof(struct loglevel) - 2) // ignore LOG_COLOROFF as well
		level = LOG_INFO;
	va_list ap;
	va_start(ap, fmt);
	FILE *out = stdout;
	if (STDERR_FILENO == loglevels[level].fd)
		out = stderr;
	if (loglevels[level].color && isatty(loglevels[level].fd))
		fputs(loglevels[level].color, out);
	if (loglevels[level].name)
		fprintf(out, "%s", loglevels[level].name);
	vfprintf(out, fmt, ap);
	fprintf(out, "\n");
	if(loglevels[level].color && isatty(loglevels[level].fd))
		fputs(loglevels[LOG_COLOROFF].color, out);
	va_end(ap);
}

#define err(fmt, arg...) do {\
	__log(LOG_ERR, fmt, ##arg); \
} while (0)
#define dbg(fmt, arg...) do {\
	__log(LOG_DEBUG, fmt, ##arg); \
} while (0)
#define warn(fmt, arg...) do {\
	__log(LOG_WARNING, fmt, ##arg); \
} while (0)
#define info(fmt, arg...) do {\
	__log(LOG_NOTICE, fmt, ##arg); \
} while (0)

#define logperror(msg) do {\
	__log(LOG_ERR, "%s: %s", msg, strerror(errno)); \
} while (0)

/*
 * The real code starts here
 */

struct graph_obj {
	uint32_t id;
	union {
		struct media_v2_entity *entity;
		struct media_v2_pad *pad;
		struct media_v2_interface *intf;
		struct media_v2_link *link;
	};
};

struct media_controller {
	int fd;
	struct media_v2_topology topo;

	int num_gobj;
	struct graph_obj *gobj;
};

static int media_init_graph_obj(struct media_controller *mc)
{
	struct media_v2_topology *topo = &mc->topo;
	int i, num_gobj;

	num_gobj = topo->num_entities + topo->num_interfaces
		   + topo->num_pads + topo->num_links;

	mc->gobj = calloc(num_gobj, sizeof(*mc->gobj));
	if (!mc->gobj) {
		logperror("couldn't allocate space for graph_obj\n");
		return -ENOMEM;
	}
	mc->num_gobj = num_gobj;

	for (i = 0; i < topo->num_entities; i++) {
		mc->gobj[i].id = topo->entities[i].id;
		mc->gobj[i].entity = &topo->entities[i];
	}
	for (i = 0; i < topo->num_interfaces; i++) {
		mc->gobj[i].id = topo->interfaces[i].id;
		mc->gobj[i].intf = &topo->interfaces[i];
	}
	for (i = 0; i < topo->num_pads; i++) {
		mc->gobj[i].id = topo->pads[i].id;
		mc->gobj[i].pad = &topo->pads[i];
dbg("pad 0x%08x, entity id: 0x%08x", mc->gobj[i].id, topo->pads[i].entity_id);

	}
	for (i = 0; i < topo->num_links; i++) {
		mc->gobj[i].id = topo->links[i].id;
		mc->gobj[i].link = &topo->links[i];
	}

	/*
	 * If we were concerned about performance, we could now sort
	 * the objects and use binary search at the find routine
	 *
	 * We could also add some logic here to create per-interfaces
	 * and per-entities linked lists with pads and links.
	 *
	 * However, this is just a test program, so let's keep it simple
	 */
	return 0;
}

static inline
struct graph_obj *find_gobj(struct media_controller *mc, uint32_t id)
{
	int i;

	/*
	 * If we were concerned about performance, we could use bsearch
	 */
	for (i = 0; i < mc->num_gobj; i++) {
		if (mc->gobj[i].id == id)
			return &mc->gobj[i];
	}
	return NULL;
}

static int media_get_topology(struct media_controller *mc)
{
	struct media_v2_topology *topo = &mc->topo;
	int i, j, ret = 0, topology_version;

	/* First call: get the amount of elements */
	memset(topo, 0, sizeof(*topo));
	ret = ioctl(mc->fd, MEDIA_IOC_G_TOPOLOGY, topo);
	if (ret < 0) {
		logperror("MEDIA_IOC_G_TOPOLOGY faled to get numbers");
		goto error;
	}

	topology_version = topo->topology_version;

	info("version: %d", topology_version);
	info("number of entities: %d", topo->num_entities);
	info("number of interfaces: %d", topo->num_interfaces);
	info("number of pads: %d", topo->num_pads);
	info("number of links: %d", topo->num_links);

	do {
		topo->entities = calloc(topo->num_entities,
					sizeof(*topo->entities));
		if (topo->num_entities && !topo->entities)
			goto error;

		topo->interfaces = calloc(topo->num_interfaces,
					  sizeof(*topo->interfaces));
		if (topo->num_interfaces && !topo->interfaces)
			goto error;

		topo->pads = calloc(topo->num_pads,
				   sizeof(*topo->pads));
		if (topo->num_pads && !topo->pads)
			goto error;

		topo->links = calloc(topo->num_links,
				     sizeof(*topo->links));
		if (topo->num_links && !topo->links)
			goto error;

		ret = ioctl(mc->fd, MEDIA_IOC_G_TOPOLOGY, topo);
		if (ret < 0) {
			if (topo->topology_version != topology_version) {
				warn("Topology changed from version %d to %d. trying again.",
					  topology_version,
					  topo->topology_version);
				/*
				 * The logic here could be smarter, but, as
				 * topology changes should be rare, this
				 * should do the work
				 */
				free(topo->entities);
				free(topo->interfaces);
				free(topo->pads);
				free(topo->links);
				topology_version = topo->topology_version;
				continue;
			}
			logperror("MEDIA_IOC_G_TOPOLOGY faled");
			goto error;
		}
	} while (ret < 0);


	media_init_graph_obj(mc);

	for (i = 0; i < topo->num_entities; i++) {
		char *obj;
		struct media_v2_entity *entity = &topo->entities[i];
		int num_pads = 0;

		/*
		 * Count the number of patches - If this would be a
		 * real application/library, we would likely be creating
		 * either a list of an array to associate entities/pads/links
		 *
		 * However, we just want to test the API, so we don't care
		 * about performance.
		 */
		for (j = 0; j < topo->num_pads; j++) {
			if (topo->pads[j].entity_id == entity->id)
				num_pads++;
		}

		obj = objname(entity->id);
		info("entity %s: %s, num pads = %d",
		     obj, entity->name, num_pads);
		free(obj);

	}

	for (i = 0; i < topo->num_interfaces; i++) {
		char *obj;
		struct media_v2_interface *intf = &topo->interfaces[i];
		struct media_v2_intf_devnode *devnode;

		/* For now, all interfaces are devnodes */
		devnode = &intf->devnode;

		obj = objname(intf->id);
		info("interface %s: %s (%d,%d)",
		     obj, intf_type(intf->intf_type),
		     devnode->major, devnode->minor);
		free(obj);
	}

	for (i = 0; i < topo->num_links; i++) {
		struct media_v2_link *link = &topo->links[i];
		char *obj, *source_obj, *sink_obj;

		obj = objname(link->id);
		source_obj = objname(link->source_id);
		sink_obj = objname(link->sink_id);

		info("link %s: %s and %s",
		     obj, source_obj, sink_obj);

		free(obj);
		free(source_obj);
		free(sink_obj);
	}

	return 0;

error:
	if (topo->entities)
		free(topo->entities);
	if (topo->interfaces)
		free(topo->interfaces);
	if (topo->pads)
		free(topo->pads);
	if (topo->links)
		free(topo->links);

	topo->entities = NULL;
	topo->interfaces = NULL;
	topo->pads = NULL;
	topo->links = NULL;

	return ret;
}

static struct media_controller *mc_open(char *devname)
{
	struct media_controller *mc;

	mc = calloc(1, sizeof(*mc));

	mc->fd = open(devname, O_RDWR);
	if (mc->fd < 0) {
		perror("Can't open media device");
		exit errno;
	}

	return mc;
}

static int mc_close(struct media_controller *mc)
{
	int ret;

	ret = close(mc->fd);

	if (mc->gobj)
		free(mc->gobj);
	if (mc->topo.entities)
		free(mc->topo.entities);
	if (mc->topo.interfaces)
		free(mc->topo.interfaces);
	if (mc->topo.pads)
		free(mc->topo.pads);
	if (mc->topo.links)
		free(mc->topo.links);
	free(mc);

	return ret;
}

int main(void)
{
	struct media_controller *mc;

	mc = mc_open("/dev/media0");

	media_get_topology(mc);

	mc_close(mc);

	return 0;
}