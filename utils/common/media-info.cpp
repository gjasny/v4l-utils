// SPDX-License-Identifier: LGPL-2.1-only
/*
 * Copyright 2018 Cisco Systems, Inc. and/or its affiliates. All rights reserved.
 */

#include <cstring>
#include <fstream>
#include <iostream>

#include <dirent.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <unistd.h>

#include <linux/media.h>

#include <media-info.h>
#ifndef __linux__
#include <linux/videodev2.h>
#include <fcntl.h>
#endif

static std::string num2s(unsigned num, bool is_hex = true)
{
	char buf[16];

	if (is_hex)
		sprintf(buf, "%08x", num);
	else
		sprintf(buf, "%u", num);
	return buf;
}

static constexpr struct {
	const char *devname;
	enum media_type type;
} media_types[] = {
	{ "video", MEDIA_TYPE_VIDEO },
	{ "vbi", MEDIA_TYPE_VBI },
	{ "radio", MEDIA_TYPE_RADIO },
	{ "swradio", MEDIA_TYPE_SDR },
	{ "v4l-subdev", MEDIA_TYPE_SUBDEV },
	{ "v4l-touch", MEDIA_TYPE_TOUCH },
	{ "media", MEDIA_TYPE_MEDIA },
	{ "frontend", MEDIA_TYPE_DVB_FRONTEND },
	{ "demux", MEDIA_TYPE_DVB_DEMUX },
	{ "dvr", MEDIA_TYPE_DVB_DVR },
	{ "net", MEDIA_TYPE_DVB_NET },
	{ "ca", MEDIA_TYPE_DTV_CA },
	{ nullptr, MEDIA_TYPE_UNKNOWN }
};

media_type mi_media_detect_type(const char *device)
{
	struct stat sb;

	if (stat(device, &sb) == -1)
		return MEDIA_TYPE_CANT_STAT;
#ifdef __linux__
	std::string uevent_path("/sys/dev/char/");

	uevent_path += num2s(major(sb.st_rdev), false) + ":" +
		num2s(minor(sb.st_rdev), false) + "/uevent";

	std::ifstream uevent_file(uevent_path.c_str());
	if (uevent_file.fail())
		return MEDIA_TYPE_UNKNOWN;

	std::string line;

	while (std::getline(uevent_file, line)) {
		if (line.compare(0, 8, "DEVNAME="))
			continue;

		line.erase(0, 8);
		if (!line.compare(0, 11, "dvb/adapter")) {
			line.erase(0, 11);
			unsigned len = 0;
			while (line[len] && line[len] != '/')
				len++;
			line.erase(0, len + 1);
		}
		for (size_t i = 0; media_types[i].devname; i++) {
			const char *devname = media_types[i].devname;
			size_t len = strlen(devname);

			if (!line.compare(0, len, devname) && isdigit(line[0+len])) {
				uevent_file.close();
				return media_types[i].type;
			}
		}
	}

	uevent_file.close();
#else // Not Linux
	int fd = open(device, O_RDONLY);
	if (fd >= 0) {
		struct v4l2_capability caps;
		int error = ioctl(fd, VIDIOC_QUERYCAP, &caps);
		close(fd);
		if (error == 0) {
			if (caps.device_caps & V4L2_CAP_VIDEO_CAPTURE) {
				return MEDIA_TYPE_VIDEO;
			} else if (caps.device_caps & V4L2_CAP_VBI_CAPTURE) {
				return MEDIA_TYPE_VBI;
			} else if (caps.device_caps & V4L2_CAP_RADIO) {
				return MEDIA_TYPE_RADIO;
			}
		}
	}
#endif
	return MEDIA_TYPE_UNKNOWN;
}

std::string mi_media_get_device(__u32 major, __u32 minor)
{
	char fmt[32];
	std::string uevent_path("/sys/dev/char/");

	sprintf(fmt, "%d:%d", major, minor);
	uevent_path += std::string(fmt) + "/uevent";

	std::ifstream uevent_file(uevent_path.c_str());
	if (uevent_file.fail())
		return "";

	std::string line;

	while (std::getline(uevent_file, line)) {
		if (line.compare(0, 8, "DEVNAME="))
			continue;
		uevent_file.close();
		return std::string("/dev/") + &line[8];
	}

	uevent_file.close();
	return "";
}

struct flag_def {
	unsigned flag;
	const char *str;
};

static std::string flags2s(unsigned val, const flag_def *def)
{
	std::string s;

	while (def->flag) {
		if (val & def->flag) {
			if (s.length()) s += ", ";
			s += def->str;
			val &= ~def->flag;
		}
		def++;
	}
	if (val) {
		if (s.length()) s += ", ";
		s += num2s(val);
	}
	return s;
}

int mi_get_dev_t_from_fd(int fd, dev_t *dev)
{
	struct stat sb;

	if (fstat(fd, &sb) == -1) {
		fprintf(stderr, "failed to stat file\n");
		return -1;
	}
	*dev = sb.st_rdev;
	return 0;
}

std::string mi_get_devpath_from_dev_t(dev_t dev)
{
	if (!dev)
		return "";

	std::string media_uevent_path("/sys/dev/char/");

	media_uevent_path += num2s(major(dev), false) + ":" +
		num2s(minor(dev), false) + "/uevent";

	FILE *uevent_fd = fopen(media_uevent_path.c_str(), "r");

	if (uevent_fd == nullptr) {
		fprintf(stderr, "failed to open %s\n", media_uevent_path.c_str());
		return "";
	}

	char *line = nullptr;
	size_t size = 0;
	std::string devpath;

	for (;;) {
		ssize_t bytes = getline(&line, &size, uevent_fd);

		if (bytes <= 0)
			break;
		line[bytes - 1] = 0;
		if (memcmp(line, "DEVNAME=", 8) || !line[8])
			continue;
		devpath = "/dev/";
		devpath += line + 8;
		break;
	}
	free(line);

	if (devpath.empty())
		fprintf(stderr, "failed to find DEVNAME in %s\n", media_uevent_path.c_str());
	return devpath;
}

int mi_get_media_fd(int fd, const char *bus_info)
{
	int media_fd = -1;
	dev_t dev;

	if (mi_get_dev_t_from_fd(fd, &dev) < 0)
		return -1;

	std::string media_path("/sys/dev/char/");

	media_path += num2s(major(dev), false) + ":" +
		num2s(minor(dev), false) + "/device";

	DIR *dp;
	struct dirent *ep;
	dp = opendir(media_path.c_str());
	if (dp == nullptr)
		return -1;
	media_path[0] = 0;
	while ((ep = readdir(dp))) {
		if (!memcmp(ep->d_name, "media", 5) && isdigit(ep->d_name[5])) {
			struct media_device_info mdinfo;
			std::string devname("/dev/");

			devname += ep->d_name;
			media_fd = open(devname.c_str(), O_RDWR);

			if (bus_info &&
			    (ioctl(media_fd, MEDIA_IOC_DEVICE_INFO, &mdinfo) ||
			     strcmp(mdinfo.bus_info, bus_info))) {
				close(media_fd);
				continue;
			}
			break;
		}
	}
	closedir(dp);
	return media_fd;
}

static constexpr flag_def entity_flags_def[] = {
	{ MEDIA_ENT_FL_DEFAULT, "default" },
	{ MEDIA_ENT_FL_CONNECTOR, "connector" },
	{ 0, nullptr }
};

std::string mi_entflags2s(__u32 flags)
{
	return flags2s(flags, entity_flags_def);
}

static constexpr flag_def interface_types_def[] = {
	{ MEDIA_INTF_T_DVB_FE, "DVB Front End" },
	{ MEDIA_INTF_T_DVB_DEMUX, "DVB Demuxer" },
	{ MEDIA_INTF_T_DVB_DVR, "DVB DVR" },
	{ MEDIA_INTF_T_DVB_CA, "DVB Conditional Access" },
	{ MEDIA_INTF_T_DVB_NET, "DVB Net" },

	{ MEDIA_INTF_T_V4L_VIDEO, "V4L Video" },
	{ MEDIA_INTF_T_V4L_VBI, "V4L VBI" },
	{ MEDIA_INTF_T_V4L_RADIO, "V4L Radio" },
	{ MEDIA_INTF_T_V4L_SUBDEV, "V4L Sub-Device" },
	{ MEDIA_INTF_T_V4L_SWRADIO, "V4L Software Defined Radio" },
	{ MEDIA_INTF_T_V4L_TOUCH, "V4L Touch" },

	{ MEDIA_INTF_T_ALSA_PCM_CAPTURE, "ALSA PCM Capture" },
	{ MEDIA_INTF_T_ALSA_PCM_PLAYBACK, "ALSA PCM Playback" },
	{ MEDIA_INTF_T_ALSA_CONTROL, "ALSA Control" },
	{ MEDIA_INTF_T_ALSA_COMPRESS, "ALSA Compress" },
	{ MEDIA_INTF_T_ALSA_RAWMIDI, "ALSA Raw MIDI" },
	{ MEDIA_INTF_T_ALSA_HWDEP, "ALSA HWDEP" },
	{ MEDIA_INTF_T_ALSA_SEQUENCER, "ALSA Sequencer" },
	{ MEDIA_INTF_T_ALSA_TIMER, "ALSA Timer" },
	{ 0, nullptr }
};

std::string mi_ifacetype2s(__u32 type)
{
	for (unsigned i = 0; interface_types_def[i].str; i++)
		if (type == interface_types_def[i].flag)
			return interface_types_def[i].str;
	return "FAIL: Unknown (" + num2s(type) + ")";
}

static constexpr flag_def entity_functions_def[] = {
	{ MEDIA_ENT_F_UNKNOWN, "FAIL: Uninitialized Function" },
	{ MEDIA_ENT_F_V4L2_SUBDEV_UNKNOWN, "FAIL: Unknown V4L2 Sub-Device" },
	{ MEDIA_ENT_T_DEVNODE_UNKNOWN, "FAIL: Unknown Device Node" },

	{ MEDIA_ENT_F_DTV_DEMOD, "Digital TV Demodulator" },
	{ MEDIA_ENT_F_TS_DEMUX, "Transport Stream Demuxer" },
	{ MEDIA_ENT_F_DTV_CA, "Digital TV Conditional Access" },
	{ MEDIA_ENT_F_DTV_NET_DECAP, "Digital TV Network ULE/MLE Desencapsulation" },
	{ MEDIA_ENT_F_IO_DTV, "Digital TV I/O" },
	{ MEDIA_ENT_F_IO_V4L, "V4L2 I/O" },
	{ MEDIA_ENT_F_IO_VBI, "VBI I/O" },
	{ MEDIA_ENT_F_IO_SWRADIO, "Software Radio I/O" },

	{ MEDIA_ENT_F_CAM_SENSOR, "Camera Sensor" },
	{ MEDIA_ENT_F_FLASH, "Flash Controller" },
	{ MEDIA_ENT_F_LENS, "Lens Controller" },
	{ MEDIA_ENT_F_ATV_DECODER, "Analog Video Decoder" },
	{ MEDIA_ENT_F_DV_DECODER, "Digital Video Decoder" },
	{ MEDIA_ENT_F_DV_ENCODER, "Digital Video Encoder" },
	{ MEDIA_ENT_F_TUNER, "Tuner" },
	{ MEDIA_ENT_F_IF_VID_DECODER, "IF-PLL Video Decoder" },
	{ MEDIA_ENT_F_IF_AUD_DECODER, "IF-PLL Audio Decoder" },
	{ MEDIA_ENT_F_AUDIO_CAPTURE, "Audio Capture" },
	{ MEDIA_ENT_F_AUDIO_PLAYBACK, "Audio Playback" },
	{ MEDIA_ENT_F_AUDIO_MIXER, "Audio Mixer" },
	{ MEDIA_ENT_F_PROC_VIDEO_COMPOSER, "Video Composer" },
	{ MEDIA_ENT_F_PROC_VIDEO_PIXEL_FORMATTER, "Video Pixel Formatter" },
	{ MEDIA_ENT_F_PROC_VIDEO_PIXEL_ENC_CONV, "Video Pixel Encoding Converter" },
	{ MEDIA_ENT_F_PROC_VIDEO_LUT, "Video Look-Up Table" },
	{ MEDIA_ENT_F_PROC_VIDEO_SCALER, "Video Scaler" },
	{ MEDIA_ENT_F_PROC_VIDEO_STATISTICS, "Video Statistics" },
	{ MEDIA_ENT_F_PROC_VIDEO_DECODER, "Video Decoder" },
	{ MEDIA_ENT_F_PROC_VIDEO_ENCODER, "Video Encoder" },
	{ MEDIA_ENT_F_VID_MUX, "Video Muxer" },
	{ MEDIA_ENT_F_VID_IF_BRIDGE, "Video Interface Bridge" },
	{ 0, nullptr }
};

std::string mi_entfunction2s(__u32 function, bool *is_invalid)
{
	std::string s;

	if (function != MEDIA_ENT_T_DEVNODE_UNKNOWN &&
	    (function & MEDIA_ENT_TYPE_MASK) == MEDIA_ENT_F_OLD_BASE &&
	    function > MEDIA_ENT_T_DEVNODE_DVB) {
		s = "Unknown device node (" + num2s(function) + ")";
		if (!is_invalid)
			return s;
		*is_invalid = true;
		return "FAIL: " + s;
	}
	if (function != MEDIA_ENT_F_V4L2_SUBDEV_UNKNOWN &&
	    (function & MEDIA_ENT_TYPE_MASK) == MEDIA_ENT_F_OLD_SUBDEV_BASE &&
	    function > MEDIA_ENT_F_TUNER) {
		s = "Unknown sub-device (" + num2s(function) + ")";
		if (!is_invalid)
			return s;
		*is_invalid = true;
		return "FAIL: " + s;
	}

	for (unsigned i = 0; entity_functions_def[i].str; i++) {
		if (function == entity_functions_def[i].flag) {
			bool fail = !memcmp(entity_functions_def[i].str, "FAIL: ", 6);

			if (is_invalid) {
				*is_invalid = fail;
				return entity_functions_def[i].str;
			}
			return fail ? entity_functions_def[i].str + 6 : entity_functions_def[i].str;
		}
	}
	if (is_invalid)
		return "WARNING: Unknown Function (" + num2s(function) + "), is v4l2-compliance out-of-date?";
	return "Unknown Function (" + num2s(function) + ")";
}

bool mi_func_requires_intf(__u32 function)
{
	switch (function) {
	case MEDIA_ENT_F_DTV_DEMOD:
	case MEDIA_ENT_F_TS_DEMUX:
	case MEDIA_ENT_F_DTV_CA:
	case MEDIA_ENT_F_IO_V4L:
	case MEDIA_ENT_F_IO_VBI:
	case MEDIA_ENT_F_IO_SWRADIO:
		return true;
	default:
		return false;
	}
}

static constexpr flag_def pad_flags_def[] = {
	{ MEDIA_PAD_FL_SINK, "Sink" },
	{ MEDIA_PAD_FL_SOURCE, "Source" },
	{ MEDIA_PAD_FL_MUST_CONNECT, "Must Connect" },
	{ 0, nullptr }
};

std::string mi_padflags2s(__u32 flags)
{
	return flags2s(flags, pad_flags_def);
}

static constexpr flag_def link_flags_def[] = {
	{ MEDIA_LNK_FL_ENABLED, "Enabled" },
	{ MEDIA_LNK_FL_IMMUTABLE, "Immutable" },
	{ MEDIA_LNK_FL_DYNAMIC, "Dynamic" },
	{ 0, nullptr }
};

std::string mi_linkflags2s(__u32 flags)
{
	std::string s = flags2s(flags & ~MEDIA_LNK_FL_LINK_TYPE, link_flags_def);

	if (!s.empty())
		s = ", " + s;
	switch (flags & MEDIA_LNK_FL_LINK_TYPE) {
	case MEDIA_LNK_FL_DATA_LINK:
		return "Data" + s;
	case MEDIA_LNK_FL_INTERFACE_LINK:
		return "Interface" + s;
	case MEDIA_LNK_FL_ANCILLARY_LINK:
		return "Ancillary" + s;
	default:
		return "Unknown (" + num2s(flags) + ")" + s;
	}
}

static __u32 read_topology(int media_fd, __u32 major, __u32 minor,
			   __u32 media_version, bool *is_invalid,
			   __u32 *function)
{
	media_v2_topology topology;
	unsigned i, j;

	memset(&topology, 0, sizeof(topology));
	if (ioctl(media_fd, MEDIA_IOC_G_TOPOLOGY, &topology))
		return 0;

	media_v2_entity v2_ents[topology.num_entities];
	media_v2_interface v2_ifaces[topology.num_interfaces];
	media_v2_pad v2_pads[topology.num_pads];
	media_v2_link v2_links[topology.num_links];

	topology.ptr_entities = (uintptr_t)v2_ents;
	topology.ptr_interfaces = (uintptr_t)v2_ifaces;
	topology.ptr_pads = (uintptr_t)v2_pads;
	topology.ptr_links = (uintptr_t)v2_links;
	if (ioctl(media_fd, MEDIA_IOC_G_TOPOLOGY, &topology))
		return 0;
	for (i = 0; i < topology.num_interfaces; i++)
		if (v2_ifaces[i].devnode.major == major &&
		    v2_ifaces[i].devnode.minor == minor)
			break;
	if (i == topology.num_interfaces) {
		fprintf(stderr, "FAIL: could not find device %d:%d in topology\n",
			major, minor);
		if (is_invalid)
			*is_invalid = true;
		if (function)
			*function = MEDIA_ENT_F_UNKNOWN;
		return MEDIA_ENT_F_UNKNOWN;
	}
	const media_v2_interface &iface = v2_ifaces[i];
	for (i = 0; i < topology.num_links; i++) {
		__u32 type = v2_links[i].flags & MEDIA_LNK_FL_LINK_TYPE;

		if (type != MEDIA_LNK_FL_INTERFACE_LINK)
			continue;
		if (v2_links[i].source_id == iface.id)
			break;
	}
	bool is_radio = iface.intf_type == MEDIA_INTF_T_V4L_RADIO;

	if (is_radio && i < topology.num_links) {
		if (is_invalid)
			*is_invalid = true;
		fprintf(stderr, "FAIL: unexpectedly found link for radio interface 0x%08x in topology\n",
			iface.id);
		return MEDIA_ENT_F_UNKNOWN;
	}
	if (!is_radio) {
		if (i == topology.num_links) {
			if (is_invalid)
				*is_invalid = true;
			fprintf(stderr, "FAIL: could not find link for interface 0x%08x in topology\n",
				iface.id);
			return MEDIA_ENT_F_UNKNOWN;
		}
		__u32 ent_id = v2_links[i].sink_id;
		for (i = 0; i < topology.num_entities; i++) {
			if (v2_ents[i].id == ent_id)
				break;
		}
		if (i == topology.num_entities) {
			if (is_invalid)
				*is_invalid = true;
			fprintf(stderr, "FAIL: could not find entity 0x%08x in topology\n",
				ent_id);
			return MEDIA_ENT_F_UNKNOWN;
		}
	}

	printf("Interface Info:\n");
	printf("\tID               : 0x%08x\n", iface.id);
	printf("\tType             : %s\n", mi_ifacetype2s(iface.intf_type).c_str());

	if (is_radio)
		return MEDIA_ENT_F_UNKNOWN;

	const media_v2_entity &ent = v2_ents[i];
	printf("Entity Info:\n");
	printf("\tID               : 0x%08x (%u)\n", ent.id, ent.id);
	printf("\tName             : %s\n", ent.name);
	printf("\tFunction         : %s\n", mi_entfunction2s(ent.function, is_invalid).c_str());
	if (MEDIA_V2_ENTITY_HAS_FLAGS(media_version) && ent.flags)
		printf("\tFlags            : %s\n", mi_entflags2s(ent.flags).c_str());

	// Yes, I know, lots of nested for-loops. If we get really complex
	// devices with such large topologies that this becomes too inefficient
	// then this needs to be changed and we'd need some maps to quickly
	// look up values.
	for (i = 0; i < topology.num_pads; i++) {
		const media_v2_pad &pad = v2_pads[i];

		if (pad.entity_id != ent.id)
			continue;
		printf("\tPad 0x%08x   : ", pad.id);
		if (MEDIA_V2_PAD_HAS_INDEX(media_version))
			printf("%u: ", pad.index);
		printf("%s\n", mi_padflags2s(pad.flags).c_str());
		for (j = 0; j < topology.num_links; j++) {
			const media_v2_link &link = v2_links[j];
			__u32 type = link.flags & MEDIA_LNK_FL_LINK_TYPE;
			__u32 remote_pad;
			__u32 remote_ent_id = 0;
			const media_v2_entity *remote_ent = nullptr;

			if (type != MEDIA_LNK_FL_DATA_LINK ||
			    (link.source_id != pad.id && link.sink_id != pad.id))
				continue;
			bool is_sink = link.sink_id == pad.id;
			remote_pad = is_sink ? link.source_id : link.sink_id;
			for (unsigned k = 0; k < topology.num_pads; k++)
				if (v2_pads[k].id == remote_pad) {
					remote_ent_id = v2_pads[k].entity_id;
					break;
				}
			if (!remote_ent_id) {
				if (is_invalid)
					*is_invalid = true;
				fprintf(stderr, "FAIL: could not find remote pad 0x%08x in topology\n",
					remote_pad);
				return ent.id;
			}
			for (unsigned k = 0; k < topology.num_entities; k++)
				if (v2_ents[k].id == remote_ent_id) {
					remote_ent = &v2_ents[k];
					break;
				}
			if (!remote_ent) {
				if (is_invalid)
					*is_invalid = true;
				fprintf(stderr, "FAIL: could not find remote entity 0x%08x in topology\n",
					remote_ent_id);
				return ent.id;
			}
			printf("\t  Link 0x%08x: %s remote pad 0x%x of entity '%s' (%s): %s\n",
			       link.id, is_sink ? "from" : "to", remote_pad,
			       remote_ent->name, mi_entfunction2s(remote_ent->function).c_str(),
			       mi_linkflags2s(link.flags).c_str());
			if (function && !*function)
				*function = remote_ent->function;
		}
	}
	return ent.id;
}

__u32 mi_media_info_for_fd(int media_fd, int fd, bool *is_invalid, __u32 *function)
{
	struct media_device_info mdinfo;
	struct stat sb;
	__u32 ent_id = 0;

	if (is_invalid)
		*is_invalid = false;

	if (function)
		*function = MEDIA_ENT_F_UNKNOWN;

	if (ioctl(media_fd, MEDIA_IOC_DEVICE_INFO, &mdinfo))
		return 0;

	struct media_entity_desc ent;
	bool found = false;

	printf("Media Driver Info:\n");
	printf("\tDriver name      : %s\n", mdinfo.driver);
	printf("\tModel            : %s\n", mdinfo.model);
	printf("\tSerial           : %s\n", mdinfo.serial);
	printf("\tBus info         : %s\n", mdinfo.bus_info);
	printf("\tMedia version    : %d.%d.%d\n",
	       mdinfo.media_version >> 16,
	       (mdinfo.media_version >> 8) & 0xff,
	       mdinfo.media_version & 0xff);
	printf("\tHardware revision: 0x%08x (%u)\n",
	       mdinfo.hw_revision, mdinfo.hw_revision);
	printf("\tDriver version   : %d.%d.%d\n",
	       mdinfo.driver_version >> 16,
	       (mdinfo.driver_version >> 8) & 0xff,
	       mdinfo.driver_version & 0xff);

	if (fd < 0)
		return 0;

	if (fstat(fd, &sb) == -1) {
		fprintf(stderr, "failed to stat file\n");
		std::exit(EXIT_FAILURE);
	}

	ent_id = read_topology(media_fd, major(sb.st_rdev), minor(sb.st_rdev),
			       mdinfo.media_version, is_invalid, function);
	if (ent_id)
		return ent_id;

	memset(&ent, 0, sizeof(ent));
	ent.id = MEDIA_ENT_ID_FLAG_NEXT;
	while (!ioctl(media_fd, MEDIA_IOC_ENUM_ENTITIES, &ent)) {
		if (ent.dev.major == major(sb.st_rdev) &&
		    ent.dev.minor == minor(sb.st_rdev)) {
			found = true;
			break;
		}
		ent.id |= MEDIA_ENT_ID_FLAG_NEXT;
	}
	if (!found)
		return 0;

	printf("Entity Info:\n");
	printf("\tID               : %u\n", ent.id);
	printf("\tName             : %s\n", ent.name);
	printf("\tType             : %s\n", mi_entfunction2s(ent.type).c_str());
	if (ent.flags)
		printf("\tFlags            : %s\n", mi_entflags2s(ent.flags).c_str());
	if (ent.flags & MEDIA_ENT_FL_DEFAULT) {
		printf("\tMajor            : %u\n", ent.dev.major);
		printf("\tMinor            : %u\n", ent.dev.minor);
	}

	struct media_links_enum links_enum;
	struct media_pad_desc pads[ent.pads];
	struct media_link_desc links[ent.links];

	memset(&links_enum, 0, sizeof(links_enum));
	links_enum.entity = ent.id;
	links_enum.pads = pads;
	links_enum.links = links;
	if (ioctl(media_fd, MEDIA_IOC_ENUM_LINKS, &links_enum))
		return ent.id;

	for (unsigned i = 0; i < ent.pads; i++)
		printf("\tPad              : %u: %s\n", pads[i].index,
		       mi_padflags2s(pads[i].flags).c_str());
	for (unsigned i = 0; i < ent.links; i++)
		printf("\tLink             : %u->%u: %s\n",
		       links[i].source.entity,
		       links[i].sink.entity,
		       mi_linkflags2s(links[i].flags).c_str());
	return ent.id;
}
