// SPDX-License-Identifier: LGPL-2.1-only
/*
 * CEC common helper functions
 *
 * Copyright 2017 Cisco Systems, Inc. and/or its affiliates. All rights reserved.
 */

#include <stdio.h>
#include <string>
#include <cec-info.h>

static std::string caps2s(unsigned caps)
{
	std::string s;

	if (caps & CEC_CAP_PHYS_ADDR)
		s += "\t\tPhysical Address\n";
	if (caps & CEC_CAP_LOG_ADDRS)
		s += "\t\tLogical Addresses\n";
	if (caps & CEC_CAP_TRANSMIT)
		s += "\t\tTransmit\n";
	if (caps & CEC_CAP_PASSTHROUGH)
		s += "\t\tPassthrough\n";
	if (caps & CEC_CAP_RC)
		s += "\t\tRemote Control Support\n";
	if (caps & CEC_CAP_MONITOR_ALL)
		s += "\t\tMonitor All\n";
	if (caps & CEC_CAP_NEEDS_HPD)
		s += "\t\tNeeds HPD\n";
	if (caps & CEC_CAP_MONITOR_PIN)
		s += "\t\tMonitor Pin\n";
	return s;
}

static std::string laflags2s(unsigned flags)
{
	std::string s;

	if (!flags)
		return s;

	s = "(";
	if (flags & CEC_LOG_ADDRS_FL_ALLOW_UNREG_FALLBACK)
		s += "Allow Fallback to Unregistered, ";
	if (flags & CEC_LOG_ADDRS_FL_ALLOW_RC_PASSTHRU)
		s += "Allow RC Passthrough, ";
	if (flags & CEC_LOG_ADDRS_FL_CDC_ONLY)
		s += "CDC-Only, ";
	if (s.length())
		s.erase(s.length() - 2, 2);
	return s + ")";
}

const char *version2s(unsigned version)
{
	switch (version) {
	case CEC_OP_CEC_VERSION_1_3A:
		return "1.3a";
	case CEC_OP_CEC_VERSION_1_4:
		return "1.4";
	case CEC_OP_CEC_VERSION_2_0:
		return "2.0";
	default:
		return "Unknown";
	}
}

/*
 * Most of these vendor IDs come from include/cectypes.h from libcec.
 */
const char *vendor2s(unsigned vendor)
{
	switch (vendor) {
	case 0x000039:
	case 0x000ce7:
		return "(Toshiba)";
	case 0x0000f0:
		return "(Samsung)";
	case 0x0005cd:
		return "(Denon)";
	case 0x000678:
		return "(Marantz)";
	case 0x000982:
		return "(Loewe)";
	case 0x0009b0:
		return "(Onkyo)";
	case 0x000c03:
		return "(HDMI)";
	case 0x001582:
		return "(Pulse-Eight)";
	case 0x001950:
	case 0x9c645e:
		return "(Harman Kardon)";
	case 0x001a11:
		return "(Google)";
	case 0x0020c7:
		return "(Akai)";
	case 0x002467:
		return "(AOC)";
	case 0x005060:
		return "(Cisco)";
	case 0x008045:
		return "(Panasonic)";
	case 0x00903e:
		return "(Philips)";
	case 0x009053:
		return "(Daewoo)";
	case 0x00a0de:
		return "(Yamaha)";
	case 0x00d0d5:
		return "(Grundig)";
	case 0x00d38d:
		return "(Hospitality Profile)";
	case 0x00e036:
		return "(Pioneer)";
	case 0x00e091:
		return "(LG)";
	case 0x08001f:
	case 0x534850:
		return "(Sharp)";
	case 0x080046:
		return "(Sony)";
	case 0x18c086:
		return "(Broadcom)";
	case 0x6b746d:
		return "(Vizio)";
	case 0x743a65:
		return "(NEC)";
	case 0x8065e9:
		return "(Benq)";
	default:
		return "";
	}
}

const char *prim_type2s(unsigned type)
{
	switch (type) {
	case CEC_OP_PRIM_DEVTYPE_TV:
		return "TV";
	case CEC_OP_PRIM_DEVTYPE_RECORD:
		return "Record";
	case CEC_OP_PRIM_DEVTYPE_TUNER:
		return "Tuner";
	case CEC_OP_PRIM_DEVTYPE_PLAYBACK:
		return "Playback";
	case CEC_OP_PRIM_DEVTYPE_AUDIOSYSTEM:
		return "Audio System";
	case CEC_OP_PRIM_DEVTYPE_SWITCH:
		return "Switch";
	case CEC_OP_PRIM_DEVTYPE_PROCESSOR:
		return "Processor";
	default:
		return "Unknown";
	}
}

const char *la_type2s(unsigned type)
{
	switch (type) {
	case CEC_LOG_ADDR_TYPE_TV:
		return "TV";
	case CEC_LOG_ADDR_TYPE_RECORD:
		return "Record";
	case CEC_LOG_ADDR_TYPE_TUNER:
		return "Tuner";
	case CEC_LOG_ADDR_TYPE_PLAYBACK:
		return "Playback";
	case CEC_LOG_ADDR_TYPE_AUDIOSYSTEM:
		return "Audio System";
	case CEC_LOG_ADDR_TYPE_SPECIFIC:
		return "Specific";
	case CEC_LOG_ADDR_TYPE_UNREGISTERED:
		return "Unregistered";
	default:
		return "Unknown";
	}
}

const char *la2s(unsigned la)
{
	switch (la & 0xf) {
	case 0:
		return "TV";
	case 1:
		return "Recording Device 1";
	case 2:
		return "Recording Device 2";
	case 3:
		return "Tuner 1";
	case 4:
		return "Playback Device 1";
	case 5:
		return "Audio System";
	case 6:
		return "Tuner 2";
	case 7:
		return "Tuner 3";
	case 8:
		return "Playback Device 2";
	case 9:
		return "Recording Device 3";
	case 10:
		return "Tuner 4";
	case 11:
		return "Playback Device 3";
	case 12:
		return "Reserved 1";
	case 13:
		return "Reserved 2";
	case 14:
		return "Specific";
	case 15:
	default:
		return "Unregistered";
	}
}

std::string all_dev_types2s(unsigned types)
{
	std::string s;

	if (types & CEC_OP_ALL_DEVTYPE_TV)
		s += "TV, ";
	if (types & CEC_OP_ALL_DEVTYPE_RECORD)
		s += "Record, ";
	if (types & CEC_OP_ALL_DEVTYPE_TUNER)
		s += "Tuner, ";
	if (types & CEC_OP_ALL_DEVTYPE_PLAYBACK)
		s += "Playback, ";
	if (types & CEC_OP_ALL_DEVTYPE_AUDIOSYSTEM)
		s += "Audio System, ";
	if (types & CEC_OP_ALL_DEVTYPE_SWITCH)
		s += "Switch, ";
	if (s.length())
		return s.erase(s.length() - 2, 2);
	return s;
}

std::string rc_src_prof2s(unsigned prof, const std::string &prefix)
{
	std::string s;

	prof &= 0x1f;
	if (prof == 0)
		return prefix + "\t\tNone\n";
	if (prof & CEC_OP_FEAT_RC_SRC_HAS_DEV_ROOT_MENU)
		s += prefix + "\t\tSource Has Device Root Menu\n";
	if (prof & CEC_OP_FEAT_RC_SRC_HAS_DEV_SETUP_MENU)
		s += prefix + "\t\tSource Has Device Setup Menu\n";
	if (prof & CEC_OP_FEAT_RC_SRC_HAS_MEDIA_CONTEXT_MENU)
		s += prefix + "\t\tSource Has Contents Menu\n";
	if (prof & CEC_OP_FEAT_RC_SRC_HAS_MEDIA_TOP_MENU)
		s += prefix + "\t\tSource Has Media Top Menu\n";
	if (prof & CEC_OP_FEAT_RC_SRC_HAS_MEDIA_CONTEXT_MENU)
		s += prefix + "\t\tSource Has Media Context-Sensitive Menu\n";
	return s;
}

std::string dev_feat2s(unsigned feat, const std::string &prefix)
{
	std::string s;

	feat &= 0x7e;
	if (feat == 0)
		return prefix + "\t\tNone\n";
	if (feat & CEC_OP_FEAT_DEV_HAS_RECORD_TV_SCREEN)
		s += prefix + "\t\tTV Supports <Record TV Screen>\n";
	if (feat & CEC_OP_FEAT_DEV_HAS_SET_OSD_STRING)
		s += prefix + "\t\tTV Supports <Set OSD String>\n";
	if (feat & CEC_OP_FEAT_DEV_HAS_DECK_CONTROL)
		s += prefix + "\t\tSupports Deck Control\n";
	if (feat & CEC_OP_FEAT_DEV_HAS_SET_AUDIO_RATE)
		s += prefix + "\t\tSource Supports <Set Audio Rate>\n";
	if (feat & CEC_OP_FEAT_DEV_SINK_HAS_ARC_TX)
		s += prefix + "\t\tSink Supports ARC Tx\n";
	if (feat & CEC_OP_FEAT_DEV_SOURCE_HAS_ARC_RX)
		s += prefix + "\t\tSource Supports ARC Rx\n";
	return s;
}

static std::string tx_status2s(const struct cec_msg &msg)
{
	std::string s;
	char num[4];
	unsigned stat = msg.tx_status;

	if (stat)
		s += "Tx";
	if (stat & CEC_TX_STATUS_OK)
		s += ", OK";
	if (stat & CEC_TX_STATUS_ARB_LOST) {
		sprintf(num, "%u", msg.tx_arb_lost_cnt);
		s += ", Arbitration Lost";
		if (msg.tx_arb_lost_cnt)
			s += " (" + std::string(num) + ")";
	}
	if (stat & CEC_TX_STATUS_NACK) {
		sprintf(num, "%u", msg.tx_nack_cnt);
		s += ", Not Acknowledged";
		if (msg.tx_nack_cnt)
			s += " (" + std::string(num) + ")";
	}
	if (stat & CEC_TX_STATUS_LOW_DRIVE) {
		sprintf(num, "%u", msg.tx_low_drive_cnt);
		s += ", Low Drive";
		if (msg.tx_low_drive_cnt)
			s += " (" + std::string(num) + ")";
	}
	if (stat & CEC_TX_STATUS_ERROR) {
		sprintf(num, "%u", msg.tx_error_cnt);
		s += ", Error";
		if (msg.tx_error_cnt)
			s += " (" + std::string(num) + ")";
	}
	if (stat & CEC_TX_STATUS_ABORTED)
		s += ", Aborted";
	if (stat & CEC_TX_STATUS_TIMEOUT)
		s += ", Timeout";
	if (stat & CEC_TX_STATUS_MAX_RETRIES)
		s += ", Max Retries";
	return s;
}

static std::string rx_status2s(unsigned stat)
{
	std::string s;

	if (stat)
		s += "Rx";
	if (stat & CEC_RX_STATUS_OK)
		s += ", OK";
	if (stat & CEC_RX_STATUS_TIMEOUT)
		s += ", Timeout";
	if (stat & CEC_RX_STATUS_FEATURE_ABORT)
		s += ", Feature Abort";
	if (stat & CEC_RX_STATUS_ABORTED)
		s += ", Aborted";
	return s;
}

std::string status2s(const struct cec_msg &msg)
{
	std::string s;

	if (msg.tx_status)
		s = tx_status2s(msg);
	if (msg.rx_status) {
		if (!s.empty())
			s += ", ";
		s += rx_status2s(msg.rx_status);
	}
	return s;
}

void cec_driver_info(const struct cec_caps &caps,
		     const struct cec_log_addrs &laddrs, __u16 phys_addr)
{
	printf("Driver Info:\n");
	printf("\tDriver Name                : %s\n", caps.driver);
	printf("\tAdapter Name               : %s\n", caps.name);
	printf("\tCapabilities               : 0x%08x\n", caps.capabilities);
	printf("%s", caps2s(caps.capabilities).c_str());
	printf("\tDriver version             : %d.%d.%d\n",
			caps.version >> 16,
			(caps.version >> 8) & 0xff,
			caps.version & 0xff);
	printf("\tAvailable Logical Addresses: %u\n",
	       caps.available_log_addrs);

	printf("\tPhysical Address           : %x.%x.%x.%x\n",
	       cec_phys_addr_exp(phys_addr));
	printf("\tLogical Address Mask       : 0x%04x\n", laddrs.log_addr_mask);
	printf("\tCEC Version                : %s\n", version2s(laddrs.cec_version));
	if (laddrs.vendor_id != CEC_VENDOR_ID_NONE)
		printf("\tVendor ID                  : 0x%06x %s\n",
		       laddrs.vendor_id, vendor2s(laddrs.vendor_id));
	printf("\tOSD Name                   : %s\n", laddrs.osd_name);
	printf("\tLogical Addresses          : %u %s\n",
	       laddrs.num_log_addrs, laflags2s(laddrs.flags).c_str());
	for (unsigned i = 0; i < laddrs.num_log_addrs; i++) {
		if (laddrs.log_addr[i] == CEC_LOG_ADDR_INVALID)
			printf("\n\t  Logical Address          : Not Allocated\n");
		else
			printf("\n\t  Logical Address          : %d (%s)\n",
			       laddrs.log_addr[i], la2s(laddrs.log_addr[i]));
		printf("\t    Primary Device Type    : %s\n",
		       prim_type2s(laddrs.primary_device_type[i]));
		printf("\t    Logical Address Type   : %s\n",
		       la_type2s(laddrs.log_addr_type[i]));
		if (laddrs.cec_version < CEC_OP_CEC_VERSION_2_0)
			continue;
		printf("\t    All Device Types       : %s\n",
		       all_dev_types2s(laddrs.all_device_types[i]).c_str());

		bool is_dev_feat = false;
		for (unsigned idx = 0; idx < sizeof(laddrs.features[0]); idx++) {
			__u8 byte = laddrs.features[i][idx];

			if (!is_dev_feat) {
				if (byte & 0x40) {
					printf("\t    RC Source Profile      :\n%s",
					       rc_src_prof2s(byte, "").c_str());
				} else {
					const char *s = "Reserved";

					switch (byte & 0xf) {
					case 0:
						s = "None";
						break;
					case 2:
						s = "RC Profile 1";
						break;
					case 6:
						s = "RC Profile 2";
						break;
					case 10:
						s = "RC Profile 3";
						break;
					case 14:
						s = "RC Profile 4";
						break;
					}
					printf("\t    RC TV Profile          : %s\n", s);
				}
			} else {
				printf("\t    Device Features        :\n%s",
				       dev_feat2s(byte, "").c_str());
			}
			if (byte & CEC_OP_FEAT_EXT)
				continue;
			if (!is_dev_feat)
				is_dev_feat = true;
			else
				break;
		}
	}
}
