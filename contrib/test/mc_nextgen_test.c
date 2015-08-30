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

#include <config.h>

#include <linux/media.h>

#include <argp.h>

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

#define PROGRAM_NAME	"mc_nextgen_test"

const char *argp_program_version = PROGRAM_NAME " version " V4L_UTILS_VERSION;
const char *argp_program_bug_address = "Mauro Carvalho Chehab <m.chehab@samsung.com>";

static const char doc[] = "\nA testing tool for the MC next geneneration API\n";

static const struct argp_option options[] = {
	{"entities",	'e',	0,		0,	"show entities", 0},
	{"interfaces",	'i',	0,		0,	"show pads", 0},
	{"data-links",	'l',	0,		0,	"show data links", 0},
	{"intf-links",	'I',	0,		0,	"show interface links", 0},

	{"device",	'd',	"DEVICE",	0,	"media controller device (default: /dev/media0", 0},

	{"help",        '?',	0,		0,	"Give this help list", -1},
	{"usage",	-3,	0,		0,	"Give a short usage message"},
	{"version",	'V',	0,		0,	"Print program version", -1},
	{ 0, 0, 0, 0, 0, 0 }
};

static int show_entities = 0;
static int show_interfaces = 0;
static int show_data_links = 0;
static int show_intf_links = 0;
static char media_device[256] = "/dev/media0";

static error_t parse_opt(int k, char *arg, struct argp_state *state)
{
	switch (k) {
	case 'e':
		show_entities++;
		break;
	case 'i':
		show_interfaces++;
		break;
	case 'l':
		show_data_links++;
		break;
	case 'I':
		show_intf_links++;
		break;

	case 'd':
		strncpy(media_device, arg, sizeof(media_device) - 1);
		media_device[sizeof(media_device)-1] = '\0';

	case '?':
		argp_state_help(state, state->out_stream,
				ARGP_HELP_SHORT_USAGE | ARGP_HELP_LONG
				| ARGP_HELP_DOC);
		fprintf(state->out_stream, "\nReport bugs to %s.\n",
			argp_program_bug_address);
		exit(0);
	case 'V':
		fprintf (state->out_stream, "%s\n", argp_program_version);
		exit(0);
	case -3:
		argp_state_help(state, state->out_stream, ARGP_HELP_USAGE);
		exit(0);
	default:
		return ARGP_ERR_UNKNOWN;
	}
	return 0;
}

static struct argp argp = {
	.options = options,
	.parser = parse_opt,
	.doc = doc,
};


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
		return "v4l2-subdev";
	case MEDIA_INTF_T_V4L_SWRADIO:
		return "swradio";
	case MEDIA_INTF_T_ALSA_PCM_CAPTURE:
		return "pcm-capture";
	case MEDIA_INTF_T_ALSA_PCM_PLAYBACK:
		return "pcm-playback";
	case MEDIA_INTF_T_ALSA_CONTROL:
		return "alsa-control";
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

enum ansi_colors {
	BLACK = 30,
	RED,
	GREEN,
	YELLOW,
	BLUE,
	MAGENTA,
	CYAN,
	WHITE
};

#define NORMAL_COLOR "\033[0;%dm"
#define BRIGHT_COLOR "\033[1;%dm"

void show(int color, int bright, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);

	if (isatty(STDOUT_FILENO)) {
		if (bright)
			printf(BRIGHT_COLOR, color);
		else
			printf(NORMAL_COLOR, color);
	}

	vprintf(fmt, ap);

	va_end(ap);
}

#define logperror(msg) do {\
       show(RED, 0, "%s: %s", msg, strerror(errno)); \
       printf("\n"); \
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
		logperror("couldn't allocate space for graph_obj");
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

static void media_show_entities(struct media_controller *mc)
{
	struct media_v2_topology *topo = &mc->topo;
	int i, j;

	for (i = 0; i < topo->num_entities; i++) {
		struct media_v2_entity *entity = &topo->entities[i];
		char *obj;
		int num_pads = 0;
		int num_sinks = 0;
		int num_sources = 0;

		/*
		 * Count the number of patches - If this would be a
		 * real application/library, we would likely be creating
		 * either a list of an array to associate entities/pads/links
		 *
		 * However, we just want to test the API, so we don't care
		 * about performance.
		 */
		for (j = 0; j < topo->num_pads; j++) {
			if (topo->pads[j].entity_id != entity->id)
				continue;

			num_pads++;
			if (topo->pads[j].flags == MEDIA_PAD_FL_SINK)
				num_sinks++;
			if (topo->pads[j].flags == MEDIA_PAD_FL_SOURCE)
				num_sources++;
		}

		obj = objname(entity->id);
		show(YELLOW, 0, "entity %s: %s, %d pad(s)",
		     obj, entity->name, num_pads);
		if (num_sinks)
			show(YELLOW, 0,", %d sink(s)", num_sinks);
		if (num_sources)
			show(YELLOW, 0,", %d source(s)", num_sources);
		printf("\n");

		free(obj);
	}
}

static void media_show_interfaces(struct media_controller *mc)
{
	struct media_v2_topology *topo = &mc->topo;
	int i;

	for (i = 0; i < topo->num_interfaces; i++) {
		char *obj;
		struct media_v2_interface *intf = &topo->interfaces[i];
		struct media_v2_intf_devnode *devnode;

		/* For now, all interfaces are devnodes */
		devnode = &intf->devnode;

		obj = objname(intf->id);
		show(GREEN, 0, "interface %s: %s (%d,%d)\n",
		     obj, intf_type(intf->intf_type),
		     devnode->major, devnode->minor);
		free(obj);
	}
}

static void media_show_links(struct media_controller *mc)
{
	struct media_v2_topology *topo = &mc->topo;
	int i, color;

	for (i = 0; i < topo->num_links; i++) {
		struct media_v2_link *link = &topo->links[i];
		char *obj, *source_obj, *sink_obj;

		color = BLUE;
		if (media_type(link->source_id) == MEDIA_GRAPH_PAD) {
			if (!show_data_links)
				continue;
			color = CYAN;
		}

		if (media_type(link->source_id) == MEDIA_GRAPH_INTF_DEVNODE) {
			if (!show_intf_links)
				continue;
		}

		obj = objname(link->id);
		source_obj = objname(link->source_id);
		sink_obj = objname(link->sink_id);

		if (link->flags & MEDIA_NEW_LNK_FL_INTERFACE_LINK)
			show(color, 0, "interface ");
		else
			show(color, 0, "data ");
		show(color, 0, "link %s: %s %s %s",
		     obj, source_obj,
		     (link->flags & MEDIA_NEW_LNK_FL_INTERFACE_LINK) ? "<=>" : "=>",
		     sink_obj);
		if (link->flags & MEDIA_LNK_FL_IMMUTABLE)
			show(color, 0, " [IMMUTABLE]");
		if (link->flags & MEDIA_LNK_FL_DYNAMIC)
			show(color, 0, " [DYNAMIC]");
		if (link->flags & MEDIA_LNK_FL_ENABLED)
			show(color, 1, " [ENABLED]");

		printf("\n");

		free(obj);
		free(source_obj);
		free(sink_obj);
	}
}
static int media_get_topology(struct media_controller *mc)
{
	struct media_v2_topology *topo = &mc->topo;
	int ret = 0, topology_version;

	/* First call: get the amount of elements */
	memset(topo, 0, sizeof(*topo));
	ret = ioctl(mc->fd, MEDIA_IOC_G_TOPOLOGY, topo);
	if (ret < 0) {
		logperror("MEDIA_IOC_G_TOPOLOGY faled to get numbers");
		goto error;
	}

	topology_version = topo->topology_version;

	show(WHITE, 0, "version: %d\n", topology_version);
	show(WHITE, 0, "number of entities: %d\n", topo->num_entities);
	show(WHITE, 0, "number of interfaces: %d\n", topo->num_interfaces);
	show(WHITE, 0, "number of pads: %d\n", topo->num_pads);
	show(WHITE, 0, "number of links: %d\n", topo->num_links);

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
				show(WHITE, 0, "Topology changed from version %d to %d. trying again.\n",
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
		logperror("Can't open media device");
		return NULL;
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

int main(int argc, char *argv[])
{
	struct media_controller *mc;
	int rc;

	argp_parse(&argp, argc, argv, ARGP_NO_HELP | ARGP_NO_EXIT, 0, 0);

	mc = mc_open(media_device);
	if (!mc)
	return -1;

	rc = media_get_topology(mc);
	if (rc) {
		mc_close(mc);
		return -2;
	}

	if (show_entities)
		media_show_entities(mc);

	if (show_interfaces)
		media_show_interfaces(mc);

	if (show_data_links || show_intf_links)
		media_show_links(mc);

	mc_close(mc);

	return 0;
}
