#!/usr/bin/perl

#   Copyright (C) 2008 Mauro Carvalho Chehab <mchehab@redhat.com>
#
#   This program is free software; you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation, version 2 of the License.
#
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
# FIXME: need to properly map 32 bites aligned read/writes also.
#

use strict;

my $debug = 0;

my %reg_map = (
	0x101 => "SAA7134_INCR_DELAY",
	0x102 => "SAA7134_ANALOG_IN_CTRL1",
	0x103 => "SAA7134_ANALOG_IN_CTRL2",
	0x104 => "SAA7134_ANALOG_IN_CTRL3",
	0x105 => "SAA7134_ANALOG_IN_CTRL4",
	0x106 => "SAA7134_HSYNC_START",
	0x107 => "SAA7134_HSYNC_STOP",
	0x108 => "SAA7134_SYNC_CTRL",
	0x109 => "SAA7134_LUMA_CTRL",
	0x10a => "SAA7134_DEC_LUMA_BRIGHT",
	0x10b => "SAA7134_DEC_LUMA_CONTRAST",
	0x10c => "SAA7134_DEC_CHROMA_SATURATION",
	0x10d => "SAA7134_DEC_CHROMA_HUE",
	0x10e => "SAA7134_CHROMA_CTRL1",
	0x10f => "SAA7134_CHROMA_GAIN",
	0x110 => "SAA7134_CHROMA_CTRL2",
	0x111 => "SAA7134_MODE_DELAY_CTRL",
	0x114 => "SAA7134_ANALOG_ADC",
	0x115 => "SAA7134_VGATE_START",
	0x116 => "SAA7134_VGATE_STOP",
	0x117 => "SAA7134_MISC_VGATE_MSB",
	0x118 => "SAA7134_RAW_DATA_GAIN",
	0x119 => "SAA7134_RAW_DATA_OFFSET",
	0x11e => "SAA7134_STATUS_VIDEO1",
	0x11f => "SAA7134_STATUS_VIDEO2",
	0x300 => "SAA7134_OFMT_VIDEO_A",
	0x301 => "SAA7134_OFMT_DATA_A",
	0x302 => "SAA7134_OFMT_VIDEO_B",
	0x303 => "SAA7134_OFMT_DATA_B",
	0x304 => "SAA7134_ALPHA_NOCLIP",
	0x305 => "SAA7134_ALPHA_CLIP",
	0x308 => "SAA7134_UV_PIXEL",
	0x309 => "SAA7134_CLIP_RED",
	0x30a => "SAA7134_CLIP_GREEN",
	0x30b => "SAA7134_CLIP_BLUE",
	0x180 => "SAA7134_I2C_ATTR_STATUS",
	0x181 => "SAA7134_I2C_DATA",
	0x182 => "SAA7134_I2C_CLOCK_SELECT",
	0x183 => "SAA7134_I2C_TIMER",
	0x140 => "SAA7134_NICAM_ADD_DATA1",
	0x141 => "SAA7134_NICAM_ADD_DATA2",
	0x142 => "SAA7134_NICAM_STATUS",
	0x143 => "SAA7134_AUDIO_STATUS",
	0x144 => "SAA7134_NICAM_ERROR_COUNT",
	0x145 => "SAA7134_IDENT_SIF",
	0x146 => "SAA7134_LEVEL_READOUT1",
	0x147 => "SAA7134_LEVEL_READOUT2",
	0x148 => "SAA7134_NICAM_ERROR_LOW",
	0x149 => "SAA7134_NICAM_ERROR_HIGH",
	0x14a => "SAA7134_DCXO_IDENT_CTRL",
	0x14b => "SAA7134_DEMODULATOR",
	0x14c => "SAA7134_AGC_GAIN_SELECT",
	0x150 => "SAA7134_CARRIER1_FREQ0",
	0x151 => "SAA7134_CARRIER1_FREQ1",
	0x152 => "SAA7134_CARRIER1_FREQ2",
	0x154 => "SAA7134_CARRIER2_FREQ0",
	0x155 => "SAA7134_CARRIER2_FREQ1",
	0x156 => "SAA7134_CARRIER2_FREQ2",
	0x158 => "SAA7134_NUM_SAMPLES0",
	0x159 => "SAA7134_NUM_SAMPLES1",
	0x15a => "SAA7134_NUM_SAMPLES2",
	0x15b => "SAA7134_AUDIO_FORMAT_CTRL",
	0x160 => "SAA7134_MONITOR_SELECT",
	0x161 => "SAA7134_FM_DEEMPHASIS",
	0x162 => "SAA7134_FM_DEMATRIX",
	0x163 => "SAA7134_CHANNEL1_LEVEL",
	0x164 => "SAA7134_CHANNEL2_LEVEL",
	0x165 => "SAA7134_NICAM_CONFIG",
	0x166 => "SAA7134_NICAM_LEVEL_ADJUST",
	0x167 => "SAA7134_STEREO_DAC_OUTPUT_SELECT",
	0x168 => "SAA7134_I2S_OUTPUT_FORMAT",
	0x169 => "SAA7134_I2S_OUTPUT_SELECT",
	0x16a => "SAA7134_I2S_OUTPUT_LEVEL",
	0x16b => "SAA7134_DSP_OUTPUT_SELECT",
	0x16c => "SAA7134_AUDIO_MUTE_CTRL",
	0x16d => "SAA7134_SIF_SAMPLE_FREQ",
	0x16e => "SAA7134_ANALOG_IO_SELECT",
	0x170 => "SAA7134_AUDIO_CLOCK0",
	0x171 => "SAA7134_AUDIO_CLOCK1",
	0x172 => "SAA7134_AUDIO_CLOCK2",
	0x173 => "SAA7134_AUDIO_PLL_CTRL",
	0x174 => "SAA7134_AUDIO_CLOCKS_PER_FIELD0",
	0x175 => "SAA7134_AUDIO_CLOCKS_PER_FIELD1",
	0x176 => "SAA7134_AUDIO_CLOCKS_PER_FIELD2",
	0x190 => "SAA7134_VIDEO_PORT_CTRL0",
	0x191 => "SAA7134_VIDEO_PORT_CTRL1",
	0x192 => "SAA7134_VIDEO_PORT_CTRL2",
	0x193 => "SAA7134_VIDEO_PORT_CTRL3",
	0x194 => "SAA7134_VIDEO_PORT_CTRL4",
	0x195 => "SAA7134_VIDEO_PORT_CTRL5",
	0x196 => "SAA7134_VIDEO_PORT_CTRL6",
	0x197 => "SAA7134_VIDEO_PORT_CTRL7",
	0x198 => "SAA7134_VIDEO_PORT_CTRL8",
	0x1a0 => "SAA7134_TS_PARALLEL",
	0x1a1 => "SAA7134_TS_PARALLEL_SERIAL",
	0x1a2 => "SAA7134_TS_SERIAL0",
	0x1a3 => "SAA7134_TS_SERIAL1",
	0x1a4 => "SAA7134_TS_DMA0",
	0x1a5 => "SAA7134_TS_DMA1",
	0x1a6 => "SAA7134_TS_DMA2",
	0x1B0 => "SAA7134_GPIO_GPMODE0",
	0x1B1 => "SAA7134_GPIO_GPMODE1",
	0x1B2 => "SAA7134_GPIO_GPMODE2",
	0x1B3 => "SAA7134_GPIO_GPMODE3",
	0x1B4 => "SAA7134_GPIO_GPSTATUS0",
	0x1B5 => "SAA7134_GPIO_GPSTATUS1",
	0x1B6 => "SAA7134_GPIO_GPSTATUS2",
	0x1B7 => "SAA7134_GPIO_GPSTATUS3",
	0x1c0 => "SAA7134_I2S_AUDIO_OUTPUT",
	0x1d0 => "SAA7134_SPECIAL_MODE",
	0x1d1 => "SAA7134_PRODUCTION_TEST_MODE",
	0x580 => "SAA7135_DSP_RWSTATE",
	0x586 => "SAA7135_DSP_RWCLEAR",
	0x591 => "SAA7133_I2S_AUDIO_CONTROL",
);

my %i2c_status = (
	0  => "IDLE",
	1  => "DONE_STOP",
	2  => "BUSY",
	3  => "TO_SCL",
	4  => "TO_ARB",
	5  => "DONE_WRITE",
	6  => "DONE_READ",
	7  => "DONE_WRITE_TO",
	8  => "DONE_READ_TO",
	9  => "NO_DEVICE",
	10 => "NO_ACKN",
	11 => "BUS_ERR",
	12 => "ARB_LOST",
	13 => "SEQ_ERR",
	14 => "ST_ERR",
	15 => "SW_ERR",
);

my %i2c_attr = (
	0 => "NOP",
	1 => "STOP",
	2 => "CONTINUE",
	3 => "START",
);

my $addr = -1;
my $write = 0;
my $direction;
my @buf;

sub flush_i2c_transaction($$)
{
	my $direction = shift;
	my $is_complete = shift;

	my $size = scalar(@buf);

	if ($direction == 0) {
		my $v = shift @buf;
		printf("write_i2c_addr(0x%02x, %d, { 0x%02x",
			$addr, $size, $v);
		while (scalar(@buf)) {
			my $v = shift @buf;
			printf(", 0x%02x", $v);
		}
		printf("});");
	} else {
		my $size = scalar(@buf);
		my $v = shift @buf;
		printf("read_i2c_addr(0x%02x, %d) /* 0x%02x",
			$addr, $size, $v);
		while (scalar(@buf)) {
			my $v = shift @buf;
			printf(", 0x%02x", $v);
		}
		printf(" */;");
	}
	@buf = ();
	$addr = -1;

	printf (" /* INCOMPLETE */") if (!$is_complete);
	printf "\n";
}
sub parse_i2c($$$$$)
{
	my $time = shift;
	my $optype = shift;
	my $align = shift;
	my $reg = shift;
	my $val = shift;
	my $discard = 0;

	if ($align ne 'l') {
		print("FIXME: currently, parser work only with 32 bits i2c reg $optype\n");
		return;
	}

	my $status = $i2c_status{$val & 0x0f};
	my $attr = $i2c_attr{($val >> 6) & 0x03};

	# Avoid poluting the logs with busy msgs
	$discard = 1 if ($status eq "BUSY" | $status eq "TO_SCL" | $status eq "TO_ARB");

	# Avoid poluting the logs with read msgs during write
	$discard = 1 if ($optype eq "read" && $status eq "DONE_WRITE");

	# Avoid poluting the logs with NOP operations
	$discard = 1 if ($attr eq "NOP");

	# Prints I2C raw transaction
	if ($debug >= 1) {
		return if ($debug == 1 && $discard);
		if ($optype eq "write") {
			printf("write_i2c(%s, %s, 0x%02x)\t/* %s */\n", $status, $attr, $val >> 8, $time);
		} else {
			printf("val = read_i2c()\t/* %s: read %s, %s, val=0x%02x */\n", $time, $status, $attr, $val >> 8);
		}
	}
	return if ($discard);
	$val >>= 8;

	if (($attr eq "START") && ($optype eq "write")) {
		flush_i2c_transaction(0, 0) if (scalar(@buf) && $addr >= 0);

		$direction = $val & 1;
		$addr = $val >> 1;
	}
	# Prints I2C transaction. This state machine is not 100%
	# it is known to fail with eeprom access
	if (($direction == 0) && ($optype eq "write")) {
		if ($attr eq "CONTINUE") {
			# Discard transactions on the wrong direction
			push @buf, $val;
		} elsif ($attr eq "STOP" && $addr >= 0) {
			flush_i2c_transaction(0, 1);
		}
	} elsif ($direction == 1) {
		if ($optype eq "write") {
			$write = 1;
			return;
		}
		return if (!$write);
		$write = 0;

		if ($attr eq "CONTINUE") {
			push @buf, $val;
		} elsif ($attr eq "STOP" && $addr >= 0) {
			flush_i2c_transaction(1, 1);
		}
	}
}

while (<>) {
	# 1286074716 slow_bar_read.: slow_bar_readl addr=0x0000000000000180 val=0x00008182
	if (m/^(\d+)\s+slow_bar_(read|write)(.).*addr=([^\s]+)\s+val=([^\s]+)/) {
		my $time = $1;
		my $optype = $2;
		my $align = $3;
		my $op = "$2$3";
		my $reg = hex($4);
		my $val = hex($5);
		if (defined($reg_map{$reg})) {
			$reg = $reg_map{$reg};
		} else {
			$reg = sprintf("0x%04x", $reg);
		}
		if ($reg =~ m/SAA7134_I2C_ATTR_STATUS/) {
			parse_i2c($time, $optype, $align, $reg, $val);
		} elsif ($optype eq "read") {
			printf ("reg = saa_%s(%s);	/* %s: read 0x%04x */\n", $op, $reg, $time, $val);
		} else {
			printf ("saa_%s(%s, 0x%04x);	/* %s */\n", $op, $reg, $val, $time);
		}
	}
}
