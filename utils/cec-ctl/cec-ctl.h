/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright 2017 Cisco Systems, Inc. and/or its affiliates. All rights reserved.
 */

#ifndef _CEC_CTL_H_
#define _CEC_CTL_H_

#include <cec-info.h>

// cec-ctl.cpp
extern bool show_info;
std::string ts2s(__u64 ts);

// cec-pin.cpp
extern __u64 eob_ts;
extern __u64 eob_ts_max;
void log_event_pin(bool is_high, __u64 ts, bool show);

#endif
