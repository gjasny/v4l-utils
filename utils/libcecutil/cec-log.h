// SPDX-License-Identifier: (GPL-2.0-only OR BSD-3-Clause)
/*
 * Copyright 2019 Cisco Systems, Inc. and/or its affiliates. All rights reserved.
 */

#ifndef _CEC_LOG_H_
#define _CEC_LOG_H_

struct cec_arg_enum_values {
	const char *type_name;
	__u8 value;
};

enum cec_arg_type {
	CEC_ARG_TYPE_U8,
	CEC_ARG_TYPE_U16,
	CEC_ARG_TYPE_U32,
	CEC_ARG_TYPE_STRING,
	CEC_ARG_TYPE_ENUM,
};

struct cec_arg {
	enum cec_arg_type type;
	__u8 num_enum_values;
	const struct cec_arg_enum_values *values;
};

#define CEC_MAX_ARGS 16

struct cec_msg_args {
	__u8 msg;
	__u8 num_args;
	const char *arg_names[CEC_MAX_ARGS+1];
	const struct cec_arg *args[CEC_MAX_ARGS];
	const char *msg_name;
};

const struct cec_msg_args *cec_log_msg_args(unsigned int index);
void cec_log_msg(const struct cec_msg *msg);
void cec_log_htng_msg(const struct cec_msg *msg);
const char *cec_log_ui_cmd_string(__u8 ui_cmd);

#endif
