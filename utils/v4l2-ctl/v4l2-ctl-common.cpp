#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <dirent.h>
#include <math.h>

#include "v4l2-ctl.h"

#ifdef HAVE_SYS_KLOG_H
#include <sys/klog.h>
#endif

#include <list>
#include <vector>
#include <map>
#include <algorithm>

struct ctrl_subset {
	unsigned offset[V4L2_CTRL_MAX_DIMS];
	unsigned size[V4L2_CTRL_MAX_DIMS];
};

typedef std::map<unsigned, std::vector<struct v4l2_ext_control> > class2ctrls_map;

typedef std::map<std::string, struct v4l2_query_ext_ctrl> ctrl_qmap;
static ctrl_qmap ctrl_str2q;
typedef std::map<unsigned, std::string> ctrl_idmap;
static ctrl_idmap ctrl_id2str;

typedef std::map<std::string, ctrl_subset> ctrl_subset_map;
static ctrl_subset_map ctrl_subsets;

typedef std::list<std::string> ctrl_get_list;
static ctrl_get_list get_ctrls;

typedef std::map<std::string, std::string> ctrl_set_map;
static ctrl_set_map set_ctrls;

typedef std::vector<std::string> dev_vec;
typedef std::map<std::string, std::string> dev_map;

static enum v4l2_priority prio = V4L2_PRIORITY_UNSET;

static bool have_query_ext_ctrl;

void common_usage(void)
{
	printf("\nGeneral/Common options:\n"
	       "  --all              display all information available\n"
	       "  -C, --get-ctrl=<ctrl>[,<ctrl>...]\n"
	       "                     get the value of the controls [VIDIOC_G_EXT_CTRLS]\n"
	       "  -c, --set-ctrl=<ctrl>=<val>[,<ctrl>=<val>...]\n"
	       "                     set the value of the controls [VIDIOC_S_EXT_CTRLS]\n"
	       "  -D, --info         show driver info [VIDIOC_QUERYCAP]\n"
	       "  -d, --device=<dev> use device <dev> instead of /dev/video0\n"
	       "                     if <dev> starts with a digit, then /dev/video<dev> is used\n"
	       "  -e, --out-device=<dev> use device <dev> for output streams instead of the\n"
	       "                     default device as set with --device\n"
	       "                     if <dev> starts with a digit, then /dev/video<dev> is used\n"
	       "  -h, --help         display this help message\n"
	       "  --help-all         all options\n"
	       "  --help-io          input/output options\n"
	       "  --help-misc        miscellaneous options\n"
	       "  --help-overlay     overlay format options\n"
	       "  --help-sdr         SDR format options\n"
	       "  --help-selection   crop/selection options\n"
	       "  --help-stds        standards and other video timings options\n"
	       "  --help-streaming   streaming options\n"
	       "  --help-tuner       tuner/modulator options\n"
	       "  --help-vbi         VBI format options\n"
	       "  --help-vidcap      video capture format options\n"
	       "  --help-vidout      vidout output format options\n"
	       "  --help-edid        edid handling options\n"
	       "  -k, --concise      be more concise if possible.\n"
	       "  -l, --list-ctrls   display all controls and their values [VIDIOC_QUERYCTRL]\n"
	       "  -L, --list-ctrls-menus\n"
	       "		     display all controls and their menus [VIDIOC_QUERYMENU]\n"
	       "  -r, --subset=<ctrl>[,<offset>,<size>]+\n"
	       "                     the subset of the N-dimensional array to get/set for control <ctrl>,\n"
	       "                     for every dimension an (<offset>, <size>) tuple is given.\n"
#ifndef NO_LIBV4L2
	       "  -w, --wrapper      use the libv4l2 wrapper library.\n"
#endif
	       "  --list-devices     list all v4l devices\n"
	       "  --log-status       log the board status in the kernel log [VIDIOC_LOG_STATUS]\n"
	       "  --get-priority     query the current access priority [VIDIOC_G_PRIORITY]\n"
	       "  --set-priority=<prio>\n"
	       "                     set the new access priority [VIDIOC_S_PRIORITY]\n"
	       "                     <prio> is 1 (background), 2 (interactive) or 3 (record)\n"
	       "  --silent           only set the result code, do not print any messages\n"
	       "  --sleep=<secs>     sleep <secs>, call QUERYCAP and close the file handle\n"
	       "  --verbose          turn on verbose ioctl status reporting\n"
	       );
}

static const char *prefixes[] = {
	"video",
	"radio",
	"vbi",
	"swradio",
	"v4l-subdev",
	NULL
};

static bool is_v4l_dev(const char *name)
{
	for (unsigned i = 0; prefixes[i]; i++) {
		unsigned l = strlen(prefixes[i]);

		if (!memcmp(name, prefixes[i], l)) {
			if (isdigit(name[l]))
				return true;
		}
	}
	return false;
}

static int calc_node_val(const char *s)
{
	int n = 0;

	s = strrchr(s, '/') + 1;

	for (unsigned i = 0; prefixes[i]; i++) {
		unsigned l = strlen(prefixes[i]);

		if (!memcmp(s, prefixes[i], l)) {
			n = i << 8;
			n += atol(s + l);
			return n;
		}
	}
	return 0;
}

static bool sort_on_device_name(const std::string &s1, const std::string &s2)
{
	int n1 = calc_node_val(s1.c_str());
	int n2 = calc_node_val(s2.c_str());

	return n1 < n2;
}

static void list_devices()
{
	DIR *dp;
	struct dirent *ep;
	dev_vec files;
	dev_map links;
	dev_map cards;
	struct v4l2_capability vcap;

	dp = opendir("/dev");
	if (dp == NULL) {
		perror ("Couldn't open the directory");
		return;
	}
	while ((ep = readdir(dp)))
		if (is_v4l_dev(ep->d_name))
			files.push_back(std::string("/dev/") + ep->d_name);
	closedir(dp);

	/* Find device nodes which are links to other device nodes */
	for (dev_vec::iterator iter = files.begin();
			iter != files.end(); ) {
		char link[64+1];
		int link_len;
		std::string target;

		link_len = readlink(iter->c_str(), link, 64);
		if (link_len < 0) {	/* Not a link or error */
			iter++;
			continue;
		}
		link[link_len] = '\0';

		/* Only remove from files list if target itself is in list */
		if (link[0] != '/')	/* Relative link */
			target = std::string("/dev/");
		target += link;
		if (find(files.begin(), files.end(), target) == files.end()) {
			iter++;
			continue;
		}

		/* Move the device node from files to links */
		if (links[target].empty())
			links[target] = *iter;
		else
			links[target] += ", " + *iter;
		files.erase(iter);
	}

	std::sort(files.begin(), files.end(), sort_on_device_name);

	for (dev_vec::iterator iter = files.begin();
			iter != files.end(); ++iter) {
		int fd = open(iter->c_str(), O_RDWR);
		std::string bus_info;

		if (fd < 0)
			continue;
		int err = ioctl(fd, VIDIOC_QUERYCAP, &vcap);
		close(fd);
		if (err)
			continue;
		bus_info = (const char *)vcap.bus_info;
		if (cards[bus_info].empty())
			cards[bus_info] += std::string((char *)vcap.card) + " (" + bus_info + "):\n";
		cards[bus_info] += "\t" + (*iter);
		if (!(links[*iter].empty()))
			cards[bus_info] += " <- " + links[*iter];
		cards[bus_info] += "\n";
	}
	for (dev_map::iterator iter = cards.begin();
			iter != cards.end(); ++iter) {
		printf("%s\n", iter->second.c_str());
	}
}

static std::string name2var(const char *name)
{
	std::string s;
	int add_underscore = 0;

	while (*name) {
		if (isalnum(*name)) {
			if (add_underscore)
				s += '_';
			add_underscore = 0;
			s += std::string(1, tolower(*name));
		}
		else if (s.length()) add_underscore = 1;
		name++;
	}
	return s;
}

static std::string safename(const unsigned char *name)
{
	std::string s;

	while (*name) {
		if (*name == '\n') {
			s += "\\n";
		}
		else if (*name == '\r') {
			s += "\\r";
		}
		else if (*name == '\f') {
			s += "\\f";
		}
		else if (*name == '\\') {
			s += "\\\\";
		}
		else if ((*name & 0x7f) < 0x20) {
			char buf[3];

			sprintf(buf, "%02x", *name);
			s += "\\x";
			s += buf;
		}
		else {
			s += *name;
		}
		name++;
	}
	return s;
}

static std::string safename(const char *name)
{
	return safename((const unsigned char *)name);
}

static std::string ctrlflags2s(__u32 flags)
{
	static const flag_def def[] = {
		{ V4L2_CTRL_FLAG_GRABBED,    "grabbed" },
		{ V4L2_CTRL_FLAG_DISABLED,   "disabled" },
		{ V4L2_CTRL_FLAG_READ_ONLY,  "read-only" },
		{ V4L2_CTRL_FLAG_UPDATE,     "update" },
		{ V4L2_CTRL_FLAG_INACTIVE,   "inactive" },
		{ V4L2_CTRL_FLAG_SLIDER,     "slider" },
		{ V4L2_CTRL_FLAG_WRITE_ONLY, "write-only" },
		{ V4L2_CTRL_FLAG_VOLATILE,   "volatile" },
		{ V4L2_CTRL_FLAG_HAS_PAYLOAD,"has-payload" },
		{ V4L2_CTRL_FLAG_EXECUTE_ON_WRITE, "execute-on-write" },
		{ 0, NULL }
	};
	return flags2s(flags, def);
}

static void print_qctrl(int fd, struct v4l2_query_ext_ctrl *queryctrl,
		struct v4l2_ext_control *ctrl, int show_menus)
{
	struct v4l2_querymenu qmenu;
	std::string s = name2var(queryctrl->name);
	unsigned i;

	memset(&qmenu, 0, sizeof(qmenu));
	qmenu.id = queryctrl->id;
	switch (queryctrl->type) {
	case V4L2_CTRL_TYPE_INTEGER:
		printf("%31s (int)    : min=%lld max=%lld step=%lld default=%lld",
				s.c_str(),
				queryctrl->minimum, queryctrl->maximum,
				queryctrl->step, queryctrl->default_value);
		break;
	case V4L2_CTRL_TYPE_INTEGER64:
		printf("%31s (int64)  : min=%lld max=%lld step=%lld default=%lld",
				s.c_str(),
				queryctrl->minimum, queryctrl->maximum,
				queryctrl->step, queryctrl->default_value);
		break;
	case V4L2_CTRL_TYPE_STRING:
		printf("%31s (str)    : min=%lld max=%lld step=%lld",
				s.c_str(),
				queryctrl->minimum, queryctrl->maximum,
				queryctrl->step);
		break;
	case V4L2_CTRL_TYPE_BOOLEAN:
		printf("%31s (bool)   : default=%lld",
				s.c_str(), queryctrl->default_value);
		break;
	case V4L2_CTRL_TYPE_MENU:
		printf("%31s (menu)   : min=%lld max=%lld default=%lld",
				s.c_str(),
				queryctrl->minimum, queryctrl->maximum,
				queryctrl->default_value);
		break;
	case V4L2_CTRL_TYPE_INTEGER_MENU:
		printf("%31s (intmenu): min=%lld max=%lld default=%lld",
				s.c_str(),
				queryctrl->minimum, queryctrl->maximum,
				queryctrl->default_value);
		break;
	case V4L2_CTRL_TYPE_BUTTON:
		printf("%31s (button) :", s.c_str());
		break;
	case V4L2_CTRL_TYPE_BITMASK:
		printf("%31s (bitmask): max=0x%08llx default=0x%08llx",
				s.c_str(), queryctrl->maximum,
				queryctrl->default_value);
		break;
	case V4L2_CTRL_TYPE_U8:
		printf("%31s (u8)     : min=%lld max=%lld step=%lld default=%lld",
				s.c_str(),
				queryctrl->minimum, queryctrl->maximum,
				queryctrl->step, queryctrl->default_value);
		break;
	case V4L2_CTRL_TYPE_U16:
		printf("%31s (u16)    : min=%lld max=%lld step=%lld default=%lld",
				s.c_str(),
				queryctrl->minimum, queryctrl->maximum,
				queryctrl->step, queryctrl->default_value);
		break;
	case V4L2_CTRL_TYPE_U32:
		printf("%31s (u32)    : min=%lld max=%lld step=%lld default=%lld",
				s.c_str(),
				queryctrl->minimum, queryctrl->maximum,
				queryctrl->step, queryctrl->default_value);
		break;
	default:
		printf("%31s (unknown): type=%x", s.c_str(), queryctrl->type);
		break;
	}
	if (queryctrl->nr_of_dims == 0) {
		switch (queryctrl->type) {
		case V4L2_CTRL_TYPE_INTEGER:
		case V4L2_CTRL_TYPE_BOOLEAN:
		case V4L2_CTRL_TYPE_MENU:
		case V4L2_CTRL_TYPE_INTEGER_MENU:
			printf(" value=%d", ctrl->value);
			break;
		case V4L2_CTRL_TYPE_BITMASK:
			printf(" value=0x%08x", ctrl->value);
			break;
		case V4L2_CTRL_TYPE_INTEGER64:
			printf(" value=%lld", ctrl->value64);
			break;
		case V4L2_CTRL_TYPE_STRING:
			printf(" value='%s'", safename(ctrl->string).c_str());
			break;
		default:
			break;
		}
	}
	if (queryctrl->nr_of_dims) {
		printf(" ");
		for (i = 0; i < queryctrl->nr_of_dims; i++)
			printf("[%u]", queryctrl->dims[i]);
	}
	if (queryctrl->flags)
		printf(" flags=%s", ctrlflags2s(queryctrl->flags).c_str());
	printf("\n");
	if ((queryctrl->type == V4L2_CTRL_TYPE_MENU ||
	     queryctrl->type == V4L2_CTRL_TYPE_INTEGER_MENU) && show_menus) {
		for (i = queryctrl->minimum; i <= queryctrl->maximum; i++) {
			qmenu.index = i;
			if (test_ioctl(fd, VIDIOC_QUERYMENU, &qmenu))
				continue;
			if (queryctrl->type == V4L2_CTRL_TYPE_MENU)
				printf("\t\t\t\t%d: %s\n", i, qmenu.name);
			else
				printf("\t\t\t\t%d: %lld (0x%llx)\n", i, qmenu.value, qmenu.value);
		}
	}
}

static int print_control(int fd, struct v4l2_query_ext_ctrl &qctrl, int show_menus)
{
	struct v4l2_control ctrl;
	struct v4l2_ext_control ext_ctrl;
	struct v4l2_ext_controls ctrls;

	memset(&ctrl, 0, sizeof(ctrl));
	memset(&ext_ctrl, 0, sizeof(ext_ctrl));
	memset(&ctrls, 0, sizeof(ctrls));
	if (qctrl.flags & V4L2_CTRL_FLAG_DISABLED)
		return 1;
	if (qctrl.type == V4L2_CTRL_TYPE_CTRL_CLASS) {
		printf("\n%s\n\n", qctrl.name);
		return 1;
	}
	ext_ctrl.id = qctrl.id;
	if ((qctrl.flags & V4L2_CTRL_FLAG_WRITE_ONLY) ||
	    qctrl.type == V4L2_CTRL_TYPE_BUTTON) {
		print_qctrl(fd, &qctrl, &ext_ctrl, show_menus);
		return 1;
	}
	if (qctrl.type >= V4L2_CTRL_COMPOUND_TYPES) {
		print_qctrl(fd, &qctrl, NULL, show_menus);
		return 1;
	}
	ctrls.which = V4L2_CTRL_ID2WHICH(qctrl.id);
	ctrls.count = 1;
	ctrls.controls = &ext_ctrl;
	if (qctrl.type == V4L2_CTRL_TYPE_INTEGER64 ||
	    qctrl.type == V4L2_CTRL_TYPE_STRING ||
	    (V4L2_CTRL_ID2WHICH(qctrl.id) != V4L2_CTRL_CLASS_USER &&
	     qctrl.id < V4L2_CID_PRIVATE_BASE)) {
		if (qctrl.type == V4L2_CTRL_TYPE_STRING) {
		    ext_ctrl.size = qctrl.maximum + 1;
		    ext_ctrl.string = (char *)malloc(ext_ctrl.size);
		    ext_ctrl.string[0] = 0;
		}
		if (test_ioctl(fd, VIDIOC_G_EXT_CTRLS, &ctrls)) {
			printf("error %d getting ext_ctrl %s\n",
					errno, qctrl.name);
			return 0;
		}
	}
	else {
		ctrl.id = qctrl.id;
		if (test_ioctl(fd, VIDIOC_G_CTRL, &ctrl)) {
			printf("error %d getting ctrl %s\n",
					errno, qctrl.name);
			return 0;
		}
		ext_ctrl.value = ctrl.value;
	}
	print_qctrl(fd, &qctrl, &ext_ctrl, show_menus);
	if (qctrl.type == V4L2_CTRL_TYPE_STRING)
		free(ext_ctrl.string);
	return 1;
}

static int query_ext_ctrl_ioctl(int fd, struct v4l2_query_ext_ctrl &qctrl)
{
	struct v4l2_queryctrl qc;
	int rc;

	if (have_query_ext_ctrl) {
		rc = test_ioctl(fd, VIDIOC_QUERY_EXT_CTRL, &qctrl);
		if (errno != ENOTTY)
			return rc;
	}
	qc.id = qctrl.id;
	rc = test_ioctl(fd, VIDIOC_QUERYCTRL, &qc);
	if (rc == 0) {
		qctrl.type = qc.type;
		memcpy(qctrl.name, qc.name, sizeof(qctrl.name));
		qctrl.minimum = qc.minimum;
		if (qc.type == V4L2_CTRL_TYPE_BITMASK) {
			qctrl.maximum = (__u32)qc.maximum;
			qctrl.default_value = (__u32)qc.default_value;
		} else {
			qctrl.maximum = qc.maximum;
			qctrl.default_value = qc.default_value;
		}
		qctrl.step = qc.step;
		qctrl.flags = qc.flags;
		qctrl.elems = 1;
		qctrl.nr_of_dims = 0;
		memset(qctrl.dims, 0, sizeof(qctrl.dims));
		switch (qctrl.type) {
		case V4L2_CTRL_TYPE_INTEGER64:
			qctrl.elem_size = sizeof(__s64);
			break;
		case V4L2_CTRL_TYPE_STRING:
			qctrl.elem_size = qc.maximum + 1;
			break;
		default:
			qctrl.elem_size = sizeof(__s32);
			break;
		}
		memset(qctrl.reserved, 0, sizeof(qctrl.reserved));
	}
	qctrl.id = qc.id;
	return rc;
}

static void list_controls(int fd, int show_menus)
{
	const unsigned next_fl = V4L2_CTRL_FLAG_NEXT_CTRL | V4L2_CTRL_FLAG_NEXT_COMPOUND;
	struct v4l2_query_ext_ctrl qctrl;
	int id;

	memset(&qctrl, 0, sizeof(qctrl));
	qctrl.id = next_fl;
	while (query_ext_ctrl_ioctl(fd, qctrl) == 0) {
		print_control(fd, qctrl, show_menus);
		qctrl.id |= next_fl;
	}
	if (qctrl.id != next_fl)
		return;
	for (id = V4L2_CID_USER_BASE; id < V4L2_CID_LASTP1; id++) {
		qctrl.id = id;
		if (query_ext_ctrl_ioctl(fd, qctrl) == 0)
			print_control(fd, qctrl, show_menus);
	}
	for (qctrl.id = V4L2_CID_PRIVATE_BASE;
			query_ext_ctrl_ioctl(fd, qctrl) == 0; qctrl.id++) {
		print_control(fd, qctrl, show_menus);
	}
}

static void find_controls(int fd)
{
	const unsigned next_fl = V4L2_CTRL_FLAG_NEXT_CTRL | V4L2_CTRL_FLAG_NEXT_COMPOUND;
	struct v4l2_query_ext_ctrl qctrl;
	int id;

	memset(&qctrl, 0, sizeof(qctrl));
	qctrl.id = next_fl;
	while (query_ext_ctrl_ioctl(fd, qctrl) == 0) {
		if (qctrl.type != V4L2_CTRL_TYPE_CTRL_CLASS &&
		    !(qctrl.flags & V4L2_CTRL_FLAG_DISABLED)) {
			ctrl_str2q[name2var(qctrl.name)] = qctrl;
			ctrl_id2str[qctrl.id] = name2var(qctrl.name);
		}
		qctrl.id |= next_fl;
	}
	if (qctrl.id != next_fl)
		return;
	for (id = V4L2_CID_USER_BASE; id < V4L2_CID_LASTP1; id++) {
		qctrl.id = id;
		if (query_ext_ctrl_ioctl(fd, qctrl) == 0 &&
		    !(qctrl.flags & V4L2_CTRL_FLAG_DISABLED)) {
			ctrl_str2q[name2var(qctrl.name)] = qctrl;
			ctrl_id2str[qctrl.id] = name2var(qctrl.name);
		}
	}
	for (qctrl.id = V4L2_CID_PRIVATE_BASE;
			query_ext_ctrl_ioctl(fd, qctrl) == 0; qctrl.id++) {
		if (!(qctrl.flags & V4L2_CTRL_FLAG_DISABLED)) {
			ctrl_str2q[name2var(qctrl.name)] = qctrl;
			ctrl_id2str[qctrl.id] = name2var(qctrl.name);
		}
	}
}

int common_find_ctrl_id(const char *name)
{
	if (ctrl_str2q.find(name) == ctrl_str2q.end())
		return 0;
	return ctrl_str2q[name].id;
}

void common_process_controls(int fd)
{
	struct v4l2_query_ext_ctrl qc = {
		V4L2_CTRL_FLAG_NEXT_CTRL | V4L2_CTRL_FLAG_NEXT_COMPOUND
	};
	int rc;

	rc = test_ioctl(fd, VIDIOC_QUERY_EXT_CTRL, &qc);
	have_query_ext_ctrl = rc == 0;

	find_controls(fd);
	for (ctrl_get_list::iterator iter = get_ctrls.begin(); iter != get_ctrls.end(); ++iter) {
	    if (ctrl_str2q.find(*iter) == ctrl_str2q.end()) {
		fprintf(stderr, "unknown control '%s'\n", iter->c_str());
		exit(1);
	    }
	}
	for (ctrl_set_map::iterator iter = set_ctrls.begin(); iter != set_ctrls.end(); ++iter) {
	    if (ctrl_str2q.find(iter->first) == ctrl_str2q.end()) {
		fprintf(stderr, "unknown control '%s'\n", iter->first.c_str());
		exit(1);
	    }
	}
}

void common_control_event(const struct v4l2_event *ev)
{
	const struct v4l2_event_ctrl *ctrl;

	ctrl = &ev->u.ctrl;
	printf("ctrl: %s\n", ctrl_id2str[ev->id].c_str());
	if (ctrl->changes & V4L2_EVENT_CTRL_CH_VALUE) {
		if (ctrl->type == V4L2_CTRL_TYPE_INTEGER64)
			printf("\tvalue: %lld 0x%llx\n", ctrl->value64, ctrl->value64);
		else if (ctrl->type == V4L2_CTRL_TYPE_BITMASK)
			printf("\tvalue: %u 0x%08x\n", ctrl->value, ctrl->value);
		else
			printf("\tvalue: %d 0x%x\n", ctrl->value, ctrl->value);
	}
	if (ctrl->changes & V4L2_EVENT_CTRL_CH_FLAGS)
		printf("\tflags: %s\n", ctrlflags2s(ctrl->flags).c_str());
	if (ctrl->changes & V4L2_EVENT_CTRL_CH_RANGE) {
		if (ctrl->type == V4L2_CTRL_TYPE_BITMASK)
			printf("\trange: max=0x%08x default=0x%08x\n",
				ctrl->maximum, ctrl->default_value);
		else
			printf("\trange: min=%d max=%d step=%d default=%d\n",
				ctrl->minimum, ctrl->maximum, ctrl->step, ctrl->default_value);
	}
}

static bool parse_subset(char *optarg)
{
	struct ctrl_subset subset;
	std::string ctrl_name;
	unsigned idx = 0;
	char *p;

	memset(&subset, 0, sizeof(subset));
	while (*optarg) {
		p = strchr(optarg, ',');
		if (p)
			*p = 0;
		if (optarg[0] == 0) {
			fprintf(stderr, "empty string\n");
			return true;
		}
		if (idx == 0) {
			ctrl_name = optarg;
		} else {
			if (idx > V4L2_CTRL_MAX_DIMS * 2) {
				fprintf(stderr, "too many dimensions\n");
				return true;
			}
			if (idx & 1)
				subset.offset[idx / 2] = strtoul(optarg, 0, 0);
			else {
				subset.size[idx / 2 - 1] = strtoul(optarg, 0, 0);
				if (subset.size[idx / 2 - 1] == 0) {
					fprintf(stderr, "<size> cannot be 0\n");
					return true;
				}
			}
		}
		idx++;
		if (p == NULL)
			break;
		optarg = p + 1;
	}
	if (idx == 1) {
		fprintf(stderr, "no <offset>, <size> tuples given\n");
		return true;
	}
	if ((idx & 1) == 0) {
		fprintf(stderr, "<offset> without <size>\n");
		return true;
	}
	ctrl_subsets[ctrl_name] = subset;

	return false;
}

static bool parse_next_subopt(char **subs, char **value)
{
	static char *const subopts[] = {
	    NULL
	};
	int opt = getsubopt(subs, subopts, value);

	if (opt < 0 || *value)
		return false;
	fprintf(stderr, "No value given to suboption <%s>\n",
			subopts[opt]);
	return true;
}

void common_cmd(int ch, char *optarg)
{
	char *value, *subs;

	switch (ch) {
	case OptGetCtrl:
		subs = optarg;
		while (*subs != '\0') {
			if (parse_next_subopt(&subs, &value)) {
				common_usage();
				exit(1);
			}
			if (strchr(value, '=')) {
				common_usage();
				exit(1);
			}
			else {
				get_ctrls.push_back(value);
			}
		}
		break;
	case OptSetCtrl:
		subs = optarg;
		while (*subs != '\0') {
			if (parse_next_subopt(&subs, &value)) {
				common_usage();
				exit(1);
			}
			if (const char *equal = strchr(value, '=')) {
				set_ctrls[std::string(value, (equal - value))] = equal + 1;
			}
			else {
				fprintf(stderr, "control '%s' without '='\n", value);
				exit(1);
			}
		}
		break;
	case OptSubset:
		if (parse_subset(optarg)) {
			common_usage();
			exit(1);
		}
		break;
	case OptSetPriority:
		prio = (enum v4l2_priority)strtoul(optarg, 0L, 0);
		break;
	case OptListDevices:
		list_devices();
		break;
	}
}

static bool fill_subset(const struct v4l2_query_ext_ctrl &qc, ctrl_subset &subset)
{
	unsigned d;

	if (qc.nr_of_dims == 0)
		return false;

	for (d = 0; d < qc.nr_of_dims; d++) {
		subset.offset[d] = 0;
		subset.size[d] = qc.dims[d];
	}

	std::string s = name2var(qc.name);

	if (ctrl_subsets.find(s) != ctrl_subsets.end()) {
		unsigned ss_dims;

		subset = ctrl_subsets[s];
		for (ss_dims = 0; ss_dims < V4L2_CTRL_MAX_DIMS && subset.size[ss_dims]; ss_dims++) ;
		if (ss_dims != qc.nr_of_dims) {
			fprintf(stderr, "expected %d dimensions but --subset specified %d\n",
					qc.nr_of_dims, ss_dims);
			return true;
		}
		for (d = 0; d < qc.nr_of_dims; d++) {
			if (subset.offset[d] + subset.size[d] > qc.dims[d]) {
				fprintf(stderr, "the subset offset+size for dimension %d is out of range\n", d);
				return true;
			}
		}
	}
	return false;
}

static bool idx_in_subset(const struct v4l2_query_ext_ctrl &qc, const ctrl_subset &subset,
			  const unsigned *divide, unsigned idx)
{
	for (unsigned d = 0; d < qc.nr_of_dims; d++) {
		unsigned i = (idx / divide[d]) % qc.dims[d];

		if (i < subset.offset[d] || i >= subset.offset[d] + subset.size[d])
			return false;
	}
	return true;
}

void common_set(int fd)
{
	if (options[OptSetPriority]) {
		if (doioctl(fd, VIDIOC_S_PRIORITY, &prio) >= 0) {
			printf("Priority set: %d\n", prio);
		}
	}

	if (options[OptSetCtrl] && !set_ctrls.empty()) {
		struct v4l2_ext_controls ctrls;
		class2ctrls_map class2ctrls;
		bool use_ext_ctrls = false;

		memset(&ctrls, 0, sizeof(ctrls));
		for (ctrl_set_map::iterator iter = set_ctrls.begin();
				iter != set_ctrls.end(); ++iter) {
			struct v4l2_ext_control ctrl;
			struct v4l2_query_ext_ctrl &qc = ctrl_str2q[iter->first];

			memset(&ctrl, 0, sizeof(ctrl));
			ctrl.id = qc.id;
			if (qc.type == V4L2_CTRL_TYPE_INTEGER64)
				use_ext_ctrls = true;
			if (qc.flags & V4L2_CTRL_FLAG_HAS_PAYLOAD) {
				struct v4l2_ext_controls ctrls = { 0, 1 };
				unsigned divide[V4L2_CTRL_MAX_DIMS] = { 0 };
				ctrl_subset subset;
				long long v;
				unsigned d, i;

				use_ext_ctrls = true;
				ctrl.size = qc.elems * qc.elem_size;
				ctrl.ptr = malloc(ctrl.size);

				ctrls.controls = &ctrl;
				ioctl(fd, VIDIOC_G_EXT_CTRLS, &ctrls);

				if (fill_subset(qc, subset))
					return;

				divide[qc.nr_of_dims - 1] = 1;
				for (d = 0; d < qc.nr_of_dims - 1; d++) {
					divide[d] = qc.dims[d + 1];
					for (i = 0; i < d; i++)
						divide[i] *= divide[d];
				}


				switch (qc.type) {
				case V4L2_CTRL_TYPE_U8:
					v = strtoul(iter->second.c_str(), NULL, 0);
					for (i = 0; i < qc.elems; i++)
						if (idx_in_subset(qc, subset, divide, i))
							ctrl.p_u8[i] = v;
					break;
				case V4L2_CTRL_TYPE_U16:
					v = strtoul(iter->second.c_str(), NULL, 0);
					for (i = 0; i < qc.elems; i++)
						if (idx_in_subset(qc, subset, divide, i))
							ctrl.p_u16[i] = v;
					break;
				case V4L2_CTRL_TYPE_U32:
					v = strtoul(iter->second.c_str(), NULL, 0);
					for (i = 0; i < qc.elems; i++)
						if (idx_in_subset(qc, subset, divide, i))
							ctrl.p_u32[i] = v;
					break;
				case V4L2_CTRL_TYPE_STRING:
					strncpy(ctrl.string, iter->second.c_str(), qc.maximum);
					ctrl.string[qc.maximum] = 0;
					break;
				default:
					fprintf(stderr, "%s: unsupported payload type\n",
							qc.name);
					break;
				}
			} else {
				if (V4L2_CTRL_DRIVER_PRIV(ctrl.id))
					use_ext_ctrls = true;
				ctrl.value = strtol(iter->second.c_str(), NULL, 0);
			}
			class2ctrls[V4L2_CTRL_ID2WHICH(ctrl.id)].push_back(ctrl);
		}
		for (class2ctrls_map::iterator iter = class2ctrls.begin();
				iter != class2ctrls.end(); ++iter) {
			if (!use_ext_ctrls &&
			    (iter->first == V4L2_CTRL_CLASS_USER ||
			     iter->first == V4L2_CID_PRIVATE_BASE)) {
				for (unsigned i = 0; i < iter->second.size(); i++) {
					struct v4l2_control ctrl;

					ctrl.id = iter->second[i].id;
					ctrl.value = iter->second[i].value;
					if (doioctl(fd, VIDIOC_S_CTRL, &ctrl)) {
						fprintf(stderr, "%s: %s\n",
								ctrl_id2str[ctrl.id].c_str(),
								strerror(errno));
					}
				}
				continue;
			}
			if (iter->second.size()) {
				ctrls.which = iter->first;
				ctrls.count = iter->second.size();
				ctrls.controls = &iter->second[0];
				if (doioctl(fd, VIDIOC_S_EXT_CTRLS, &ctrls)) {
					if (ctrls.error_idx >= ctrls.count) {
						fprintf(stderr, "Error setting controls: %s\n",
								strerror(errno));
					}
					else {
						fprintf(stderr, "%s: %s\n",
								ctrl_id2str[iter->second[ctrls.error_idx].id].c_str(),
								strerror(errno));
					}
				}
			}
		}
	}
}

static void print_array(const struct v4l2_query_ext_ctrl &qc, void *p)
{
	ctrl_subset subset;
	unsigned divide[V4L2_CTRL_MAX_DIMS] = { 0 };
	unsigned from, to;
	unsigned d, i;

	if (fill_subset(qc, subset))
		return;

	divide[qc.nr_of_dims - 1] = 1;
	for (d = 0; d < qc.nr_of_dims - 1; d++) {
		divide[d] = qc.dims[d + 1];
		for (i = 0; i < d; i++)
			divide[i] *= divide[d];
	}

	from = subset.offset[qc.nr_of_dims - 1];
	to = subset.offset[qc.nr_of_dims - 1] + subset.size[qc.nr_of_dims - 1] - 1;

	for (unsigned idx = 0; idx < qc.elems; idx += qc.dims[qc.nr_of_dims - 1]) {
		for (d = 0; d < qc.nr_of_dims - 1; d++) {
			unsigned i = (idx / divide[d]) % qc.dims[d];

			if (i < subset.offset[d] || i >= subset.offset[d] + subset.size[d])
				break;
		}
		if (d < qc.nr_of_dims - 1)
			continue;

		printf("%s", qc.name);
		for (d = 0; d < qc.nr_of_dims - 1; d++)
			printf("[%u]", (idx / divide[d]) % qc.dims[d]);
		printf(": ");
		switch (qc.type) {
		case V4L2_CTRL_TYPE_U8:
			for (i = from; i <= to; i++) {
				printf("%4d", ((__u8 *)p)[idx + i]);
				if (i < to)
					printf(", ");
			}
			printf("\n");
			break;
		case V4L2_CTRL_TYPE_U16:
			for (i = from; i <= to; i++) {
				printf("%6d", ((__u16 *)p)[idx + i]);
				if (i < to)
					printf(", ");
			}
			printf("\n");
			break;
		case V4L2_CTRL_TYPE_U32:
			for (i = from; i <= to; i++) {
				printf("%6d", ((__u32 *)p)[idx + i]);
				if (i < to)
					printf(", ");
			}
			printf("\n");
			break;
		}
	}
}

void common_get(int fd)
{
	if (options[OptGetCtrl] && !get_ctrls.empty()) {
		struct v4l2_ext_controls ctrls;
		class2ctrls_map class2ctrls;
		bool use_ext_ctrls = false;

		memset(&ctrls, 0, sizeof(ctrls));
		for (ctrl_get_list::iterator iter = get_ctrls.begin();
				iter != get_ctrls.end(); ++iter) {
			struct v4l2_ext_control ctrl;
			struct v4l2_query_ext_ctrl &qc = ctrl_str2q[*iter];

			memset(&ctrl, 0, sizeof(ctrl));
			ctrl.id = qc.id;
			if (qc.type == V4L2_CTRL_TYPE_INTEGER64)
				use_ext_ctrls = true;
			if (qc.flags & V4L2_CTRL_FLAG_HAS_PAYLOAD) {
				use_ext_ctrls = true;
				ctrl.size = qc.elems * qc.elem_size;
				ctrl.ptr = calloc(1, ctrl.size);
			}
			if (V4L2_CTRL_DRIVER_PRIV(ctrl.id))
				use_ext_ctrls = true;
			class2ctrls[V4L2_CTRL_ID2WHICH(ctrl.id)].push_back(ctrl);
		}
		for (class2ctrls_map::iterator iter = class2ctrls.begin();
				iter != class2ctrls.end(); ++iter) {
			if (!use_ext_ctrls &&
			    (iter->first == V4L2_CTRL_CLASS_USER ||
			     iter->first == V4L2_CID_PRIVATE_BASE)) {
				for (unsigned i = 0; i < iter->second.size(); i++) {
					struct v4l2_control ctrl;

					ctrl.id = iter->second[i].id;
					doioctl(fd, VIDIOC_G_CTRL, &ctrl);
					printf("%s: %d\n", ctrl_id2str[ctrl.id].c_str(), ctrl.value);
				}
				continue;
			}
			if (iter->second.size()) {
				ctrls.which = iter->first;
				ctrls.count = iter->second.size();
				ctrls.controls = &iter->second[0];
				doioctl(fd, VIDIOC_G_EXT_CTRLS, &ctrls);
				for (unsigned i = 0; i < iter->second.size(); i++) {
					struct v4l2_ext_control ctrl = iter->second[i];
					std::string &name = ctrl_id2str[ctrl.id];
					struct v4l2_query_ext_ctrl &qc = ctrl_str2q[name];

					if (qc.flags & V4L2_CTRL_FLAG_HAS_PAYLOAD) {
						switch (qc.type) {
						case V4L2_CTRL_TYPE_U8:
						case V4L2_CTRL_TYPE_U16:
						case V4L2_CTRL_TYPE_U32:
							print_array(qc, ctrl.ptr);
							break;
						case V4L2_CTRL_TYPE_STRING:
							printf("%s: '%s'\n", name.c_str(),
								safename(ctrl.string).c_str());
							break;
						default:
							fprintf(stderr, "%s: unsupported payload type\n",
									qc.name);
							break;
						}
					} else
						printf("%s: %d\n", name.c_str(), ctrl.value);
				}
			}
		}
	}

	if (options[OptGetPriority]) {
		if (doioctl(fd, VIDIOC_G_PRIORITY, &prio) == 0)
			printf("Priority: %d\n", prio);
	}

	if (options[OptLogStatus]) {
		static char buf[40960];
		int len = -1;

		if (doioctl(fd, VIDIOC_LOG_STATUS, NULL) == 0) {
			printf("\nStatus Log:\n\n");
#ifdef HAVE_KLOGCTL
			len = klogctl(3, buf, sizeof(buf) - 1);
#endif
			if (len >= 0) {
				char *p = buf;
				char *q;

				buf[len] = 0;
				while ((q = strstr(p, "START STATUS"))) {
					p = q + 1;
				}
				if (p) {
					while (p > buf && *p != '<') p--;
					q = p;
					while ((q = strstr(q, "<6>"))) {
						memcpy(q, "   ", 3);
					}
					printf("%s", p);
				}
			}
		}
	}
}

void common_list(int fd)
{
	if (options[OptListCtrlsMenus]) {
		list_controls(fd, 1);
	}

	if (options[OptListCtrls]) {
		list_controls(fd, 0);
	}
}
