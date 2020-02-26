/*
 * File auto-generated from the kernel sources. Please, don't edit it
 */
#ifndef _DVB_V5_CONSTS_H
#define _DVB_V5_CONSTS_H
#include <libdvbv5/dvb-frontend.h>
struct fe_caps_name {
	unsigned  idx;
	char *name;
};
extern struct fe_caps_name fe_caps_name[31];
struct fe_status_name {
	unsigned  idx;
	char *name;
};
extern struct fe_status_name fe_status_name[8];
extern const char *fe_code_rate_name[14];
extern const char *fe_modulation_name[15];
extern const char *fe_transmission_mode_name[10];
extern const unsigned fe_bandwidth_name[8];
extern const char *fe_guard_interval_name[12];
extern const char *fe_hierarchy_name[6];
extern const char *fe_voltage_name[4];
extern const char *fe_tone_name[3];
extern const char *fe_inversion_name[4];
extern const char *fe_pilot_name[4];
extern const char *fe_rolloff_name[5];
extern const char *dvb_v5_name[72];
extern const char *delivery_system_name[20];
#endif
