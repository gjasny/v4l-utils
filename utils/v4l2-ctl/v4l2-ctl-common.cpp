#include <algorithm>
#include <cctype>
#include <cstring>
#include <list>
#include <map>
#include <vector>

#include <dirent.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>

#include <linux/media.h>

#include "v4l2-ctl.h"

#ifdef HAVE_SYS_KLOG_H
#include <sys/klog.h>
#endif

struct ctrl_subset {
	unsigned offset[V4L2_CTRL_MAX_DIMS];
	unsigned size[V4L2_CTRL_MAX_DIMS];
};

using class2ctrls_map = std::map<unsigned int, std::vector<struct v4l2_ext_control> >;

using ctrl_qmap = std::map<std::string, struct v4l2_query_ext_ctrl>;
static ctrl_qmap ctrl_str2q;
using ctrl_idmap = std::map<unsigned int, std::string>;
static ctrl_idmap ctrl_id2str;

using ctrl_subset_map = std::map<std::string, ctrl_subset>;
static ctrl_subset_map ctrl_subsets;

using ctrl_get_list = std::list<std::string>;
static ctrl_get_list get_ctrls;

using ctrl_set_pair = std::pair<std::string, std::string>;
using ctrl_set_list = std::list<ctrl_set_pair>;
static ctrl_set_list set_ctrls;

using dev_vec = std::vector<std::string>;
using dev_map = std::map<std::string, std::string>;

static enum v4l2_priority prio = V4L2_PRIORITY_UNSET;

static bool have_query_ext_ctrl;

void common_usage()
{
	printf("\nGeneral/Common options:\n"
	       "  --all              display all information available\n"
	       "  -C, --get-ctrl <ctrl>[,<ctrl>...]\n"
	       "                     get the value of the controls [VIDIOC_G_EXT_CTRLS]\n"
	       "  -c, --set-ctrl <ctrl>=<val>[,<ctrl>=<val>...]\n"
	       "                     set the value of the controls [VIDIOC_S_EXT_CTRLS]\n"
	       "  -D, --info         show driver info [VIDIOC_QUERYCAP]\n"
	       "  -d, --device <dev> use device <dev> instead of /dev/video0\n"
	       "                     if <dev> starts with a digit, then /dev/video<dev> is used\n"
	       "                     Otherwise if -z was specified earlier, then <dev> is the entity name\n"
	       "                     or interface ID (if prefixed with 0x) as found in the topology of the\n"
	       "                     media device with the bus info string as specified by the -z option.\n"
	       "  -e, --out-device <dev> use device <dev> for output streams instead of the\n"
	       "                     default device as set with --device\n"
	       "                     if <dev> starts with a digit, then /dev/video<dev> is used\n"
	       "                     Otherwise if -z was specified earlier, then <dev> is the entity name\n"
	       "                     or interface ID (if prefixed with 0x) as found in the topology of the\n"
	       "                     media device with the bus info string as specified by the -z option.\n"
	       "  -E, --export-device <dev> use device <dev> for exporting DMA buffers\n"
	       "                     if <dev> starts with a digit, then /dev/video<dev> is used\n"
	       "                     Otherwise if -z was specified earlier, then <dev> is the entity name\n"
	       "                     or interface ID (if prefixed with 0x) as found in the topology of the\n"
	       "                     media device with the bus info string as specified by the -z option.\n"
	       "  -z, --media-bus-info <bus-info>\n"
	       "                     find the media device with the given bus info string. If set, then\n"
	       "                     -d, -e and -E options can use the entity name or interface ID to refer\n"
	       "                     to the device nodes.\n"
	       "  -h, --help         display this help message\n"
	       "  --help-all         all options\n"
	       "  --help-io          input/output options\n"
	       "  --help-meta        metadata format options\n"
	       "  --help-misc        miscellaneous options\n"
	       "  --help-overlay     overlay format options\n"
	       "  --help-sdr         SDR format options\n"
	       "  --help-selection   crop/selection options\n"
	       "  --help-stds        standards and other video timings options\n"
	       "  --help-streaming   streaming options\n"
	       "  --help-subdev      sub-device options\n"
	       "  --help-tuner       tuner/modulator options\n"
	       "  --help-vbi         VBI format options\n"
	       "  --help-vidcap      video capture format options\n"
	       "  --help-vidout      vidout output format options\n"
	       "  --help-edid        edid handling options\n"
	       "  -k, --concise      be more concise if possible.\n"
	       "  -l, --list-ctrls   display all controls and their values [VIDIOC_QUERYCTRL]\n"
	       "  -L, --list-ctrls-menus\n"
	       "		     display all controls and their menus [VIDIOC_QUERYMENU]\n"
	       "  -r, --subset <ctrl>[,<offset>,<size>]+\n"
	       "                     the subset of the N-dimensional array to get/set for control <ctrl>,\n"
	       "                     for every dimension an (<offset>, <size>) tuple is given.\n"
#ifndef NO_LIBV4L2
	       "  -w, --wrapper      use the libv4l2 wrapper library.\n"
#endif
	       "  --list-devices     list all v4l devices. If -z was given, then list just the\n"
	       "                     devices of the media device with the bus info string as\n"
	       "                     specified by the -z option.\n"
	       "  --log-status       log the board status in the kernel log [VIDIOC_LOG_STATUS]\n"
	       "  --get-priority     query the current access priority [VIDIOC_G_PRIORITY]\n"
	       "  --set-priority <prio>\n"
	       "                     set the new access priority [VIDIOC_S_PRIORITY]\n"
	       "                     <prio> is 1 (background), 2 (interactive) or 3 (record)\n"
	       "  --silent           only set the result code, do not print any messages\n"
	       "  --sleep <secs>     sleep <secs>, call QUERYCAP and close the file handle\n"
	       "  --verbose          turn on verbose ioctl status reporting\n"
	       "  --version          show version information\n"
	       );
}

static const char *prefixes[] = {
	"video",
	"radio",
	"vbi",
	"swradio",
	"v4l-subdev",
	"v4l-touch",
	"media",
	nullptr
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

	s = std::strrchr(s, '/') + 1;

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

static void list_media_devices(const std::string &media_bus_info)
{
	DIR *dp;
	struct dirent *ep;
	int media_fd = -1;
	std::map<dev_t, std::string> devices;

	dp = opendir("/dev");
	if (dp == nullptr) {
		perror ("Couldn't open the directory");
		return;
	}
	while ((ep = readdir(dp))) {
		std::string s("/dev/");

		s += ep->d_name;
		if (memcmp(ep->d_name, "media", 5)) {
			if (!is_v4l_dev(ep->d_name))
				continue;
			struct stat st;
			if (stat(s.c_str(), &st))
				continue;
			devices[st.st_rdev] = s;
			continue;
		}
		int fd = open(s.c_str(), O_RDWR);

		if (fd < 0)
			continue;
		struct media_device_info mdi;

		if (!ioctl(fd, MEDIA_IOC_DEVICE_INFO, &mdi) &&
		    media_bus_info == mdi.bus_info) {
			media_fd = fd;
			printf("%s\n", s.c_str());
		} else {
			close(fd);
		}
	}
	closedir(dp);
	if (media_fd < 0)
		return;

	media_v2_topology topology;
	memset(&topology, 0, sizeof(topology));
	if (ioctl(media_fd, MEDIA_IOC_G_TOPOLOGY, &topology)) {
		close(media_fd);
		return;
	}

	auto ifaces = new media_v2_interface[topology.num_interfaces];
	topology.ptr_interfaces = (uintptr_t)ifaces;

	if (!ioctl(media_fd, MEDIA_IOC_G_TOPOLOGY, &topology))
		for (unsigned i = 0; i < topology.num_interfaces; i++) {
			dev_t dev = makedev(ifaces[i].devnode.major,
					    ifaces[i].devnode.minor);

			if (devices.find(dev) != devices.end())
				printf("%s\n", devices[dev].c_str());
		}
	close(media_fd);
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
	if (dp == nullptr) {
		perror ("Couldn't open the directory");
		return;
	}
	while ((ep = readdir(dp)))
		if (is_v4l_dev(ep->d_name))
			files.push_back(std::string("/dev/") + ep->d_name);
	closedir(dp);

	/* Find device nodes which are links to other device nodes */
	for (auto iter = files.begin();
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
		iter = files.erase(iter);
	}

	std::sort(files.begin(), files.end(), sort_on_device_name);

	for (const auto &file : files) {
		int fd = open(file.c_str(), O_RDWR);
		std::string bus_info;
		std::string card;

		if (fd < 0)
			continue;
		int err = ioctl(fd, VIDIOC_QUERYCAP, &vcap);
		if (err) {
			struct media_device_info mdi;

			err = ioctl(fd, MEDIA_IOC_DEVICE_INFO, &mdi);
			if (!err) {
				if (mdi.bus_info[0])
					bus_info = mdi.bus_info;
				else
					bus_info = std::string("platform:") + mdi.driver;
				if (mdi.model[0])
					card = mdi.model;
				else
					card = mdi.driver;
			}
		} else {
			bus_info = reinterpret_cast<const char *>(vcap.bus_info);
			card = reinterpret_cast<const char *>(vcap.card);
		}
		close(fd);
		if (err)
			continue;
		if (cards[bus_info].empty())
			cards[bus_info] += card + " (" + bus_info + "):\n";
		cards[bus_info] += "\t" + file;
		if (!(links[file].empty()))
			cards[bus_info] += " <- " + links[file];
		cards[bus_info] += "\n";
	}
	for (const auto &card : cards) {
		printf("%s\n", card.second.c_str());
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
	return safename(reinterpret_cast<const unsigned char *>(name));
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

	if (qc.flags & V4L2_CTRL_FLAG_DYNAMIC_ARRAY)
		subset.size[0] = qc.elems;

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

static void print_array(const v4l2_query_ext_ctrl &qc, const v4l2_ext_control &ctrl,
			const ctrl_subset &subset)
{
	std::string &name = ctrl_id2str[qc.id];
	unsigned divide[V4L2_CTRL_MAX_DIMS] = { 0 };
	unsigned from, to;
	unsigned d, i;

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

		printf("%s", name.c_str());
		for (d = 0; d < qc.nr_of_dims - 1; d++)
			printf("[%u]", (idx / divide[d]) % qc.dims[d]);
		printf(": ");
		switch (qc.type) {
		case V4L2_CTRL_TYPE_U8:
			for (i = from; i <= to; i++) {
				printf("%4u", ctrl.p_u8[idx + i]);
				if (i < to)
					printf(", ");
			}
			printf("\n");
			break;
		case V4L2_CTRL_TYPE_U16:
			for (i = from; i <= to; i++) {
				printf("%6u", ctrl.p_u16[idx + i]);
				if (i < to)
					printf(", ");
			}
			printf("\n");
			break;
		case V4L2_CTRL_TYPE_U32:
			for (i = from; i <= to; i++) {
				printf("%10u", ctrl.p_u32[idx + i]);
				if (i < to)
					printf(", ");
			}
			printf("\n");
			break;
		}
	}
}

static void print_value(int fd, const v4l2_query_ext_ctrl &qc, const v4l2_ext_control &ctrl,
			bool show_payload, bool fill_in_subset)
{
	v4l2_querymenu qmenu;

	if (qc.nr_of_dims) {
		if (!show_payload)
			return;
		ctrl_subset subset;
		if (fill_in_subset) {
			if (fill_subset(qc, subset))
				return;
		} else {
			memset(&subset, 0, sizeof(subset));
			for (unsigned i = 0; i < qc.nr_of_dims; i++)
				subset.size[i] = qc.dims[i];
			if (qc.flags & V4L2_CTRL_FLAG_DYNAMIC_ARRAY)
				subset.size[0] = qc.elems;
		}
		print_array(qc, ctrl, subset);
		return;
	}

	if (qc.flags & V4L2_CTRL_FLAG_HAS_PAYLOAD) {
		switch (qc.type) {
		case V4L2_CTRL_TYPE_U8:
			printf("%u", *ctrl.p_u8);
			break;
		case V4L2_CTRL_TYPE_U16:
			printf("%u", *ctrl.p_u16);
			break;
		case V4L2_CTRL_TYPE_U32:
			printf("%u", *ctrl.p_u32);
			break;
		case V4L2_CTRL_TYPE_STRING:
			printf("'%s'", safename(ctrl.string).c_str());
			break;
		case V4L2_CTRL_TYPE_AREA:
			printf("%dx%d", ctrl.p_area->width, ctrl.p_area->height);
			break;
		default:
			printf("unsupported payload type");
			break;
		}
		return;
	}
	switch (qc.type) {
	case V4L2_CTRL_TYPE_INTEGER64:
		printf("%lld", ctrl.value64);
		break;
	case V4L2_CTRL_TYPE_MENU:
	case V4L2_CTRL_TYPE_INTEGER_MENU:
		memset(&qmenu, 0, sizeof(qmenu));
		qmenu.id = ctrl.id;
		qmenu.index = ctrl.value;
		if (test_ioctl(fd, VIDIOC_QUERYMENU, &qmenu))
			printf("%d", ctrl.value);
		else if (qc.type == V4L2_CTRL_TYPE_MENU)
			printf("%d (%s)", ctrl.value, qmenu.name);
		else
			printf("%d (%lld 0x%llx)", ctrl.value, qmenu.value, qmenu.value);
		break;
	default:
		printf("%d", ctrl.value);
		break;
	}
}

static void print_qctrl(int fd, const v4l2_query_ext_ctrl &qc,
			const v4l2_ext_control &ctrl, bool show_menus)
{
	std::string s = name2var(qc.name);
	unsigned i;

	switch (qc.type) {
	case V4L2_CTRL_TYPE_INTEGER:
		printf("%31s %#8.8x (int)    : min=%lld max=%lld step=%lld default=%lld",
				s.c_str(), qc.id, qc.minimum, qc.maximum,
				qc.step, qc.default_value);
		break;
	case V4L2_CTRL_TYPE_INTEGER64:
		printf("%31s %#8.8x (int64)  : min=%lld max=%lld step=%lld default=%lld",
				s.c_str(), qc.id, qc.minimum, qc.maximum,
				qc.step, qc.default_value);
		break;
	case V4L2_CTRL_TYPE_STRING:
		printf("%31s %#8.8x (str)    : min=%lld max=%lld step=%lld",
				s.c_str(), qc.id, qc.minimum, qc.maximum, qc.step);
		break;
	case V4L2_CTRL_TYPE_BOOLEAN:
		printf("%31s %#8.8x (bool)   : default=%lld",
				s.c_str(), qc.id, qc.default_value);
		break;
	case V4L2_CTRL_TYPE_MENU:
		printf("%31s %#8.8x (menu)   : min=%lld max=%lld default=%lld",
				s.c_str(), qc.id, qc.minimum, qc.maximum,
				qc.default_value);
		break;
	case V4L2_CTRL_TYPE_INTEGER_MENU:
		printf("%31s %#8.8x (intmenu): min=%lld max=%lld default=%lld",
				s.c_str(), qc.id, qc.minimum, qc.maximum,
				qc.default_value);
		break;
	case V4L2_CTRL_TYPE_BUTTON:
		printf("%31s %#8.8x (button) :", s.c_str(), qc.id);
		break;
	case V4L2_CTRL_TYPE_BITMASK:
		printf("%31s %#8.8x (bitmask): max=0x%08llx default=0x%08llx",
				s.c_str(), qc.id, qc.maximum, qc.default_value);
		break;
	case V4L2_CTRL_TYPE_U8:
		printf("%31s %#8.8x (u8)     : min=%lld max=%lld step=%lld default=%lld",
				s.c_str(), qc.id, qc.minimum, qc.maximum,
				qc.step, qc.default_value);
		break;
	case V4L2_CTRL_TYPE_U16:
		printf("%31s %#8.8x (u16)    : min=%lld max=%lld step=%lld default=%lld",
				s.c_str(), qc.id, qc.minimum, qc.maximum,
				qc.step, qc.default_value);
		break;
	case V4L2_CTRL_TYPE_U32:
		printf("%31s %#8.8x (u32)    : min=%lld max=%lld step=%lld default=%lld",
				s.c_str(), qc.id, qc.minimum, qc.maximum,
				qc.step, qc.default_value);
		break;
	case V4L2_CTRL_TYPE_AREA:
		printf("%31s %#8.8x (area)   :", s.c_str(), qc.id);
		break;
	case V4L2_CTRL_TYPE_HDR10_CLL_INFO:
		printf("%31s %#8.8x (hdr10-cll-info):", s.c_str(), qc.id);
		break;
	case V4L2_CTRL_TYPE_HDR10_MASTERING_DISPLAY:
		printf("%31s %#8.8x (hdr10-mastering-display):", s.c_str(), qc.id);
		break;
	case V4L2_CTRL_TYPE_H264_SPS:
		printf("%31s %#8.8x (h264-sps):", s.c_str(), qc.id);
		break;
	case V4L2_CTRL_TYPE_H264_PPS:
		printf("%31s %#8.8x (h264-pps):", s.c_str(), qc.id);
		break;
	case V4L2_CTRL_TYPE_H264_SCALING_MATRIX:
		printf("%31s %#8.8x (h264-scaling-matrix):", s.c_str(), qc.id);
		break;
	case V4L2_CTRL_TYPE_H264_SLICE_PARAMS:
		printf("%31s %#8.8x (h264-slice-params):", s.c_str(), qc.id);
		break;
	case V4L2_CTRL_TYPE_H264_DECODE_PARAMS:
		printf("%31s %#8.8x (h264-decode-params):", s.c_str(), qc.id);
		break;
	case V4L2_CTRL_TYPE_H264_PRED_WEIGHTS:
		printf("%31s %#8.8x (h264-pred-weights):", s.c_str(), qc.id);
		break;
	case V4L2_CTRL_TYPE_HEVC_SPS:
		printf("%31s %#8.8x (hevc-sps):", s.c_str(), qc.id);
		break;
	case V4L2_CTRL_TYPE_HEVC_PPS:
		printf("%31s %#8.8x (hevc-sps):", s.c_str(), qc.id);
		break;
	case V4L2_CTRL_TYPE_HEVC_SLICE_PARAMS:
		printf("%31s %#8.8x (hevc-slice-params):", s.c_str(), qc.id);
		break;
	case V4L2_CTRL_TYPE_HEVC_SCALING_MATRIX:
		printf("%31s %#8.8x (hevc-scaling-matrix):", s.c_str(), qc.id);
		break;
	case V4L2_CTRL_TYPE_HEVC_DECODE_PARAMS:
		printf("%31s %#8.8x (hevc-decode-params):", s.c_str(), qc.id);
		break;
	case V4L2_CTRL_TYPE_VP8_FRAME:
		printf("%31s %#8.8x (vp8-frame):", s.c_str(), qc.id);
		break;
	case V4L2_CTRL_TYPE_VP9_FRAME:
		printf("%31s %#8.8x (vp9-frame):", s.c_str(), qc.id);
		break;
	case V4L2_CTRL_TYPE_MPEG2_QUANTISATION:
		printf("%31s %#8.8x (mpeg2-quantisation):", s.c_str(), qc.id);
		break;
	case V4L2_CTRL_TYPE_MPEG2_SEQUENCE:
		printf("%31s %#8.8x (mpeg2-sequence):", s.c_str(), qc.id);
		break;
	case V4L2_CTRL_TYPE_MPEG2_PICTURE:
		printf("%31s %#8.8x (mpeg2-picture):", s.c_str(), qc.id);
		break;
	case V4L2_CTRL_TYPE_FWHT_PARAMS:
		printf("%31s %#8.8x (fwht-params):", s.c_str(), qc.id);
		break;
	default:
		printf("%31s %#8.8x (unknown): type=%x",
				s.c_str(), qc.id, qc.type);
		break;
	}
	if (qc.nr_of_dims == 0) {
		printf(" value=");
		print_value(fd, qc, ctrl, false, false);
	} else {
		if (qc.flags & V4L2_CTRL_FLAG_DYNAMIC_ARRAY)
			printf(" elems=%u", qc.elems);
		printf(" dims=");
		for (i = 0; i < qc.nr_of_dims; i++)
			printf("[%u]", qc.dims[i]);
	}
	if (qc.flags)
		printf(" flags=%s", ctrlflags2s(qc.flags).c_str());
	printf("\n");
	if ((qc.type == V4L2_CTRL_TYPE_MENU ||
	     qc.type == V4L2_CTRL_TYPE_INTEGER_MENU) && show_menus) {
		v4l2_querymenu qmenu;

		memset(&qmenu, 0, sizeof(qmenu));
		qmenu.id = ctrl.id;
		for (i = qc.minimum; i <= qc.maximum; i++) {
			qmenu.index = i;
			if (test_ioctl(fd, VIDIOC_QUERYMENU, &qmenu))
				continue;
			if (qc.type == V4L2_CTRL_TYPE_MENU)
				printf("\t\t\t\t%d: %s\n", i, qmenu.name);
			else
				printf("\t\t\t\t%d: %lld (0x%llx)\n", i, qmenu.value, qmenu.value);
		}
	}
}

static int print_control(int fd, struct v4l2_query_ext_ctrl &qctrl, bool show_menus)
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
		print_qctrl(fd, qctrl, ext_ctrl, show_menus);
		return 1;
	}
	ctrls.which = V4L2_CTRL_ID2WHICH(qctrl.id);
	ctrls.count = 1;
	ctrls.controls = &ext_ctrl;
	if (qctrl.type == V4L2_CTRL_TYPE_INTEGER64 ||
	    qctrl.type == V4L2_CTRL_TYPE_STRING ||
	    qctrl.nr_of_dims ||
	    qctrl.type >= V4L2_CTRL_COMPOUND_TYPES ||
	    (V4L2_CTRL_ID2WHICH(qctrl.id) != V4L2_CTRL_CLASS_USER &&
	     qctrl.id < V4L2_CID_PRIVATE_BASE)) {
		if (qctrl.flags & V4L2_CTRL_FLAG_HAS_PAYLOAD) {
			ext_ctrl.size = qctrl.elems * qctrl.elem_size;
			ext_ctrl.ptr = calloc(1, ext_ctrl.size);
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
	print_qctrl(fd, qctrl, ext_ctrl, show_menus);
	if (qctrl.flags & V4L2_CTRL_FLAG_HAS_PAYLOAD)
		free(ext_ctrl.ptr);
	return 1;
}

static int query_ext_ctrl_ioctl(int fd, struct v4l2_query_ext_ctrl &qctrl)
{
	struct v4l2_queryctrl qc;
	int rc;

	if (have_query_ext_ctrl) {
		rc = test_ioctl(fd, VIDIOC_QUERY_EXT_CTRL, &qctrl);
		if (rc != ENOTTY)
			return rc;
	}
	qc.id = qctrl.id;
	rc = test_ioctl(fd, VIDIOC_QUERYCTRL, &qc);
	if (rc == 0) {
		qctrl.type = qc.type;
		memcpy(qctrl.name, qc.name, sizeof(qctrl.name));
		qctrl.minimum = qc.minimum;
		if (qc.type == V4L2_CTRL_TYPE_BITMASK) {
			qctrl.maximum = static_cast<__u32>(qc.maximum);
			qctrl.default_value = static_cast<__u32>(qc.default_value);
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
			qctrl.flags |= V4L2_CTRL_FLAG_HAS_PAYLOAD;
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

static void list_controls(int fd, bool show_menus)
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

static void find_controls(cv4l_fd &_fd)
{
	const unsigned next_fl = V4L2_CTRL_FLAG_NEXT_CTRL | V4L2_CTRL_FLAG_NEXT_COMPOUND;
	struct v4l2_query_ext_ctrl qctrl;
	int fd = _fd.g_fd();
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

void common_process_controls(cv4l_fd &fd)
{
	struct v4l2_query_ext_ctrl qc = {
		V4L2_CTRL_FLAG_NEXT_CTRL | V4L2_CTRL_FLAG_NEXT_COMPOUND
	};
	int rc;

	rc = test_ioctl(fd.g_fd(), VIDIOC_QUERY_EXT_CTRL, &qc);
	have_query_ext_ctrl = rc == 0;

	find_controls(fd);
	for (const auto &get_ctrl : get_ctrls) {
		std::string s = get_ctrl;
		if (isdigit(s[0])) {
			__u32 id = strtoul(s.c_str(), nullptr, 0);
			if (ctrl_id2str.find(id) != ctrl_id2str.end())
				s = ctrl_id2str[id];
		}
		if (ctrl_str2q.find(s) == ctrl_str2q.end()) {
			fprintf(stderr, "unknown control '%s'\n", s.c_str());
			std::exit(EXIT_FAILURE);
		}
	}
	for (const auto &set_ctrl : set_ctrls) {
		std::string s = set_ctrl.first;
		if (isdigit(s[0])) {
			__u32 id = strtoul(s.c_str(), nullptr, 0);
			if (ctrl_id2str.find(id) != ctrl_id2str.end())
				s = ctrl_id2str[id];
		}
		if (ctrl_str2q.find(s) == ctrl_str2q.end()) {
			fprintf(stderr, "unknown control '%s'\n", s.c_str());
			std::exit(EXIT_FAILURE);
		}
	}
}

void common_control_event(int fd, const struct v4l2_event *ev)
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
	if (ctrl->changes & V4L2_EVENT_CTRL_CH_DIMENSIONS) {
		v4l2_query_ext_ctrl qctrl = {};
	
		qctrl.id = ev->id;
		if (!query_ext_ctrl_ioctl(fd, qctrl)) {
			ctrl_str2q[name2var(qctrl.name)] = qctrl;
			printf("\tdimensions: ");
			for (unsigned i = 0; i < qctrl.nr_of_dims; i++)
				printf("[%u]", qctrl.dims[i]);
			printf("\n");
		}
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
		p = std::strchr(optarg, ',');
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
				subset.offset[idx / 2] = strtoul(optarg, nullptr, 0);
			else {
				subset.size[idx / 2 - 1] = strtoul(optarg, nullptr, 0);
				if (subset.size[idx / 2 - 1] == 0) {
					fprintf(stderr, "<size> cannot be 0\n");
					return true;
				}
			}
		}
		idx++;
		if (p == nullptr)
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
	    nullptr
	};
	int opt = v4l_getsubopt(subs, subopts, value);

	if (opt < 0 || *value)
		return false;
	fprintf(stderr, "Missing suboption value\n");
	return true;
}

void common_cmd(const std::string &media_bus_info, int ch, char *optarg)
{
	char *value, *subs;

	switch (ch) {
	case OptGetCtrl:
		subs = optarg;
		while (*subs != '\0') {
			if (parse_next_subopt(&subs, &value)) {
				common_usage();
				std::exit(EXIT_FAILURE);
			}
			if (std::strchr(value, '=')) {
				common_usage();
				std::exit(EXIT_FAILURE);
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
				std::exit(EXIT_FAILURE);
			}
			if (const char *equal = std::strchr(value, '=')) {
				set_ctrls.emplace_back(std::string(value, (equal - value)),
						       std::string(equal + 1));
			}
			else {
				fprintf(stderr, "control '%s' without '='\n", value);
				std::exit(EXIT_FAILURE);
			}
		}
		break;
	case OptSubset:
		if (parse_subset(optarg)) {
			common_usage();
			std::exit(EXIT_FAILURE);
		}
		break;
	case OptSetPriority:
		prio = static_cast<enum v4l2_priority>(strtoul(optarg, nullptr, 0));
		break;
	case OptListDevices:
		if (media_bus_info.empty())
			list_devices();
		else
			list_media_devices(media_bus_info);
		break;
	}
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

void common_set(cv4l_fd &_fd)
{
	int fd = _fd.g_fd();

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
		for (const auto &set_ctrl : set_ctrls) {
			std::string s = set_ctrl.first;
			if (isdigit(s[0])) {
				__u32 id = strtoul(s.c_str(), nullptr, 0);
				s = ctrl_id2str[id];
			}
			struct v4l2_ext_control ctrl;
			struct v4l2_query_ext_ctrl &qc = ctrl_str2q[s];

			memset(&ctrl, 0, sizeof(ctrl));
			ctrl.id = qc.id;
			if (qc.type == V4L2_CTRL_TYPE_INTEGER64 ||
			    qc.flags & V4L2_CTRL_FLAG_UPDATE)
				use_ext_ctrls = true;
			if (qc.flags & V4L2_CTRL_FLAG_HAS_PAYLOAD) {
				struct v4l2_ext_controls ctrls = { { 0 }, 1 };
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

				if (qc.nr_of_dims) {
					divide[qc.nr_of_dims - 1] = 1;
					for (d = 0; d < qc.nr_of_dims - 1; d++) {
						divide[d] = qc.dims[d + 1];
						for (i = 0; i < d; i++)
							divide[i] *= divide[d];
					}
				}


				switch (qc.type) {
				case V4L2_CTRL_TYPE_U8:
					v = strtoul(set_ctrl.second.c_str(), nullptr, 0);
					for (i = 0; i < qc.elems; i++)
						if (idx_in_subset(qc, subset, divide, i))
							ctrl.p_u8[i] = v;
					break;
				case V4L2_CTRL_TYPE_U16:
					v = strtoul(set_ctrl.second.c_str(), nullptr, 0);
					for (i = 0; i < qc.elems; i++)
						if (idx_in_subset(qc, subset, divide, i))
							ctrl.p_u16[i] = v;
					break;
				case V4L2_CTRL_TYPE_U32:
					v = strtoul(set_ctrl.second.c_str(), nullptr, 0);
					for (i = 0; i < qc.elems; i++)
						if (idx_in_subset(qc, subset, divide, i))
							ctrl.p_u32[i] = v;
					break;
				case V4L2_CTRL_TYPE_STRING:
					strncpy(ctrl.string, set_ctrl.second.c_str(), qc.maximum);
					ctrl.string[qc.maximum] = 0;
					break;
				case V4L2_CTRL_TYPE_AREA:
					sscanf(set_ctrl.second.c_str(), "%ux%u",
					       &ctrl.p_area->width, &ctrl.p_area->height);
					break;
				default:
					fprintf(stderr, "%s: unsupported payload type\n",
							qc.name);
					break;
				}
			} else {
				if (V4L2_CTRL_DRIVER_PRIV(ctrl.id))
					use_ext_ctrls = true;
				if (qc.type == V4L2_CTRL_TYPE_INTEGER64)
					ctrl.value64 = strtoll(set_ctrl.second.c_str(), nullptr, 0);
				else
					ctrl.value = strtol(set_ctrl.second.c_str(), nullptr, 0);
			}
			class2ctrls[V4L2_CTRL_ID2WHICH(ctrl.id)].push_back(ctrl);
		}
		for (auto &class2ctrl : class2ctrls) {
			if (!use_ext_ctrls &&
			    (class2ctrl.first == V4L2_CTRL_CLASS_USER ||
			     class2ctrl.first == V4L2_CID_PRIVATE_BASE)) {
				for (const auto &i : class2ctrl.second) {
					struct v4l2_control ctrl;

					ctrl.id = i.id;
					ctrl.value = i.value;
					if (doioctl(fd, VIDIOC_S_CTRL, &ctrl)) {
						fprintf(stderr, "%s: %s\n",
								ctrl_id2str[ctrl.id].c_str(),
								strerror(errno));
					}
				}
				continue;
			}
			if (!class2ctrl.second.empty()) {
				ctrls.which = class2ctrl.first;
				ctrls.count = class2ctrl.second.size();
				ctrls.controls = &class2ctrl.second[0];
				if (doioctl(fd, VIDIOC_S_EXT_CTRLS, &ctrls)) {
					if (ctrls.error_idx >= ctrls.count) {
						fprintf(stderr, "Error setting controls: %s\n",
								strerror(errno));
					}
					else {
						fprintf(stderr, "%s: %s\n",
								ctrl_id2str[class2ctrl.second[ctrls.error_idx].id].c_str(),
								strerror(errno));
					}
				}
			}
		}
	}
}

void common_get(cv4l_fd &_fd)
{
	int fd = _fd.g_fd();

	if (options[OptGetCtrl] && !get_ctrls.empty()) {
		struct v4l2_ext_controls ctrls;
		class2ctrls_map class2ctrls;
		bool use_ext_ctrls = false;

		memset(&ctrls, 0, sizeof(ctrls));
		for (const auto &get_ctrl : get_ctrls) {
			std::string s = get_ctrl;
			if (isdigit(s[0])) {
				__u32 id = strtoul(s.c_str(), nullptr, 0);
				s = ctrl_id2str[id];
			}
			struct v4l2_ext_control ctrl;
			struct v4l2_query_ext_ctrl &qc = ctrl_str2q[s];

			memset(&ctrl, 0, sizeof(ctrl));
			ctrl.id = qc.id;
			if (qc.type == V4L2_CTRL_TYPE_INTEGER64 ||
			    qc.flags & V4L2_CTRL_FLAG_UPDATE)
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
		for (auto &class2ctrl : class2ctrls) {
			if (!use_ext_ctrls &&
			    (class2ctrl.first == V4L2_CTRL_CLASS_USER ||
			     class2ctrl.first == V4L2_CID_PRIVATE_BASE)) {
				for (const auto &i : class2ctrl.second) {
					struct v4l2_control ctrl;

					ctrl.id = i.id;
					doioctl(fd, VIDIOC_G_CTRL, &ctrl);
					printf("%s: %d\n", ctrl_id2str[ctrl.id].c_str(), ctrl.value);
				}
				continue;
			}
			if (!class2ctrl.second.empty()) {
				ctrls.which = class2ctrl.first;
				ctrls.count = class2ctrl.second.size();
				ctrls.controls = &class2ctrl.second[0];
				doioctl(fd, VIDIOC_G_EXT_CTRLS, &ctrls);
				for (auto ctrl : class2ctrl.second) {
					std::string &name = ctrl_id2str[ctrl.id];
					struct v4l2_query_ext_ctrl &qc = ctrl_str2q[name];

					if (qc.nr_of_dims) {
						print_value(fd, qc, ctrl, true, true);
						continue;
					}

					printf("%s: ", name.c_str());
					print_value(fd, qc, ctrl, true, false);
					printf("\n");
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

		if (doioctl(fd, VIDIOC_LOG_STATUS, nullptr) == 0) {
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

void common_list(cv4l_fd &fd)
{
	if (options[OptListCtrls] || options[OptListCtrlsMenus]) {
		list_controls(fd.g_fd(), options[OptListCtrlsMenus]);
	}
}
