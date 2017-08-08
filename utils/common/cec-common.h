/*
 * CEC common helper functions
 *
 * Copyright 2017 Cisco Systems, Inc. and/or its affiliates. All rights reserved.
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

#ifndef _CEC_COMMON_H_
#define _CEC_COMMON_H_

#include <linux/cec.h>

#define cec_phys_addr_exp(pa) \
        ((pa) >> 12), ((pa) >> 8) & 0xf, ((pa) >> 4) & 0xf, (pa) & 0xf

const char *la2s(unsigned la);
const char *la_type2s(unsigned type);
const char *prim_type2s(unsigned type);
const char *version2s(unsigned version);
std::string all_dev_types2s(unsigned types);
std::string rc_src_prof2s(unsigned prof, const std::string &prefix);
std::string dev_feat2s(unsigned feat, const std::string &prefix);

void cec_driver_info(const struct cec_caps &caps,
		     const struct cec_log_addrs &laddrs, __u16 phys_addr);

#endif
