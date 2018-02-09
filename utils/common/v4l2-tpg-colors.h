/*
 * v4l2-tpg-colors.h - Color definitions for the test pattern generator
 *
 * Copyright 2014 Cisco Systems, Inc. and/or its affiliates. All rights reserved.
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

#ifndef _V4L2_TPG_COLORS_H_
#define _V4L2_TPG_COLORS_H_

#include <v4l2-tpg.h>

extern const struct tpg_rbg_color8 tpg_colors[TPG_COLOR_MAX];
extern const unsigned short tpg_rec709_to_linear[255 * 16 + 1];
extern const unsigned short tpg_linear_to_rec709[255 * 16 + 1];
extern const struct tpg_rbg_color16 tpg_csc_colors[V4L2_COLORSPACE_DCI_P3 + 1]
						  [V4L2_XFER_FUNC_SMPTE2084 + 1]
						  [TPG_COLOR_CSC_BLACK + 1];

#endif
