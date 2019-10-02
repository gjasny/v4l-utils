// SPDX-License-Identifier: (GPL-2.0-only OR BSD-3-Clause)
/*
 * Copyright 2019 Cisco Systems, Inc. and/or its affiliates. All rights reserved.
 */

#ifndef _CEC_PARSE_H_
#define _CEC_PARSE_H_

#include "cec-parse-gen.h"

void cec_parse_usage_options(const char *options);
void cec_parse_msg_args(struct cec_msg &msg, int reply, const struct cec_msg_args *opt, int ch);
int cec_parse_subopt(char **subs, const char * const *subopts, char **value);
unsigned cec_parse_phys_addr(const char *value);

#endif
