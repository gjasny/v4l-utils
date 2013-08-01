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
#include <config.h>

#ifdef HAVE_SYS_KLOG_H
#include <sys/klog.h>
#endif

#include <linux/videodev2.h>
#include <libv4l2.h>

#include <list>
#include <vector>
#include <map>
#include <string>
#include <algorithm>

#include "v4l2-ctl.h"

typedef std::map<unsigned, std::vector<struct v4l2_ext_control> > class2ctrls_map;

typedef std::map<std::string, struct v4l2_queryctrl> ctrl_qmap;
static ctrl_qmap ctrl_str2q;
typedef std::map<unsigned, std::string> ctrl_idmap;
static ctrl_idmap ctrl_id2str;

typedef std::list<std::string> ctrl_get_list;
static ctrl_get_list get_ctrls;

typedef std::map<std::string, std::string> ctrl_set_map;
static ctrl_set_map set_ctrls;

typedef std::vector<std::string> dev_vec;
typedef std::map<std::string, std::string> dev_map;

static enum v4l2_priority prio = V4L2_PRIORITY_UNSET;

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
	       "  -h, --help         display this help message\n"
	       "  --help-all         all options\n"
	       "  --help-io          input/output options\n"
	       "  --help-misc        miscellaneous options\n"
	       "  --help-overlay     overlay format options\n"
	       "  --help-selection   crop/selection options\n"
	       "  --help-stds        standards and other video timings options\n"
	       "  --help-streaming   streaming options\n"
	       "  --help-tuner       tuner/modulator options\n"
	       "  --help-vbi         VBI format options\n"
	       "  --help-vidcap      video capture format options\n"
	       "  --help-vidout      vidout output format options\n"
	       "  -k, --concise      be more concise if possible.\n"
	       "  -l, --list-ctrls   display all controls and their values [VIDIOC_QUERYCTRL]\n"
	       "  -L, --list-ctrls-menus\n"
	       "		     display all controls and their menus [VIDIOC_QUERYMENU]\n"
	       "  -w, --wrapper      use the libv4l2 wrapper library.\n"
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

static bool is_v4l_dev(const char *name)
{
	return !memcmp(name, "video", 5) ||
		!memcmp(name, "radio", 5) ||
		!memcmp(name, "vbi", 3) ||
		!memcmp(name, "v4l-subdev", 10);
}

static int calc_node_val(const char *s)
{
	int n = 0;

	s = strrchr(s, '/') + 1;
	if (!memcmp(s, "video", 5)) n = 0;
	else if (!memcmp(s, "radio", 5)) n = 0x100;
	else if (!memcmp(s, "vbi", 3)) n = 0x200;
	else if (!memcmp(s, "v4l-subdev", 10)) n = 0x300;
	n += atol(s + (n >= 0x200 ? 3 : 5));
	return n;
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
		doioctl(fd, VIDIOC_QUERYCAP, &vcap);
		close(fd);
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

static std::string name2var(unsigned char *name)
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
		{ 0, NULL }
	};
	return flags2s(flags, def);
}

static void print_qctrl(int fd, struct v4l2_queryctrl *queryctrl,
		struct v4l2_ext_control *ctrl, int show_menus)
{
	struct v4l2_querymenu qmenu;
	std::string s = name2var(queryctrl->name);
	int i;

	memset(&qmenu, 0, sizeof(qmenu));
	qmenu.id = queryctrl->id;
	switch (queryctrl->type) {
	case V4L2_CTRL_TYPE_INTEGER:
		printf("%31s (int)    : min=%d max=%d step=%d default=%d value=%d",
				s.c_str(),
				queryctrl->minimum, queryctrl->maximum,
				queryctrl->step, queryctrl->default_value,
				ctrl->value);
		break;
	case V4L2_CTRL_TYPE_INTEGER64:
		printf("%31s (int64)  : value=%lld", s.c_str(), ctrl->value64);
		break;
	case V4L2_CTRL_TYPE_STRING:
		printf("%31s (str)    : min=%d max=%d step=%d value='%s'",
				s.c_str(),
				queryctrl->minimum, queryctrl->maximum,
				queryctrl->step, safename(ctrl->string).c_str());
		break;
	case V4L2_CTRL_TYPE_BOOLEAN:
		printf("%31s (bool)   : default=%d value=%d",
				s.c_str(),
				queryctrl->default_value, ctrl->value);
		break;
	case V4L2_CTRL_TYPE_MENU:
		printf("%31s (menu)   : min=%d max=%d default=%d value=%d",
				s.c_str(),
				queryctrl->minimum, queryctrl->maximum,
				queryctrl->default_value, ctrl->value);
		break;
	case V4L2_CTRL_TYPE_INTEGER_MENU:
		printf("%31s (intmenu): min=%d max=%d default=%d value=%d",
				s.c_str(),
				queryctrl->minimum, queryctrl->maximum,
				queryctrl->default_value, ctrl->value);
		break;
	case V4L2_CTRL_TYPE_BUTTON:
		printf("%31s (button) :", s.c_str());
		break;
	case V4L2_CTRL_TYPE_BITMASK:
		printf("%31s (bitmask): max=0x%08x default=0x%08x value=0x%08x",
				s.c_str(), queryctrl->maximum,
				queryctrl->default_value, ctrl->value);
		break;
	default: break;
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

static int print_control(int fd, struct v4l2_queryctrl &qctrl, int show_menus)
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
	ctrls.ctrl_class = V4L2_CTRL_ID2CLASS(qctrl.id);
	ctrls.count = 1;
	ctrls.controls = &ext_ctrl;
	if (qctrl.type == V4L2_CTRL_TYPE_INTEGER64 ||
	    qctrl.type == V4L2_CTRL_TYPE_STRING ||
	    (V4L2_CTRL_ID2CLASS(qctrl.id) != V4L2_CTRL_CLASS_USER &&
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

static void list_controls(int fd, int show_menus)
{
	struct v4l2_queryctrl qctrl;
	int id;

	memset(&qctrl, 0, sizeof(qctrl));
	qctrl.id = V4L2_CTRL_FLAG_NEXT_CTRL;
	while (test_ioctl(fd, VIDIOC_QUERYCTRL, &qctrl) == 0) {
			print_control(fd, qctrl, show_menus);
		qctrl.id |= V4L2_CTRL_FLAG_NEXT_CTRL;
	}
	if (qctrl.id != V4L2_CTRL_FLAG_NEXT_CTRL)
		return;
	for (id = V4L2_CID_USER_BASE; id < V4L2_CID_LASTP1; id++) {
		qctrl.id = id;
		if (test_ioctl(fd, VIDIOC_QUERYCTRL, &qctrl) == 0)
			print_control(fd, qctrl, show_menus);
	}
	for (qctrl.id = V4L2_CID_PRIVATE_BASE;
			test_ioctl(fd, VIDIOC_QUERYCTRL, &qctrl) == 0; qctrl.id++) {
		print_control(fd, qctrl, show_menus);
	}
}

static void find_controls(int fd)
{
	struct v4l2_queryctrl qctrl;
	int id;

	memset(&qctrl, 0, sizeof(qctrl));
	qctrl.id = V4L2_CTRL_FLAG_NEXT_CTRL;
	while (test_ioctl(fd, VIDIOC_QUERYCTRL, &qctrl) == 0) {
		if (qctrl.type != V4L2_CTRL_TYPE_CTRL_CLASS &&
		    !(qctrl.flags & V4L2_CTRL_FLAG_DISABLED)) {
			ctrl_str2q[name2var(qctrl.name)] = qctrl;
			ctrl_id2str[qctrl.id] = name2var(qctrl.name);
		}
		qctrl.id |= V4L2_CTRL_FLAG_NEXT_CTRL;
	}
	if (qctrl.id != V4L2_CTRL_FLAG_NEXT_CTRL)
		return;
	for (id = V4L2_CID_USER_BASE; id < V4L2_CID_LASTP1; id++) {
		qctrl.id = id;
		if (test_ioctl(fd, VIDIOC_QUERYCTRL, &qctrl) == 0 &&
		    !(qctrl.flags & V4L2_CTRL_FLAG_DISABLED)) {
			ctrl_str2q[name2var(qctrl.name)] = qctrl;
			ctrl_id2str[qctrl.id] = name2var(qctrl.name);
		}
	}
	for (qctrl.id = V4L2_CID_PRIVATE_BASE;
			test_ioctl(fd, VIDIOC_QUERYCTRL, &qctrl) == 0; qctrl.id++) {
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
		else
			printf("\tvalue: %d 0x%x\n", ctrl->value, ctrl->value);
	}
	if (ctrl->changes & V4L2_EVENT_CTRL_CH_FLAGS)
		printf("\tflags: %s\n", ctrlflags2s(ctrl->flags).c_str());
	if (ctrl->changes & V4L2_EVENT_CTRL_CH_RANGE)
		printf("\trange: min=%d max=%d step=%d default=%d\n",
			ctrl->minimum, ctrl->maximum, ctrl->step, ctrl->default_value);
}

static bool parse_next_subopt(char **subs, char **value)
{
	static char *const subopts[] = {
	    NULL
	};
	int opt = getsubopt(subs, subopts, value);

	if (*value == NULL) {
		fprintf(stderr, "No value given to suboption <%s>\n",
				subopts[opt]);
		return true;
	}
	return false;
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
	case OptSetPriority:
		prio = (enum v4l2_priority)strtoul(optarg, 0L, 0);
		break;
	case OptListDevices:
		list_devices();
		break;
	}
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

			memset(&ctrl, 0, sizeof(ctrl));
			ctrl.id = ctrl_str2q[iter->first].id;
			if (ctrl_str2q[iter->first].type == V4L2_CTRL_TYPE_INTEGER64)
				use_ext_ctrls = true;
			if (ctrl_str2q[iter->first].type == V4L2_CTRL_TYPE_STRING) {
				unsigned len = iter->second.length();
				unsigned maxlen = ctrl_str2q[iter->first].maximum;

				use_ext_ctrls = true;
				ctrl.size = maxlen + 1;
				ctrl.string = (char *)malloc(ctrl.size);
				if (len > maxlen) {
					memcpy(ctrl.string, iter->second.c_str(), maxlen);
					ctrl.string[maxlen] = 0;
				}
				else {
					strcpy(ctrl.string, iter->second.c_str());
				}
			} else {
				if (V4L2_CTRL_DRIVER_PRIV(ctrl.id))
					use_ext_ctrls = true;
				ctrl.value = strtol(iter->second.c_str(), NULL, 0);
			}
			class2ctrls[V4L2_CTRL_ID2CLASS(ctrl.id)].push_back(ctrl);
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
				ctrls.ctrl_class = iter->first;
				ctrls.count = iter->second.size();
				ctrls.controls = &iter->second[0];
				if (doioctl(fd, VIDIOC_S_EXT_CTRLS, &ctrls)) {
					if (ctrls.error_idx >= ctrls.count) {
						fprintf(stderr, "Error setting MPEG controls: %s\n",
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

			memset(&ctrl, 0, sizeof(ctrl));
			ctrl.id = ctrl_str2q[*iter].id;
			if (ctrl_str2q[*iter].type == V4L2_CTRL_TYPE_INTEGER64)
				use_ext_ctrls = true;
			if (ctrl_str2q[*iter].type == V4L2_CTRL_TYPE_STRING) {
				use_ext_ctrls = true;
				ctrl.size = ctrl_str2q[*iter].maximum + 1;
				ctrl.string = (char *)malloc(ctrl.size);
				ctrl.string[0] = 0;
			}
			if (V4L2_CTRL_DRIVER_PRIV(ctrl.id))
				use_ext_ctrls = true;
			class2ctrls[V4L2_CTRL_ID2CLASS(ctrl.id)].push_back(ctrl);
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
				ctrls.ctrl_class = iter->first;
				ctrls.count = iter->second.size();
				ctrls.controls = &iter->second[0];
				doioctl(fd, VIDIOC_G_EXT_CTRLS, &ctrls);
				for (unsigned i = 0; i < iter->second.size(); i++) {
					struct v4l2_ext_control ctrl = iter->second[i];

					if (ctrl_str2q[ctrl_id2str[ctrl.id]].type == V4L2_CTRL_TYPE_STRING)
						printf("%s: '%s'\n", ctrl_id2str[ctrl.id].c_str(),
							       safename(ctrl.string).c_str());
					else
						printf("%s: %d\n", ctrl_id2str[ctrl.id].c_str(), ctrl.value);
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
