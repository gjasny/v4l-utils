/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * qvidcap: a control panel controlling v4l2 devices.
 *
 * Copyright 2018 Cisco Systems, Inc. and/or its affiliates. All rights reserved.
 */

#ifndef QVIDCAP_H
#define QVIDCAP_H

#include <config.h>

// Must come before cv4l-helpers.h
#include <libv4l2.h>

#include "cv4l-helpers.h"
#include "capture-win-gl.h"

__u32 read_u32(int fd);
int initSocket(int port, cv4l_fmt &fmt, v4l2_fract &pixelaspect);

#endif
