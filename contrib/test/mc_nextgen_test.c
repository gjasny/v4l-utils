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
#include <sys/sysmacros.h>
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
	{"dot",		'D',	0,		0,	"show in Graphviz format", 0},
	{"max_tsout",	't',	"NUM_TSOUT",	0,	"max number of DTV TS out entities/interfaces in graphviz output (default: 5)", 0},
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
static int show_dot = 0;
static int max_tsout = 5;
static char media_device[256] = "/dev/media0";


static inline void *media_get_uptr(__u64 arg)
{
	return (void *)(uintptr_t)arg;
}

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
	case 'D':
		show_dot++;
		break;

	case 'd':
		strncpy(media_device, arg, sizeof(media_device) - 1);
		media_device[sizeof(media_device)-1] = '\0';
		break;

	case 't':
		max_tsout = atoi(arg);
		break;

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
		return "unknown intf type";
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
	case MEDIA_INTF_T_ALSA_SEQUENCER:
		return "sequencer";
	case MEDIA_INTF_T_ALSA_TIMER:
		return "ALSA timer";
	default:
		return "unknown_intf";
	}
};

static inline const char *ent_function(uint32_t function)
{
	switch (function) {

	/* DVB entities */
	case MEDIA_ENT_F_DTV_DEMOD:
		return "DTV demod";
	case MEDIA_ENT_F_TS_DEMUX:
		return "MPEG-TS demux";
	case MEDIA_ENT_F_DTV_CA:
		return "DTV CA";
	case MEDIA_ENT_F_DTV_NET_DECAP:
		return "DTV Network decap";

	/* I/O entities */
	case MEDIA_ENT_F_IO_DTV:
		return "DTV I/O";
	case MEDIA_ENT_F_IO_VBI:
		return "VBI I/O";
	case MEDIA_ENT_F_IO_SWRADIO:
		return "SDR I/O";

	/*Analog TV IF-PLL decoders */
	case MEDIA_ENT_F_IF_VID_DECODER:
		return "IF video decoder";
	case MEDIA_ENT_F_IF_AUD_DECODER:
		return "IF sound decoder";

	/* Audio Entity Functions */
	case MEDIA_ENT_F_AUDIO_CAPTURE:
		return "Audio Capture";
	case MEDIA_ENT_F_AUDIO_PLAYBACK:
		return "Audio Playback";
	case MEDIA_ENT_F_AUDIO_MIXER:
		return "Audio Mixer";

#if 0
	/* Connectors */
	case MEDIA_ENT_F_CONN_RF:
		return "RF connector";
	case MEDIA_ENT_F_CONN_SVIDEO:
		return "S-Video connector";
	case MEDIA_ENT_F_CONN_COMPOSITE:
		return "Composite connector";
#endif

	/* Entities based on MEDIA_ENT_F_OLD_BASE */
	case MEDIA_ENT_F_IO_V4L:
		return "V4L I/O";
	case MEDIA_ENT_F_CAM_SENSOR:
		return "Camera Sensor";
	case MEDIA_ENT_F_FLASH:
		return "Flash LED/light";
	case MEDIA_ENT_F_LENS:
		return "Lens";
	case MEDIA_ENT_F_ATV_DECODER:
		return "ATV decoder";
	case MEDIA_ENT_F_TUNER:
		return "tuner";

	/* Anything else */
	case MEDIA_ENT_F_V4L2_SUBDEV_UNKNOWN:
	default:
		return "unknown entity type";
	}
}

/* Ancilary function to produce an human readable ID for an object */

static char *objname(uint32_t id, char delimiter)
{
	char *name;
	int ret;

	ret = asprintf(&name, "%s%c%d", gobj_type(id), delimiter, media_localid(id));
	if (ret < 0)
		return NULL;

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
 * Code to convert devnode major, minor into a name
 *
 * This code was imported from the Media controller interface library (libmediactl.c) under LGPL v2.1
 *      Copyright (C) 2010-2014 Ideas on board SPRL
 *      Contact: Laurent Pinchart <laurent.pinchart@ideasonboard.com>
 */
#ifdef HAVE_LIBUDEV

#include <libudev.h>

static int media_open_ifname(void **priv)
{
	struct udev **udev = (struct udev **)priv;

	*udev = udev_new();
	if (*udev == NULL)
		return -ENOMEM;
	return 0;
}

static void media_close_ifname(void *priv)
{
	struct udev *udev = (struct udev *)priv;

	if (udev != NULL)
		udev_unref(udev);
}

static char *media_get_ifname_udev(struct media_v2_intf_devnode *devnode, struct udev *udev)
{
	struct udev_device *device;
	dev_t devnum;
	const char *p;
	char *name = NULL;
	int ret;

	if (udev == NULL)
		return NULL;

	devnum = makedev(devnode->major, devnode->minor);
	device = udev_device_new_from_devnum(udev, 'c', devnum);
	if (device) {
		p = udev_device_get_devnode(device);
		if (p) {
			ret = asprintf(&name, "%s", p);
			if (ret < 0)
				return NULL;
		}
	}

	udev_device_unref(device);

	return name;
}
#else
static inline char *media_open_ifname(void **priv) { return NULL; } ;
static void media_close_ifname(void *priv) {};
#endif	/* HAVE_LIBUDEV */

char *media_get_ifname(struct media_v2_interface *intf, void *priv)
{
	struct media_v2_intf_devnode *devnode;
	struct stat devstat;
	char devname[32];
	char sysname[32];
	char target[1024];
	char *p, *name = NULL;
	int ret;

	/* Only handles Devnode interfaces */
	if (media_type(intf->id) != MEDIA_GRAPH_INTF_DEVNODE)
		return NULL;

	devnode = &intf->devnode;

#ifdef HAVE_LIBUDEV
	/* Try first to convert using udev */
	name = media_get_ifname_udev(devnode, priv);
	if (name)
		return name;
#endif

	/* Failed. Let's fallback to get it via sysfs */

	sprintf(sysname, "/sys/dev/char/%u:%u",
		devnode->major, devnode->minor);

	ret = readlink(sysname, target, sizeof(target) - 1);
	if (ret < 0)
		return NULL;

	target[ret] = '\0';
	p = strrchr(target, '/');
	if (p == NULL)
		return NULL;

	sprintf(devname, "/dev/%s", p + 1);
	if (strstr(p + 1, "dvb")) {
		char *s = p + 1;

		if (strncmp(s, "dvb", 3))
			return NULL;
		s += 3;
		p = strchr(s, '.');
		if (!p)
			return NULL;
		*p = '/';
		sprintf(devname, "/dev/dvb/adapter%s", s);
	} else {
		sprintf(devname, "/dev/%s", p + 1);
	}
	ret = stat(devname, &devstat);
	if (ret < 0)
		return NULL;

	/* Sanity check: udev might have reordered the device nodes.
	 * Make sure the major/minor match. We should really use
	 * libudev.
	 */
	if (major(devstat.st_rdev) == intf->devnode.major &&
	    minor(devstat.st_rdev) == intf->devnode.minor) {
		ret = asprintf(&name, "%s", devname);
		if (ret < 0)
			return NULL;
	}
	return name;
}

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
	/* Currently, used only for pads->entity */
	struct graph_obj *parent;
	/* Used only for entities */
	int num_pads, num_pad_sinks, num_pad_sources;
};

struct media_controller {
	int fd;
	struct media_v2_topology topo;
	struct media_device_info info;

	int num_gobj;
	struct graph_obj *gobj;
};

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

static int media_init_graph_obj(struct media_controller *mc)
{
	struct media_v2_topology *topo = &mc->topo;
	int i, j, num_gobj;
	int idx = 0;
	struct media_v2_entity *entities = media_get_uptr(topo->ptr_entities);
	struct media_v2_interface *interfaces = media_get_uptr(topo->ptr_interfaces);
	struct media_v2_pad *pads = media_get_uptr(topo->ptr_pads);
	struct media_v2_link *links = media_get_uptr(topo->ptr_links);

	num_gobj = topo->num_entities + topo->num_interfaces
		   + topo->num_pads + topo->num_links;

	mc->gobj = calloc(num_gobj, sizeof(*mc->gobj));
	if (!mc->gobj) {
		logperror("couldn't allocate space for graph_obj");
		return -ENOMEM;
	}

	for (i = 0; i < topo->num_pads; i++) {
		mc->gobj[idx].id = pads[i].id;
		mc->gobj[idx].pad = &pads[i];
		idx++;
	}

	mc->num_gobj = num_gobj;

	for (i = 0; i < topo->num_entities; i++) {
		struct graph_obj *gobj;

		mc->gobj[idx].id = entities[i].id;
		mc->gobj[idx].entity = &entities[i];

		/* Set the parent object for the pads */
		for (j = 0; j < topo->num_pads; j++) {
			if (pads[j].entity_id != entities[i].id)
				continue;

			/* The data below is useful for Graphviz generation */
			mc->gobj[idx].num_pads++;
			if (pads[j].flags & MEDIA_PAD_FL_SINK)
				mc->gobj[idx].num_pad_sinks++;
			if (pads[j].flags & MEDIA_PAD_FL_SOURCE)
				mc->gobj[idx].num_pad_sources++;

			gobj = find_gobj(mc, pads[j].id);
			if (gobj)
				gobj->parent = &mc->gobj[idx];
		}
		idx++;
	}
	for (i = 0; i < topo->num_interfaces; i++) {
		mc->gobj[idx].id = interfaces[i].id;
		mc->gobj[idx].intf = &interfaces[i];
		idx++;
	}
	for (i = 0; i < topo->num_links; i++) {
		mc->gobj[idx].id = links[i].id;
		mc->gobj[idx].link = &links[i];
		idx++;
	}

	mc->num_gobj = num_gobj;

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

static void media_show_entities(struct media_controller *mc)
{
	struct media_v2_topology *topo = &mc->topo;
	struct media_v2_entity *entities = media_get_uptr(topo->ptr_entities);
	struct media_v2_pad *pads = media_get_uptr(topo->ptr_pads);
	int i, j;

	for (i = 0; i < topo->num_entities; i++) {
		struct media_v2_entity *entity = &entities[i];
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
			if (pads[j].entity_id != entity->id)
				continue;

			num_pads++;
			if (pads[j].flags & MEDIA_PAD_FL_SINK)
				num_sinks++;
			if (pads[j].flags & MEDIA_PAD_FL_SOURCE)
				num_sources++;
		}

		obj = objname(entity->id, '#');
		show(YELLOW, 0, "entity %s: '%s' %s, %d pad(s)",
		     obj, ent_function(entity->function),
		     entity->name, num_pads);
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
	struct media_v2_interface *interfaces = media_get_uptr(topo->ptr_interfaces);
	void *priv = NULL;
	int i;

	media_open_ifname(&priv);
	for (i = 0; i < topo->num_interfaces; i++) {
		char *obj, *devname;
		struct media_v2_interface *intf = &interfaces[i];

		obj = objname(intf->id, '#');
		devname = media_get_ifname(intf, priv);
		show(GREEN, 0, "interface %s: %s %s\n",
		     obj, intf_type(intf->intf_type),
		     devname);
		free(obj);
		free(devname);
	}
	media_close_ifname(priv);
}

static void media_show_links(struct media_controller *mc)
{
	struct media_v2_topology *topo = &mc->topo;
	struct media_v2_link *links = media_get_uptr(topo->ptr_links);
	int i, color;

	for (i = 0; i < topo->num_links; i++) {
		struct media_v2_link *link = &links[i];
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

		obj = objname(link->id, '#');
		source_obj = objname(link->source_id, '#');
		sink_obj = objname(link->sink_id, '#');

		if ((link->flags & MEDIA_LNK_FL_LINK_TYPE) == MEDIA_LNK_FL_INTERFACE_LINK)
			show(color, 0, "interface ");
		else
			show(color, 0, "data ");
		show(color, 0, "link %s: %s %s %s",
		     obj, source_obj,
		     ((link->flags & MEDIA_LNK_FL_LINK_TYPE) == MEDIA_LNK_FL_INTERFACE_LINK) ? "<=>" : "=>",
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

static void media_get_device_info(struct media_controller *mc)
{
	struct media_device_info *info = &mc->info;
	int ret = 0;

	ret = ioctl(mc->fd, MEDIA_IOC_DEVICE_INFO, info);
	if (ret < 0) {
		logperror("MEDIA_IOC_DEVICE_INFO failed");
		return;
	}
	if (show_dot)
		return;

	show(WHITE, 0, "Device: %s (driver %s)\n",
	     info->model, info->driver);
	if (info->serial[0])
		show(WHITE, 0, "Serial: %s\n", info->serial);
	show(WHITE, 0, "Bus: %s\n", info->bus_info);
}

static int media_get_topology(struct media_controller *mc)
{
	struct media_v2_topology *topo = &mc->topo;
	int ret = 0, topology_version;

	media_get_device_info(mc);

	/* First call: get the amount of elements */
	memset(topo, 0, sizeof(*topo));
	ret = ioctl(mc->fd, MEDIA_IOC_G_TOPOLOGY, topo);
	if (ret < 0) {
		logperror("MEDIA_IOC_G_TOPOLOGY faled to get numbers");
		goto error;
	}

	topology_version = topo->topology_version;

	if (!show_dot) {
		show(WHITE, 0, "version: %d\n", topology_version);
		show(WHITE, 0, "number of entities: %d\n", topo->num_entities);
		show(WHITE, 0, "number of interfaces: %d\n", topo->num_interfaces);
		show(WHITE, 0, "number of pads: %d\n", topo->num_pads);
		show(WHITE, 0, "number of links: %d\n", topo->num_links);
	}

	do {
		topo->ptr_entities = (__u64)calloc(topo->num_entities,
					sizeof(struct media_v2_entity));
		if (topo->num_entities && !topo->ptr_entities)
			goto error;

		topo->ptr_interfaces = (__u64)calloc(topo->num_interfaces,
					  sizeof(struct media_v2_interface));
		if (topo->num_interfaces && !topo->ptr_interfaces)
			goto error;

		topo->ptr_pads = (__u64)calloc(topo->num_pads,
				   sizeof(struct media_v2_pad));
		if (topo->num_pads && !topo->ptr_pads)
			goto error;

		topo->ptr_links = (__u64)calloc(topo->num_links,
				     sizeof(struct media_v2_link));
		if (topo->num_links && !topo->ptr_links)
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
				free((void *)topo->ptr_entities);
				free((void *)topo->ptr_interfaces);
				free((void *)topo->ptr_pads);
				free((void *)topo->ptr_links);
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
	if (topo->ptr_entities)
		free((void *)topo->ptr_entities);
	if (topo->ptr_interfaces)
		free((void *)topo->ptr_interfaces);
	if (topo->ptr_pads)
		free((void *)topo->ptr_pads);
	if (topo->ptr_links)
		free((void *)topo->ptr_links);

	topo->ptr_entities = 0;
	topo->ptr_interfaces = 0;
	topo->ptr_pads = 0;
	topo->ptr_links = 0;

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
	if (mc->topo.ptr_entities)
		free((void *)mc->topo.ptr_entities);
	if (mc->topo.ptr_interfaces)
		free((void *)mc->topo.ptr_interfaces);
	if (mc->topo.ptr_pads)
		free((void *)mc->topo.ptr_pads);
	if (mc->topo.ptr_links)
		free((void *)mc->topo.ptr_links);
	free(mc);

	return ret;
}

/* Graphviz styles */
#define DOT_HEADER	"digraph board {\n\trankdir=TB\n\tcolorscheme=x11\n"
#define STYLE_INTF	"shape=box, style=filled, fillcolor=yellow"
#define STYLE_ENTITY	"shape=Mrecord, style=filled, fillcolor=lightblue"
#define STYLE_ENT_SRC	"shape=Mrecord, style=filled, fillcolor=cadetblue"
#define STYLE_ENT_SINK	"shape=Mrecord, style=filled, fillcolor=aquamarine"
#define STYLE_DATA_LINK	"color=blue"
#define STYLE_INTF_LINK	"dir=\"none\" color=\"orange\""

static void media_show_graphviz(struct media_controller *mc)
{
	struct media_v2_topology *topo = &mc->topo;
	struct media_v2_entity *entities = media_get_uptr(topo->ptr_entities);
	struct media_v2_interface *interfaces = media_get_uptr(topo->ptr_interfaces);
	struct media_v2_pad *pads = media_get_uptr(topo->ptr_pads);
	struct media_v2_link *links = media_get_uptr(topo->ptr_links);

	int i, j;
	char *obj;
	void *priv = NULL;

	printf("%s", DOT_HEADER);

	if (mc->info.model[0])
		printf("\tlabelloc=\"t\"\n\tlabel=\"%s\n driver:%s, bus: %s\n\"\n",
		       mc->info.model, mc->info.driver, mc->info.bus_info);

	media_open_ifname(&priv);
	for (i = 0; i < topo->num_interfaces; i++) {
		struct media_v2_interface *intf = &interfaces[i];
		char *devname;

		obj = objname(intf->id, '_');
		devname = media_get_ifname(intf, priv);
		printf("\t%s [label=\"%s\\n%s\\n%s\", " STYLE_INTF"]\n",
		       obj, obj, intf_type(intf->intf_type),
		     devname);
		free(obj);
		free(devname);
	}
	media_close_ifname(priv);

#define DEMUX_TSOUT	"demux-tsout #"
#define DVR_TSOUT	"dvr-tsout #"

	for (i = 0; i < topo->num_entities; i++) {
		struct media_v2_entity *entity = &entities[i];
		struct graph_obj *gobj;
		int first, idx;

		if (max_tsout) {
			int i = 0;

			if (!strncmp(entity->name, DEMUX_TSOUT, strlen(DEMUX_TSOUT)))
				i = atoi(&entity->name[strlen(DEMUX_TSOUT)]);
			else if (!strncmp(entity->name, DVR_TSOUT, strlen(DVR_TSOUT)))
				i = atoi(&entity->name[strlen(DVR_TSOUT)]);

			if (i >= max_tsout)
				continue;
		}

		gobj = find_gobj(mc, entity->id);
		obj = objname(entity->id, '_');
		printf("\t%s [label=\"{", obj);
		free(obj);

		/* Print the sink pads */
		if (!gobj || gobj->num_pad_sinks) {
			first = 1;
			idx = 0;
			printf("{");
			for (j = 0; j < topo->num_pads; j++) {
				if (pads[j].entity_id != entity->id)
					continue;

				if (pads[j].flags & MEDIA_PAD_FL_SINK) {
					if (first)
						first = 0;
					else
						printf (" | ");

					obj = objname(pads[j].id, '_');
					printf("<%s> %d", obj, idx);
					free(obj);
				}
				idx++;
			}
			printf("} | ");
		}
		obj = objname(entity->id, '_');
		printf("%s\\n%s\\n%s", obj, ent_function(entity->function), entity->name);
		free(obj);

		/* Print the source pads */
		if (!gobj || gobj->num_pad_sources) {
			int pad_count = 0;
			first = 1;
			idx = 0;
			printf(" | {");

			for (j = 0; j < topo->num_pads; j++) {
				if (pads[j].entity_id != entity->id)
					continue;

				if (entity->function == MEDIA_ENT_F_TS_DEMUX && pad_count > max_tsout)
					continue;
				pad_count++;

				if (pads[j].flags & MEDIA_PAD_FL_SOURCE) {
					if (first)
						first = 0;
					else
						printf (" | ");

					obj = objname(pads[j].id, '_');
					printf("<%s> %d", obj, idx);
					free(obj);
				}
				idx++;
			}
			printf("}");
		}
		printf("}\", ");
		if (!gobj || (gobj->num_pad_sources && gobj->num_pad_sinks))
			printf(STYLE_ENTITY"]\n");
		else if (gobj->num_pad_sinks)
			printf(STYLE_ENT_SINK"]\n");
		else
			printf(STYLE_ENT_SRC"]\n");
	}

	for (i = 0; i < topo->num_links; i++) {
		struct media_v2_link *link = &links[i];
		char *source_pad_obj, *sink_pad_obj;
		char *source_ent_obj, *sink_ent_obj;

		if (media_type(link->source_id) == MEDIA_GRAPH_PAD) {
			struct media_v2_entity *source, *sink;
			struct graph_obj *gobj, *parent;

			source_pad_obj = objname(link->source_id, '_');
			gobj = find_gobj(mc, link->source_id);
			if (!gobj) {
				show(RED, 0, "Graph object for %s not found\n",
				     source_pad_obj);
				free(source_pad_obj);
				continue;
			}
			parent = gobj->parent;
			if (!parent) {
				show(RED, 0, "Sink entity for %s not found\n",
				     source_pad_obj);
				free(source_pad_obj);
				continue;
			}
			source = parent->entity;

			sink_pad_obj = objname(link->sink_id, '_');

			gobj = find_gobj(mc, link->sink_id);
			if (!gobj) {
				show(RED, 0, "Graph object for %s not found\n",
				     sink_pad_obj);
				free(source_pad_obj);
				free(sink_pad_obj);
				continue;
			}
			parent = gobj->parent;
			if (!parent) {
				show(RED, 0, "Sink entity for %s not found\n",
				     sink_pad_obj);
				free(source_pad_obj);
				free(sink_pad_obj);
				continue;
			}
			sink = parent->entity;

			if (max_tsout) {
				int i = 0;

				if (!strncmp(sink->name, DEMUX_TSOUT, strlen(DEMUX_TSOUT)))
					i = atoi(&sink->name[strlen(DEMUX_TSOUT)]);
				else if (!strncmp(sink->name, DVR_TSOUT, strlen(DVR_TSOUT)))
					i = atoi(&sink->name[strlen(DVR_TSOUT)]);
				if (i >= max_tsout)
					continue;
			}

			source_ent_obj = objname(source->id, '_');
			sink_ent_obj = objname(sink->id, '_');

			printf("\t%s:%s -> %s:%s [" STYLE_DATA_LINK,
			       source_ent_obj, source_pad_obj,
			       sink_ent_obj, sink_pad_obj);
			if (!(link->flags & MEDIA_LNK_FL_ENABLED))
				printf(" style=\"dashed\"");
			printf("]\n");

			free(source_pad_obj);
			free(sink_pad_obj);
			free(source_ent_obj);
			free(sink_ent_obj);
		}

		if (media_type(link->source_id) == MEDIA_GRAPH_INTF_DEVNODE) {
			struct media_v2_entity *sink;
			struct graph_obj *gobj;

			sink_ent_obj = objname(link->sink_id, '_');
			gobj = find_gobj(mc, link->sink_id);
			if (!gobj) {
				show(RED, 0, "Graph object for %s not found\n",
				     sink_ent_obj);
				free(sink_ent_obj);
				continue;
			}
			sink = gobj->entity;

			if (max_tsout) {
				int i = 0;

				if (!strncmp(sink->name, DEMUX_TSOUT, strlen(DEMUX_TSOUT)))
					i = atoi(&sink->name[strlen(DEMUX_TSOUT)]);
				else if (!strncmp(sink->name, DVR_TSOUT, strlen(DVR_TSOUT)))
					i = atoi(&sink->name[strlen(DVR_TSOUT)]);
				if (i >= max_tsout)
					continue;
			}

			source_ent_obj = objname(link->source_id, '_');

			printf("\t%s -> %s [" STYLE_INTF_LINK,
			       source_ent_obj, sink_ent_obj);
			if (!(link->flags & MEDIA_LNK_FL_ENABLED))
				printf(" style=\"dashed\"");
			printf("]\n");

			free(source_ent_obj);
			free(sink_ent_obj);
		}
	}

	printf ("}\n");
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

	if (show_dot) {
		media_show_graphviz(mc);
	} else {
		if (show_entities)
			media_show_entities(mc);

		if (show_interfaces)
			media_show_interfaces(mc);

		if (show_data_links || show_intf_links)
			media_show_links(mc);
	}

	mc_close(mc);

	return 0;
}
