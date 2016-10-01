/*
 * cec-htng-funcs - HDMI CEC wrapper functions for Hospitality Profile
 *
 * Copyright 2016-2017 Cisco Systems, Inc. and/or its affiliates. All rights reserved.
 *
 * This program is free software; you may redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * Alternatively you can redistribute this file under the terms of the
 * BSD license as stated below:
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. The names of its contributors may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
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

#ifndef _CEC_HTNG_FUNCS_H
#define _CEC_HTNG_FUNCS_H

#include "cec-htng.h"

static inline void cec_msg_htng_init(struct cec_msg *msg, __u8 op)
{
	msg->len = 6;
	msg->msg[1] = CEC_MSG_VENDOR_COMMAND_WITH_ID;
	msg->msg[2] = VENDOR_ID_HTNG >> 16;
	msg->msg[3] = (VENDOR_ID_HTNG >> 8) & 0xff;
	msg->msg[4] = VENDOR_ID_HTNG & 0xff;
	msg->msg[5] = op;
}

/* HTNG Feature */
static inline void cec_msg_htng_tuner_1part_chan(struct cec_msg *msg,
						 __u8 htng_tuner_type,
						 __u16 chan)
{
	cec_msg_htng_init(msg, CEC_MSG_HTNG_TUNER_1PART_CHAN);
	msg->msg[msg->len++] = htng_tuner_type;
	msg->msg[msg->len++] = chan >> 8;
	msg->msg[msg->len++] = chan & 0xff;
}

static inline void cec_ops_htng_tuner_1part_chan(const struct cec_msg *msg,
						 __u8 *htng_tuner_type,
						 __u16 *chan)
{
	*htng_tuner_type = msg->msg[6];
	*chan = (msg->msg[7] << 8) | msg->msg[8];
}

static inline void cec_msg_htng_tuner_2part_chan(struct cec_msg *msg,
						 __u8 htng_tuner_type,
						 __u8 major_chan,
						 __u16 minor_chan)
{
	cec_msg_htng_init(msg, CEC_MSG_HTNG_TUNER_2PART_CHAN);
	msg->msg[msg->len++] = htng_tuner_type;
	msg->msg[msg->len++] = major_chan;
	msg->msg[msg->len++] = minor_chan >> 8;
	msg->msg[msg->len++] = minor_chan & 0xff;
}

static inline void cec_ops_htng_tuner_2part_chan(const struct cec_msg *msg,
						 __u8 *htng_tuner_type,
						 __u8 *major_chan,
						 __u16 *minor_chan)
{
	*htng_tuner_type = msg->msg[6];
	*major_chan = msg->msg[7];
	*minor_chan = (msg->msg[8] << 8) | msg->msg[9];
}

static inline void cec_msg_htng_input_sel_av(struct cec_msg *msg,
					     __u16 input)
{
	cec_msg_htng_init(msg, CEC_MSG_HTNG_INPUT_SEL_AV);
	msg->msg[msg->len++] = input >> 8;
	msg->msg[msg->len++] = input & 0xff;
}

static inline void cec_ops_htng_input_sel_av(const struct cec_msg *msg,
					     __u16 *input)
{
	*input = (msg->msg[6] << 8) | msg->msg[7];
}

static inline void cec_msg_htng_input_sel_pc(struct cec_msg *msg,
					     __u16 input)
{
	cec_msg_htng_init(msg, CEC_MSG_HTNG_INPUT_SEL_PC);
	msg->msg[msg->len++] = input >> 8;
	msg->msg[msg->len++] = input & 0xff;
}

static inline void cec_ops_htng_input_sel_pc(const struct cec_msg *msg,
					     __u16 *input)
{
	*input = (msg->msg[6] << 8) | msg->msg[7];
}

static inline void cec_msg_htng_input_sel_hdmi(struct cec_msg *msg,
					       __u16 input)
{
	cec_msg_htng_init(msg, CEC_MSG_HTNG_INPUT_SEL_HDMI);
	msg->msg[msg->len++] = input >> 8;
	msg->msg[msg->len++] = input & 0xff;
}

static inline void cec_ops_htng_input_sel_hdmi(const struct cec_msg *msg,
					       __u16 *input)
{
	*input = (msg->msg[6] << 8) | msg->msg[7];
}

static inline void cec_msg_htng_input_sel_component(struct cec_msg *msg,
						    __u16 input)
{
	cec_msg_htng_init(msg, CEC_MSG_HTNG_INPUT_SEL_COMPONENT);
	msg->msg[msg->len++] = input >> 8;
	msg->msg[msg->len++] = input & 0xff;
}

static inline void cec_ops_htng_input_sel_component(const struct cec_msg *msg,
						    __u16 *input)
{
	*input = (msg->msg[6] << 8) | msg->msg[7];
}

static inline void cec_msg_htng_input_sel_dvi(struct cec_msg *msg,
					      __u16 input)
{
	cec_msg_htng_init(msg, CEC_MSG_HTNG_INPUT_SEL_DVI);
	msg->msg[msg->len++] = input >> 8;
	msg->msg[msg->len++] = input & 0xff;
}

static inline void cec_ops_htng_input_sel_dvi(const struct cec_msg *msg,
					      __u16 *input)
{
	*input = (msg->msg[6] << 8) | msg->msg[7];
}

static inline void cec_msg_htng_input_sel_dp(struct cec_msg *msg,
					     __u16 input)
{
	cec_msg_htng_init(msg, CEC_MSG_HTNG_INPUT_SEL_DP);
	msg->msg[msg->len++] = input >> 8;
	msg->msg[msg->len++] = input & 0xff;
}

static inline void cec_ops_htng_input_sel_dp(const struct cec_msg *msg,
					     __u16 *input)
{
	*input = (msg->msg[6] << 8) | msg->msg[7];
}

static inline void cec_msg_htng_input_sel_usb(struct cec_msg *msg,
					      __u16 input)
{
	cec_msg_htng_init(msg, CEC_MSG_HTNG_INPUT_SEL_USB);
	msg->msg[msg->len++] = input >> 8;
	msg->msg[msg->len++] = input & 0xff;
}

static inline void cec_ops_htng_input_sel_usb(const struct cec_msg *msg,
					      __u16 *input)
{
	*input = (msg->msg[6] << 8) | msg->msg[7];
}

static inline void cec_msg_htng_set_def_pwr_on_input_src(struct cec_msg *msg,
							 __u8 htng_input_src,
							 __u8 htng_tuner_type,
							 __u8 major,
							 __u16 input)
{
	cec_msg_htng_init(msg, CEC_MSG_HTNG_SET_DEF_PWR_ON_INPUT_SRC);
	msg->msg[msg->len++] = htng_input_src;
	msg->msg[msg->len++] = htng_tuner_type;
	if (htng_input_src == CEC_OP_HTNG_INPUT_SRC_TUNER_2PART)
		msg->msg[msg->len++] = major;
	msg->msg[msg->len++] = input >> 8;
	msg->msg[msg->len++] = input & 0xff;
	if (htng_input_src != CEC_OP_HTNG_INPUT_SRC_TUNER_2PART)
		msg->msg[msg->len++] = 0;
}

static inline void cec_ops_htng_set_def_pwr_on_input_src(const struct cec_msg *msg,
							 __u8 *htng_input_src,
							 __u8 *htng_tuner_type,
							 __u8 *major,
							 __u16 *input)
{
	*htng_input_src = msg->msg[6];
	*htng_tuner_type = msg->msg[7];
	if (*htng_input_src == CEC_OP_HTNG_INPUT_SRC_TUNER_2PART) {
		*major = msg->msg[8];
		*input = (msg->msg[9] << 8) | msg->msg[10];
	} else {
		*major = 0;
		*input = (msg->msg[8] << 8) | msg->msg[9];
	}
}

static inline void cec_msg_htng_set_tv_speakers(struct cec_msg *msg,
						__u8 on)
{
	cec_msg_htng_init(msg, CEC_MSG_HTNG_SET_TV_SPEAKERS);
	msg->msg[msg->len++] = on;
}

static inline void cec_ops_htng_set_tv_speakers(const struct cec_msg *msg,
						__u8 *on)
{
	*on = msg->msg[6];
}

static inline void cec_msg_htng_set_dig_audio(struct cec_msg *msg,
					      __u8 on)
{
	cec_msg_htng_init(msg, CEC_MSG_HTNG_SET_DIG_AUDIO);
	msg->msg[msg->len++] = on;
}

static inline void cec_ops_htng_set_dig_audio(const struct cec_msg *msg,
					      __u8 *on)
{
	*on = msg->msg[6];
}

static inline void cec_msg_htng_set_ana_audio(struct cec_msg *msg,
					      __u8 on)
{
	cec_msg_htng_init(msg, CEC_MSG_HTNG_SET_ANA_AUDIO);
	msg->msg[msg->len++] = on;
}

static inline void cec_ops_htng_set_ana_audio(const struct cec_msg *msg,
					      __u8 *on)
{
	*on = msg->msg[6];
}

static inline void cec_msg_htng_set_def_pwr_on_vol(struct cec_msg *msg,
						   __u8 vol)
{
	cec_msg_htng_init(msg, CEC_MSG_HTNG_SET_DEF_PWR_ON_VOL);
	msg->msg[msg->len++] = vol;
}

static inline void cec_ops_htng_set_def_pwr_on_vol(const struct cec_msg *msg,
						   __u8 *vol)
{
	*vol = msg->msg[6];
}

static inline void cec_msg_htng_set_max_vol(struct cec_msg *msg,
					    __u8 vol)
{
	cec_msg_htng_init(msg, CEC_MSG_HTNG_SET_MAX_VOL);
	msg->msg[msg->len++] = vol;
}

static inline void cec_ops_htng_set_max_vol(const struct cec_msg *msg,
					    __u8 *vol)
{
	*vol = msg->msg[6];
}

static inline void cec_msg_htng_set_min_vol(struct cec_msg *msg,
					    __u8 vol)
{
	cec_msg_htng_init(msg, CEC_MSG_HTNG_SET_MIN_VOL);
	msg->msg[msg->len++] = vol;
}

static inline void cec_ops_htng_set_min_vol(const struct cec_msg *msg,
					    __u8 *vol)
{
	*vol = msg->msg[6];
}

static inline void cec_msg_htng_set_blue_screen(struct cec_msg *msg,
						__u8 blue)
{
	cec_msg_htng_init(msg, CEC_MSG_HTNG_SET_BLUE_SCREEN);
	msg->msg[msg->len++] = blue;
}

static inline void cec_ops_htng_set_blue_screen(const struct cec_msg *msg,
						__u8 *blue)
{
	*blue = msg->msg[6];
}

static inline void cec_msg_htng_set_brightness(struct cec_msg *msg,
					       __u8 brightness)
{
	cec_msg_htng_init(msg, CEC_MSG_HTNG_SET_BRIGHTNESS);
	msg->msg[msg->len++] = brightness;
}

static inline void cec_ops_htng_set_brightness(const struct cec_msg *msg,
					       __u8 *brightness)
{
	*brightness = msg->msg[6];
}

static inline void cec_msg_htng_set_color(struct cec_msg *msg,
					  __u8 color)
{
	cec_msg_htng_init(msg, CEC_MSG_HTNG_SET_COLOR);
	msg->msg[msg->len++] = color;
}

static inline void cec_ops_htng_set_color(const struct cec_msg *msg,
					  __u8 *color)
{
	*color = msg->msg[6];
}

static inline void cec_msg_htng_set_contrast(struct cec_msg *msg,
					     __u8 contrast)
{
	cec_msg_htng_init(msg, CEC_MSG_HTNG_SET_CONTRAST);
	msg->msg[msg->len++] = contrast;
}

static inline void cec_ops_htng_set_contrast(const struct cec_msg *msg,
					     __u8 *contrast)
{
	*contrast = msg->msg[6];
}

static inline void cec_msg_htng_set_sharpness(struct cec_msg *msg,
					      __u8 sharpness)
{
	cec_msg_htng_init(msg, CEC_MSG_HTNG_SET_SHARPNESS);
	msg->msg[msg->len++] = sharpness;
}

static inline void cec_ops_htng_set_sharpness(const struct cec_msg *msg,
					      __u8 *sharpness)
{
	*sharpness = msg->msg[6];
}

static inline void cec_msg_htng_set_hue(struct cec_msg *msg,
					__u8 hue)
{
	cec_msg_htng_init(msg, CEC_MSG_HTNG_SET_HUE);
	msg->msg[msg->len++] = hue;
}

static inline void cec_ops_htng_set_hue(const struct cec_msg *msg,
					__u8 *hue)
{
	*hue = msg->msg[6];
}

static inline void cec_msg_htng_set_led_backlight(struct cec_msg *msg,
						  __u8 led_backlight)
{
	cec_msg_htng_init(msg, CEC_MSG_HTNG_SET_LED_BACKLIGHT);
	msg->msg[msg->len++] = led_backlight;
}

static inline void cec_ops_htng_set_led_backlight(const struct cec_msg *msg,
						  __u8 *led_backlight)
{
	*led_backlight = msg->msg[6];
}

static inline void cec_msg_htng_set_tv_osd_control(struct cec_msg *msg,
						   __u8 on)
{
	cec_msg_htng_init(msg, CEC_MSG_HTNG_SET_TV_OSD_CONTROL);
	msg->msg[msg->len++] = on;
}

static inline void cec_ops_htng_set_tv_osd_control(const struct cec_msg *msg,
						   __u8 *on)
{
	*on = msg->msg[6];
}

static inline void cec_msg_htng_set_audio_only_display(struct cec_msg *msg,
						       __u8 on)
{
	cec_msg_htng_init(msg, CEC_MSG_HTNG_SET_AUDIO_ONLY_DISPLAY);
	msg->msg[msg->len++] = on;
}

static inline void cec_ops_htng_set_audio_only_display(const struct cec_msg *msg,
						       __u8 *on)
{
	*on = msg->msg[6];
}

static inline void cec_msg_htng_set_date(struct cec_msg *msg,
					 const char *date)
{
	cec_msg_htng_init(msg, CEC_MSG_HTNG_SET_DATE);
	memcpy(msg->msg + msg->len, date, 8);
	msg->len += 8;
}

static inline void cec_ops_htng_set_date(const struct cec_msg *msg,
					 char *date)
{
	memcpy(date, msg->msg + 6, 8);
	date[8] = '\0';
}

static inline void cec_msg_htng_set_date_format(struct cec_msg *msg,
						__u8 ddmm)
{
	cec_msg_htng_init(msg, CEC_MSG_HTNG_SET_DATE_FORMAT);
	msg->msg[msg->len++] = ddmm;
}

static inline void cec_ops_htng_set_date_format(const struct cec_msg *msg,
						__u8 *ddmm)
{
	*ddmm = msg->msg[6];
}

static inline void cec_msg_htng_set_time(struct cec_msg *msg,
					 const char *time)
{
	cec_msg_htng_init(msg, CEC_MSG_HTNG_SET_TIME);
	memcpy(msg->msg + msg->len, time, 6);
	msg->len += 6;
}

static inline void cec_ops_htng_set_time(const struct cec_msg *msg,
					 char *time)
{
	memcpy(time, msg->msg + 6, 6);
	time[6] = '\0';
}

static inline void cec_msg_htng_set_clk_brightness_standby(struct cec_msg *msg,
							   __u8 brightness)
{
	cec_msg_htng_init(msg, CEC_MSG_HTNG_SET_CLK_BRIGHTNESS_STANDBY);
	msg->msg[msg->len++] = brightness;
}

static inline void cec_ops_htng_set_clk_brightness_standby(const struct cec_msg *msg,
							   __u8 *brightness)
{
	*brightness = msg->msg[6];
}

static inline void cec_msg_htng_set_clk_brightness_on(struct cec_msg *msg,
						      __u8 brightness)
{
	cec_msg_htng_init(msg, CEC_MSG_HTNG_SET_CLK_BRIGHTNESS_ON);
	msg->msg[msg->len++] = brightness;
}

static inline void cec_ops_htng_set_clk_brightness_on(const struct cec_msg *msg,
						      __u8 *brightness)
{
	*brightness = msg->msg[6];
}

static inline void cec_msg_htng_led_control(struct cec_msg *msg,
					    __u8 htng_led_control)
{
	cec_msg_htng_init(msg, CEC_MSG_HTNG_LED_CONTROL);
	msg->msg[msg->len++] = htng_led_control;
}

static inline void cec_ops_htng_led_control(const struct cec_msg *msg,
					    __u8 *htng_led_control)
{
	*htng_led_control = msg->msg[6];
}

static inline void cec_msg_htng_lock_tv_pwr_button(struct cec_msg *msg,
						   __u8 on)
{
	cec_msg_htng_init(msg, CEC_MSG_HTNG_LOCK_TV_PWR_BUTTON);
	msg->msg[msg->len++] = on;
}

static inline void cec_ops_htng_lock_tv_pwr_button(const struct cec_msg *msg,
						   __u8 *on)
{
	*on = msg->msg[6];
}

static inline void cec_msg_htng_lock_tv_vol_buttons(struct cec_msg *msg,
						    __u8 on)
{
	cec_msg_htng_init(msg, CEC_MSG_HTNG_LOCK_TV_VOL_BUTTONS);
	msg->msg[msg->len++] = on;
}

static inline void cec_ops_htng_lock_tv_vol_buttons(const struct cec_msg *msg,
						    __u8 *on)
{
	*on = msg->msg[6];
}

static inline void cec_msg_htng_lock_tv_chan_buttons(struct cec_msg *msg,
						     __u8 on)
{
	cec_msg_htng_init(msg, CEC_MSG_HTNG_LOCK_TV_CHAN_BUTTONS);
	msg->msg[msg->len++] = on;
}

static inline void cec_ops_htng_lock_tv_chan_buttons(const struct cec_msg *msg,
						     __u8 *on)
{
	*on = msg->msg[6];
}

static inline void cec_msg_htng_lock_tv_input_buttons(struct cec_msg *msg,
						      __u8 on)
{
	cec_msg_htng_init(msg, CEC_MSG_HTNG_LOCK_TV_INPUT_BUTTONS);
	msg->msg[msg->len++] = on;
}

static inline void cec_ops_htng_lock_tv_input_buttons(const struct cec_msg *msg,
						      __u8 *on)
{
	*on = msg->msg[6];
}

static inline void cec_msg_htng_lock_tv_other_buttons(struct cec_msg *msg,
						      __u8 on)
{
	cec_msg_htng_init(msg, CEC_MSG_HTNG_LOCK_TV_OTHER_BUTTONS);
	msg->msg[msg->len++] = on;
}

static inline void cec_ops_htng_lock_tv_other_buttons(const struct cec_msg *msg,
						      __u8 *on)
{
	*on = msg->msg[6];
}

static inline void cec_msg_htng_lock_everything(struct cec_msg *msg,
						__u8 on)
{
	cec_msg_htng_init(msg, CEC_MSG_HTNG_LOCK_EVERYTHING);
	msg->msg[msg->len++] = on;
}

static inline void cec_ops_htng_lock_everything(const struct cec_msg *msg,
						__u8 *on)
{
	*on = msg->msg[6];
}

static inline void cec_msg_htng_lock_everything_but_pwr(struct cec_msg *msg,
							__u8 on)
{
	cec_msg_htng_init(msg, CEC_MSG_HTNG_LOCK_EVERYTHING_BUT_PWR);
	msg->msg[msg->len++] = on;
}

static inline void cec_ops_htng_lock_everything_but_pwr(const struct cec_msg *msg,
							__u8 *on)
{
	*on = msg->msg[6];
}

static inline void cec_msg_htng_hotel_mode(struct cec_msg *msg,
					   __u8 on, __u8 options)
{
	cec_msg_htng_init(msg, CEC_MSG_HTNG_HOTEL_MODE);
	msg->msg[msg->len++] = on;
	msg->msg[msg->len++] = options;
}

static inline void cec_ops_htng_hotel_mode(const struct cec_msg *msg,
					   __u8 *on, __u8 *options)
{
	*on = msg->msg[6];
	*options = msg->msg[7];
}

static inline void cec_msg_htng_set_pwr_saving_profile(struct cec_msg *msg,
						       __u8 on, __u8 val)
{
	cec_msg_htng_init(msg, CEC_MSG_HTNG_SET_PWR_SAVING_PROFILE);
	msg->msg[msg->len++] = on;
	msg->msg[msg->len++] = val;
}

static inline void cec_ops_htng_set_pwr_saving_profile(const struct cec_msg *msg,
						       __u8 *on, __u8 *val)
{
	*on = msg->msg[6];
	*val = msg->msg[7];
}

static inline void cec_msg_htng_set_sleep_timer(struct cec_msg *msg,
						__u8 minutes)
{
	cec_msg_htng_init(msg, CEC_MSG_HTNG_SET_SLEEP_TIMER);
	msg->msg[msg->len++] = minutes;
}

static inline void cec_ops_htng_set_sleep_timer(const struct cec_msg *msg,
						__u8 *minutes)
{
	*minutes = msg->msg[6];
}

static inline void cec_msg_htng_set_wakeup_time(struct cec_msg *msg,
						const char *time)
{
	cec_msg_htng_init(msg, CEC_MSG_HTNG_SET_WAKEUP_TIME);
	memcpy(msg->msg + 6, time, 4);
	msg->len += 4;
}

static inline void cec_ops_htng_set_wakeup_time(const struct cec_msg *msg,
						char *time)
{
	memcpy(time, msg->msg + 6, 4);
	time[4] = '\0';
}

static inline void cec_msg_htng_set_auto_off_time(struct cec_msg *msg,
						  const char *time)
{
	cec_msg_htng_init(msg, CEC_MSG_HTNG_SET_AUTO_OFF_TIME);
	memcpy(msg->msg + 6, time, 4);
	msg->len += 4;
}

static inline void cec_ops_htng_set_auto_off_time(const struct cec_msg *msg,
						  char *time)
{
	memcpy(time, msg->msg + 6, 4);
	time[4] = '\0';
}

static inline void cec_msg_htng_set_wakeup_src(struct cec_msg *msg,
					       __u8 htng_input_src,
					       __u8 htng_tuner_type,
					       __u8 major,
					       __u16 input)
{
	cec_msg_htng_init(msg, CEC_MSG_HTNG_SET_WAKEUP_SRC);
	msg->msg[msg->len++] = htng_input_src;
	msg->msg[msg->len++] = htng_tuner_type;
	if (htng_input_src == CEC_OP_HTNG_INPUT_SRC_TUNER_2PART)
		msg->msg[msg->len++] = major;
	msg->msg[msg->len++] = input >> 8;
	msg->msg[msg->len++] = input & 0xff;
	if (htng_input_src != CEC_OP_HTNG_INPUT_SRC_TUNER_2PART)
		msg->msg[msg->len++] = 0;
}

static inline void cec_ops_htng_set_wakeup_src(const struct cec_msg *msg,
					__u8 *htng_input_src,
					__u8 *htng_tuner_type,
					__u8 *major,
					__u16 *input)
{
	*htng_input_src = msg->msg[6];
	*htng_tuner_type = msg->msg[7];
	if (*htng_input_src == CEC_OP_HTNG_INPUT_SRC_TUNER_2PART) {
		*major = msg->msg[8];
		*input = (msg->msg[9] << 8) | msg->msg[10];
	} else {
		*major = 0;
		*input = (msg->msg[8] << 8) | msg->msg[9];
	}
}

static inline void cec_msg_htng_set_init_wakeup_vol(struct cec_msg *msg,
						    __u8 vol,
						    __u8 minutes)
{
	cec_msg_htng_init(msg, CEC_MSG_HTNG_SET_INIT_WAKEUP_VOL);
	msg->msg[msg->len++] = vol;
	msg->msg[msg->len++] = minutes;
}

static inline void cec_ops_htng_set_init_wakeup_vol(const struct cec_msg *msg,
						    __u8 *vol,
						    __u8 *minutes)
{
	*vol = msg->msg[6];
	*minutes = msg->msg[7];
}

static inline void cec_msg_htng_clr_all_sleep_wake(struct cec_msg *msg)
{
	cec_msg_htng_init(msg, CEC_MSG_HTNG_CLR_ALL_SLEEP_WAKE);
}

static inline void cec_msg_htng_global_direct_tune_freq(struct cec_msg *msg,
					       __u8 htng_chan_type,
					       __u8 htng_prog_type,
					       __u8 htng_system_type,
					       __u16 freq,
					       __u16 service_id,
					       __u8 htng_mod_type,
					       __u8 htng_symbol_rate,
					       __u16 symbol_rate)
{
	cec_msg_htng_init(msg, CEC_MSG_HTNG_GLOBAL_DIRECT_TUNE_FREQ);
	msg->msg[msg->len++] = (htng_chan_type & 7) |
			       ((htng_prog_type & 1) << 3) |
			       (htng_system_type << 4);
	msg->msg[msg->len++] = freq >> 8;
	msg->msg[msg->len++] = freq & 0xff;
	msg->msg[msg->len++] = service_id >> 8;
	msg->msg[msg->len++] = service_id & 0xff;
	msg->msg[msg->len++] = (htng_mod_type & 0xf) |
			       ((htng_symbol_rate & 1) << 7);
	if (htng_symbol_rate == CEC_OP_HTNG_SYMBOL_RATE_MANUAL) {
		msg->msg[msg->len++] = symbol_rate >> 8;
		msg->msg[msg->len++] = symbol_rate & 0xff;
	}
}

static inline void cec_ops_htng_global_direct_tune_freq(const struct cec_msg *msg,
					__u8 *htng_chan_type,
					__u8 *htng_prog_type,
					__u8 *htng_system_type,
					__u16 *freq,
					__u16 *service_id,
					__u8 *htng_mod_type,
					__u8 *htng_symbol_rate,
					__u16 *symbol_rate)
{
	*htng_chan_type = msg->msg[6] & 7;
	*htng_prog_type = (msg->msg[6] >> 3) & 1;
	*htng_system_type = (msg->msg[6] >> 4) & 0xf;
	*freq = (msg->msg[7] << 8) | msg->msg[8];
	*service_id = (msg->msg[9] << 8) | msg->msg[10];
	*htng_mod_type = msg->msg[11] & 0xf;
	*htng_symbol_rate = (msg->msg[11] >> 7) & 1;
	if (*htng_symbol_rate == CEC_OP_HTNG_SYMBOL_RATE_MANUAL)
		*symbol_rate = (msg->msg[12] << 8) | msg->msg[13];
	else
		*symbol_rate = 0;
}

static inline void cec_msg_htng_global_direct_tune_chan(struct cec_msg *msg,
					       __u8 htng_chan_type,
					       __u8 htng_prog_type,
					       __u16 chan)
{
	cec_msg_htng_init(msg, CEC_MSG_HTNG_GLOBAL_DIRECT_TUNE_CHAN);
	msg->msg[msg->len++] = (htng_chan_type & 7) |
			       ((htng_prog_type & 1) << 3);
	msg->msg[msg->len++] = chan >> 8;
	msg->msg[msg->len++] = chan & 0xff;
}

static inline void cec_ops_htng_global_direct_tune_chan(const struct cec_msg *msg,
					__u8 *htng_chan_type,
					__u8 *htng_prog_type,
					__u16 *chan)
{
	*htng_chan_type = msg->msg[6] & 7;
	*htng_prog_type = (msg->msg[6] >> 3) & 1;
	*chan = (msg->msg[7] << 8) | msg->msg[8];
}

static inline void cec_msg_htng_global_direct_tune_ext_freq(struct cec_msg *msg,
					       __u8 htng_ext_chan_type,
					       __u8 htng_prog_type,
					       __u8 htng_system_type,
					       __u16 freq,
					       __u16 service_id,
					       __u8 htng_mod_type,
					       __u8 htng_onid,
					       __u16 onid,
					       __u8 htng_nid,
					       __u16 nid,
					       __u8 htng_tsid_plp,
					       __u16 tsid_plp,
					       __u8 htng_symbol_rate,
					       __u16 symbol_rate)
{
	cec_msg_htng_init(msg, CEC_MSG_HTNG_GLOBAL_DIRECT_TUNE_EXT_FREQ);
	msg->msg[msg->len++] = (htng_ext_chan_type & 7) |
			       ((htng_prog_type & 1) << 3) |
			       (htng_system_type << 4);
	msg->msg[msg->len++] = freq >> 8;
	msg->msg[msg->len++] = freq & 0xff;
	msg->msg[msg->len++] = service_id >> 8;
	msg->msg[msg->len++] = service_id & 0xff;
	msg->msg[msg->len++] = (htng_mod_type & 0xf) |
			       ((htng_onid & 1) << 4) |
			       ((htng_nid & 1) << 5) |
			       ((htng_tsid_plp & 1) << 6) |
			       ((htng_symbol_rate & 1) << 7);
	if (htng_onid == CEC_OP_HTNG_ONID_MANUAL) {
		msg->msg[msg->len++] = onid >> 8;
		msg->msg[msg->len++] = onid & 0xff;
	}
	if (htng_nid == CEC_OP_HTNG_NID_MANUAL) {
		msg->msg[msg->len++] = nid >> 8;
		msg->msg[msg->len++] = nid & 0xff;
	}
	if (msg->len < CEC_MAX_MSG_SIZE &&
	    htng_tsid_plp == CEC_OP_HTNG_TSID_PLP_MANUAL) {
		msg->msg[msg->len++] = tsid_plp >> 8;
		msg->msg[msg->len++] = tsid_plp & 0xff;
	}
	if (msg->len < CEC_MAX_MSG_SIZE &&
	    htng_symbol_rate == CEC_OP_HTNG_SYMBOL_RATE_MANUAL) {
		msg->msg[msg->len++] = symbol_rate >> 8;
		msg->msg[msg->len++] = symbol_rate & 0xff;
	}
}

static inline void cec_ops_htng_global_direct_tune_ext_freq(const struct cec_msg *msg,
					       __u8 *htng_ext_chan_type,
					       __u8 *htng_prog_type,
					       __u8 *htng_system_type,
					       __u16 *freq,
					       __u16 *service_id,
					       __u8 *htng_mod_type,
					       __u8 *htng_onid,
					       __u16 *onid,
					       __u8 *htng_nid,
					       __u16 *nid,
					       __u8 *htng_tsid_plp,
					       __u16 *tsid_plp,
					       __u8 *htng_symbol_rate,
					       __u16 *symbol_rate)
{
	unsigned offset = 12;

	*htng_ext_chan_type = msg->msg[6] & 7;
	*htng_prog_type = (msg->msg[6] >> 3) & 1;
	*htng_system_type = (msg->msg[6] >> 4) & 0xf;
	*freq = (msg->msg[7] << 8) | msg->msg[8];
	*service_id = (msg->msg[9] << 8) | msg->msg[10];
	*htng_mod_type = msg->msg[11] & 0xf;
	*htng_onid = (msg->msg[11] >> 4) & 1;
	*htng_nid = (msg->msg[11] >> 5) & 1;
	*htng_tsid_plp = (msg->msg[11] >> 6) & 1;
	*htng_symbol_rate = (msg->msg[11] >> 7) & 1;
	if (*htng_onid == CEC_OP_HTNG_ONID_MANUAL) {
		*onid = (msg->msg[offset] << 8) | msg->msg[offset + 1];
		offset += 2;
	} else {
		*onid = 0;
	}
	if (*htng_nid == CEC_OP_HTNG_NID_MANUAL) {
		*nid = (msg->msg[offset] << 8) | msg->msg[offset + 1];
		offset += 2;
	} else {
		*nid = 0;
	}
	if (offset < CEC_MAX_MSG_SIZE &&
	    *htng_tsid_plp == CEC_OP_HTNG_TSID_PLP_MANUAL) {
		*tsid_plp = (msg->msg[offset] << 8) | msg->msg[offset + 1];
		offset += 2;
	} else {
		*tsid_plp = 0;
	}
	if (offset < CEC_MAX_MSG_SIZE &&
	    *htng_symbol_rate == CEC_OP_HTNG_SYMBOL_RATE_MANUAL) {
		*symbol_rate = (msg->msg[offset] << 8) | msg->msg[offset + 1];
		offset += 2;
	} else {
		*symbol_rate = 0;
	}
}

#endif
