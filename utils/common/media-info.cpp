/*
 * Copyright 2018 Cisco Systems, Inc. and/or its affiliates. All rights reserved.
 *
 * This program is free software; you may redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/sysmacros.h>
#include <dirent.h>

#include <linux/media.h>

#include <fstream>

static std::string num2s(unsigned num, bool is_hex = true)
{
	char buf[16];

	if (is_hex)
		sprintf(buf, "%08x", num);
	else
		sprintf(buf, "%u", num);
	return buf;
}

typedef struct {
	unsigned flag;
	const char *str;
} flag_def;

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

int mi_get_media_fd(int fd)
{
	struct stat sb;
	int media_fd = -1;

	if (fstat(fd, &sb) == -1) {
		fprintf(stderr, "failed to stat file\n");
		return -1;
	}

	std::string media_path("/sys/dev/char/");

	media_path += num2s(major(sb.st_rdev), false) + ":" +
		num2s(minor(sb.st_rdev), false) + "/device";

	DIR *dp;
	struct dirent *ep;
	dp = opendir(media_path.c_str());
	if (dp == NULL)
		return -1;
	media_path[0] = 0;
	while ((ep = readdir(dp))) {
		if (!memcmp(ep->d_name, "media", 5) && isdigit(ep->d_name[5])) {
			std::string devname("/dev/");

			devname += ep->d_name;
			media_fd = open(devname.c_str(), O_RDWR);
			break;
		}
	}
	closedir(dp);
	return media_fd;
}

static const flag_def entity_flags_def[] = {
	{ MEDIA_ENT_FL_DEFAULT, "default" },
	{ MEDIA_ENT_FL_CONNECTOR, "connector" },
	{ 0, NULL }
};

static std::string entflags2s(__u32 flags)
{
	return flags2s(flags, entity_flags_def);
}

static const flag_def interface_types_def[] = {
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
	{ 0, NULL }
};

static std::string ifacetype2s(__u32 type)
{
	for (unsigned i = 0; interface_types_def[i].str; i++)
		if (type == interface_types_def[i].flag)
			return interface_types_def[i].str;
	return "Unknown (" + num2s(type) + ")";
}

static const flag_def entity_types_def[] = {
	{ MEDIA_ENT_F_UNKNOWN, "Unknown" },
	{ MEDIA_ENT_F_DTV_DEMOD, "Digital TV Demodulator" },
	{ MEDIA_ENT_F_TS_DEMUX, "Transport Stream Demuxer" },
	{ MEDIA_ENT_F_DTV_CA, "Digital TV Conditional Access" },
	{ MEDIA_ENT_F_DTV_NET_DECAP, "Digital TV Network ULE/MLE Desencapsulation" },
	{ MEDIA_ENT_F_IO_DTV, "Digital TV I/O" },
	{ MEDIA_ENT_F_IO_VBI, "VBI I/O" },
	{ MEDIA_ENT_F_IO_SWRADIO, "Software Radio I/O" },
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
	{ MEDIA_ENT_F_VID_MUX, "Video Muxer" },
	{ MEDIA_ENT_F_VID_IF_BRIDGE, "Video Interface Bridge" },

	{ MEDIA_ENT_F_IO_V4L, "V4L2 I/O" },
	{ MEDIA_ENT_F_CAM_SENSOR, "Camera Sensor" },
	{ MEDIA_ENT_F_FLASH, "Flash Controller" },
	{ MEDIA_ENT_F_LENS, "Lens Controller" },
	{ MEDIA_ENT_F_ATV_DECODER, "Analog Video Decoder" },
	{ MEDIA_ENT_F_TUNER, "Tuner" },
	{ MEDIA_ENT_F_V4L2_SUBDEV_UNKNOWN, "Unknown" },
	{ 0, NULL }
};

static std::string enttype2s(__u32 type)
{
	for (unsigned i = 0; entity_types_def[i].str; i++)
		if (type == entity_types_def[i].flag)
			return entity_types_def[i].str;
	return "Unknown (" + num2s(type) + ")";
}

static const flag_def pad_flags_def[] = {
	{ MEDIA_PAD_FL_SINK, "Sink" },
	{ MEDIA_PAD_FL_SOURCE, "Source" },
	{ MEDIA_PAD_FL_MUST_CONNECT, "Must Connect" },
	{ 0, NULL }
};

static std::string padflags2s(__u32 flags)
{
	return flags2s(flags, pad_flags_def);
}

static const flag_def link_flags_def[] = {
	{ MEDIA_LNK_FL_ENABLED, "Enabled" },
	{ MEDIA_LNK_FL_IMMUTABLE, "Immutable" },
	{ MEDIA_LNK_FL_DYNAMIC, "Dynamic" },
	{ 0, NULL }
};

static std::string linkflags2s(__u32 flags)
{
	std::string s = flags2s(flags, link_flags_def);

	if (!s.empty())
		s = ", " + s;
	switch (flags & MEDIA_LNK_FL_LINK_TYPE) {
	case MEDIA_LNK_FL_DATA_LINK:
		return "Data" + s;
	case MEDIA_LNK_FL_INTERFACE_LINK:
		return "Interface" + s;
	default:
		return "Unknown" + s;
	}
}

static bool read_topology(int media_fd, __u32 major, __u32 minor)
{
	media_v2_topology topology;
	unsigned i, j;

	memset(&topology, 0, sizeof(topology));
	if (ioctl(media_fd, MEDIA_IOC_G_TOPOLOGY, &topology))
		return false;

	media_v2_entity v2_ents[topology.num_entities];
	media_v2_interface v2_ifaces[topology.num_interfaces];
	media_v2_pad v2_pads[topology.num_pads];
	media_v2_link v2_links[topology.num_links];

	topology.ptr_entities = (__u64)v2_ents;
	topology.ptr_interfaces = (__u64)v2_ifaces;
	topology.ptr_pads = (__u64)v2_pads;
	topology.ptr_links = (__u64)v2_links;
	if (ioctl(media_fd, MEDIA_IOC_G_TOPOLOGY, &topology))
		return false;
	for (i = 0; i < topology.num_interfaces; i++)
		if (v2_ifaces[i].devnode.major == major &&
		    v2_ifaces[i].devnode.minor == minor)
			break;
	if (i == topology.num_interfaces) {
		fprintf(stderr, "could not find %d:%d device in topology\n",
			major, minor);
		return false;
	}
	media_v2_interface &iface = v2_ifaces[i];
	for (i = 0; i < topology.num_links; i++) {
		__u32 type = v2_links[i].flags & MEDIA_LNK_FL_LINK_TYPE;

		if (type != MEDIA_LNK_FL_INTERFACE_LINK)
			continue;
		if (v2_links[i].source_id == iface.id)
			break;
	}
	if (i == topology.num_links) {
		fprintf(stderr, "could not find link for interface %u in topology\n",
			iface.id);
		return false;
	}
	__u32 ent_id = v2_links[i].sink_id;
	for (i = 0; i < topology.num_entities; i++) {
		if (v2_ents[i].id == ent_id)
			break;
	}
	if (i == topology.num_entities) {
		fprintf(stderr, "could not find entity %u in topology\n",
			ent_id);
		return false;
	}
	media_v2_entity &ent = v2_ents[i];

	printf("Interface Info:\n");
	printf("\tID               : 0x%08x\n", iface.id);
	printf("\tType             : %s\n", ifacetype2s(iface.intf_type).c_str());
	printf("Entity Info:\n");
	printf("\tID               : 0x%08x (%u)\n", ent.id, ent.id);
	printf("\tName             : %s\n", ent.name);
	printf("\tFunction         : %s\n", enttype2s(ent.function).c_str());

	// Yes, I know, lots of nested for-loops. If we get really complex
	// devices with such large topologies that this becomes too inefficient
	// then this needs to be changed and we'd need some maps to quickly
	// look up values.
	for (i = 0; i < topology.num_pads; i++) {
		const media_v2_pad &pad = v2_pads[i];

		if (pad.entity_id != ent.id)
			continue;
		printf("\tPad 0x%08x   : %s\n",
		       pad.id, padflags2s(pad.flags).c_str());
		for (j = 0; j < topology.num_links; j++) {
			const media_v2_link &link = v2_links[j];
			__u32 type = link.flags & MEDIA_LNK_FL_LINK_TYPE;
			__u32 remote_pad;
			__u32 remote_ent_id = 0;
			const media_v2_entity *remote_ent = NULL;

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
				fprintf(stderr, "could not find remote pad 0x%08x in topology\n",
					remote_pad);
				return true;
			}
			for (unsigned k = 0; k < topology.num_entities; k++)
				if (v2_ents[k].id == remote_ent_id) {
					remote_ent = &v2_ents[k];
					break;
				}
			if (!remote_ent) {
				fprintf(stderr, "could not find remote entity 0x%08x in topology\n",
					remote_ent_id);
				return true;
			}
			printf("\t  Link 0x%08x: %s remote pad 0x%x of entity '%s': %s\n",
			       link.id, is_sink ? "from" : "to", remote_pad,
			       remote_ent->name, linkflags2s(link.flags).c_str());
		}
	}
	return true;
}

void mi_media_info_for_fd(int media_fd, int fd)
{
	struct media_device_info mdinfo;
	struct stat sb;

	if (ioctl(media_fd, MEDIA_IOC_DEVICE_INFO, &mdinfo))
		return;

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
		return;

	if (fstat(fd, &sb) == -1) {
		fprintf(stderr, "failed to stat file\n");
		exit(1);
	}

	if (read_topology(media_fd, major(sb.st_rdev), minor(sb.st_rdev)))
		return;

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
		return;

	printf("Entity Info:\n");
	printf("\tID               : %u\n", ent.id);
	printf("\tName             : %s\n", ent.name);
	printf("\tType             : %s\n", enttype2s(ent.type).c_str());
	if (ent.flags)
		printf("\tFlags            : %s\n", entflags2s(ent.flags).c_str());
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
		return;

	for (unsigned i = 0; i < ent.pads; i++)
		printf("\tPad              : %u: %s\n", pads[i].index,
		       padflags2s(pads[i].flags).c_str());
	for (unsigned i = 0; i < ent.links; i++)
		printf("\tLink             : %u->%u: %s\n",
		       links[i].source.entity,
		       links[i].sink.entity,
		       linkflags2s(links[i].flags).c_str());
}
