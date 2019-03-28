#!/bin/bash

#
# Copyright (c) 2019 by Mauro Carvalho Chehab <mchehab@kernel+samsung.org>
#
# Licensed under the terms of the GNU GPL License version 2
#

#
# Script to test M2M transform drivers like vim2m and GStreamer v4l2 plugin
#
# NOTE:
#
# 1. This script assumes that vim2m driver is loaded
# 2. GStreamer (up to version 1.14.4) silently ignores M2M convert devices
#    that supports Bayer. That includes newer versions of vim2m driver.
#    To use GStreamer with older versions, this patch requires backporting:
#	https://gitlab.freedesktop.org/gstreamer/gst-plugins-good/commit/dc7bd483260d6a5299f78a312740a74c7b3d7994
# 3. GStreamer also usually just passes data through if capture format is
#	identical to output format. Since version 1.14, this can be disabled.
#	This script disables it, on such versions.
# 4. Right now, GStreamer's v4l2 convert plugin is not capable of negotiating
#	Bayer formats. So, by default, it tests only YUY2 format. If you want
#	to test bayer as well, there's an experimental patch available at:
#	https://gitlab.freedesktop.org/mchehab_kernel/gst-plugins-good/commit/9dd48f551c4b8c8c841b32f61658b1f88ef08995

#
# Default values
#
HOST=127.0.0.1
PORT=8632
VDEV=""
CAPFMT=""
OUTFMT=RGB
CAPWIDTH=340
OUTWIDTH=320
CAPHEIGHT=240
OUTHEIGHT=200
COUNT=120
HELP=""

#
# Parse command line arguments
#
while [ "$1" != "" ]; do
	case $1 in
	--help)
		HELP=1
		;;
	--host)
		shift
		HOST="$1"
		;;
	--port)
		shift
		PORT="$1"
		;;
	--vdev)
		shift
		VDEV="$1"
		;;
	--count)
		shift
		COUNT="$1"
		;;
	--capfmt)
		shift
		CAPFMT="$CAPFMT $1"
		;;
	--outfmt)
		shift
		OUTFMT="$1"
		;;
	--capwidth)
		shift
		CAPWIDTH="$1"
		;;
	--outwidth)
		shift
		OUTWIDTH="$1"
		;;
	--capheight)
		shift
		CAPHEIGHT="$1"
		;;
	--outheight)
		shift
		OUTHEIGHT="$1"
		;;

	*)
		echo "Unknown argument '$1'"
		HELP=1
		;;

	esac
	shift
done

if [ "$HELP" != "" ]; then
	echo "$0 [--help] [--vdev videodev] [--host host_name_or_ip] [--port tcp_port]"
	echo "		[--capfmt capture_format] [--outfmt output_format]"
	echo "		[--capwidth capture_width] [--capheight capture_height]"
	echo "		[--capwidth output_width] [--capheight output_height]"
	exit -1
fi

# At Gstreamer 1.15 (unstable), the first m2m convert device receives a
# nice name: v4l2convert. The other ones use the same name convention as
# before

GST_VER=$(gst-launch-1.0 --version|grep "gst-launch-1.0 version"|cut -d' ' -f 3)
GST_SUBVER=$(echo $GST_VER|cut -d'.' -f2)
if [ $GST_SUBVER -lt 15 ]; then
	if [ "$VDEV" == "" ]; then
		VDEV="video0"
	fi
fi

M2M="v4l2${VDEV}convert"
if [ "$(gst-inspect-1.0 $M2M 2>/dev/null)" == "" ]; then
	echo "GStreamer doesn't recognized $M2M."
	echo "Don't forget to load vim2m."
	exit 1
fi

SUP_CAPFMTS=$(gst-inspect-1.0 $M2M 2>/dev/null|perl -e 'while (<>) { $T="$1" if (m/(SRC|SINK)\s+template/);  $T="" if (m/^\S/); $f{$T}.= $1 if ($T ne "" && m/format:\s+(.*)/); }; $f{SRC}=~s/[\{\}\,]//g; $f{SRC}=~s/\(string\)//g; print $f{SRC}')

# TODO: validate the output format qas well
SUP_OUTFMTS=$(gst-inspect-1.0 $M2M 2>/dev/null|perl -e 'while (<>) { $T="$1" if (m/(SRC|SINK)\s+template/); $T="" if (m/^\S/); $f{$T}.= $1 if ($T ne "" && m/format:\s+(.*)/); }; $f{SINK}=~s/[\{\}\,]//g; $f{SINK}=~s/\(string\)//g; print $f{SINK}')


# If input and output formats are identical, we need to explicitly disable
# passthrough mode.
# Unfortunately, such feature may not be available

HAS_DISABLE_PASSTHROUGH=""
if [ "$(gst-inspect-1.0 $M2M|grep disable-passthrough)" != "" ]; then
	M2M="$M2M disable-passthrough=1"
	HAS_DISABLE_PASSTHROUGH="1"
fi

#
# For each selected capture format, call Gst
#
# In order for the user to check if passthrough mode is disabled, do both
# horizontal and vertical flip
#
BAYERFMTS="bggr gbrg grbg rggb"

# Check it bayer is supported
HAS_BAYER=""
if [ "$(gst-inspect-1.0 bayer2rgb 2>/dev/null)" != "" ]; then
	HAS_BAYER=1
fi

# Select a default that would test more formats, as possible by GStreamer
# version and compilation

if [ "$CAPFMT" == "" ]; then
	if [ "$HAS_BAYER" == "" ]; then
		DISABLE_FMTS="$BAYERFMTS $DISABLE_FMTS"
	fi
	if [ "$HAS_DISABLE_PASSTHROUGH" == "" ]; then
		DISABLE_FMTS="$BAYERFMTS $OUTFMT"
	fi

	CAPFMT=" $SUP_CAPFMTS "
	for i in $DISABLE_FMTS; do
		CAPFMT=${CAPFMT/ $i / }
	done
else
	if [ "$HAS_DISABLE_PASSTHROUGH" == "" ]; then
		DISABLE_FMTS="$OUTFMT"
		echo "Can't use capture format $OUTFMT"
	fi

	CAPFMT=" $SUP_CAPFMTS "
	for i in $DISABLE_FMTS; do
		CAPFMT=${CAPFMT/ $i / }
	done
fi

#
# Displays all used command line parameters and default values
#
CAPFMT=${CAPFMT//  / }
SUP_CAPFMTS=${SUP_CAPFMTS//  / }
SUP_OUTFMTS=${SUP_OUTFMTS//  / }

echo "Using ${M2M}, source ${OUTWIDTH}x${OUTHEIGHT}, sink ${CAPWIDTH}x${CAPHEIGHT}".
echo "Supported output formats :$SUP_OUTFMTS (using $OUTFMT)"
echo "Supported capture formats:$SUP_CAPFMTS"
echo "Testing those cap formats:$CAPFMT"
echo "Sending stream to ${HOST}:${PORT}"
echo
echo "Be sure that port ${PORT} is enabled at the ${HOST} host."
echo "At ${HOST} host, you should run:"
echo
echo "  $ while :; do gst-launch-1.0 tcpserversrc port=${PORT} host=0.0.0.0 ! decodebin ! videoconvert ! autovideosink; done"

for FMT in $CAPFMT; do
	echo

	VIDEOFMT="video/x-raw,format=${FMT},width=${CAPWIDTH},height=${CAPHEIGHT} "
	NOT_SUPPORTED=""
	for i in $BAYERFMTS; do
		if [ "$FMT" == "$i" ]; then
			if [ "$HAS_BAYER" == "" ]; then
				NOT_SUPPORTED=1
			fi

			VIDEOFMT="video/x-bayer,format=${FMT},width=${CAPWIDTH},height=${CAPHEIGHT} ! bayer2rgb "
			break
		fi
	done
	if [ "$NOT_SUPPORTED" != "" ]; then
		echo "Bayer formats like $FMT aren't supported by GStreamer"
		continue
	fi

	NOT_SUPPORTED="1"
	for i in $SUP_CAPFMTS; do
		if [ "$FMT" == "$i" ]; then
			NOT_SUPPORTED=""
			break
		fi
	done
	if [ "$NOT_SUPPORTED" != "" ]; then
		echo "Format $FMT not supported by GStreamer"
		continue
	fi

	echo "Format $FMT";

	gst-launch-1.0 -q \
	  videotestsrc num-buffers=${COUNT} ! \
	  video/x-raw,format=${OUTFMT},width=${OUTWIDTH},height=${OUTHEIGHT} ! \
	  ${M2M} extra-controls="s,horizontal_flip=1,vertical_flip=1" ! \
	  ${VIDEOFMT} ! videoconvert ! \
	  jpegenc ! tcpclientsink host=${HOST} port=${PORT}

	sleep 3
done
