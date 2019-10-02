// SPDX-License-Identifier: (GPL-2.0-only OR BSD-3-Clause)
/*
 * Copyright 2019 Cisco Systems, Inc. and/or its affiliates. All rights reserved.
 */

#ifndef _CEC_LOG_H_
#define _CEC_LOG_H_

void log_msg(const struct cec_msg *msg);
void log_htng_msg(const struct cec_msg *msg);
const char *ui_cmd_string(__u8 ui_cmd);

#endif
