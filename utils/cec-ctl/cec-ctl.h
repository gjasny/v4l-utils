/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright 2017 Cisco Systems, Inc. and/or its affiliates. All rights reserved.
 */

#ifndef _CEC_CTL_H_
#define _CEC_CTL_H_

#include <cec-info.h>

// cec-ctl.cpp
extern bool verbose;
std::string ts2s(__u64 ts);
std::string ts2s(double ts);
void log_msg(const struct cec_msg *msg);

// cec-pin.cpp
extern __u64 eob_ts;
extern __u64 eob_ts_max;
void log_event_pin(bool is_high, __u64 ts, bool show);

#endif
