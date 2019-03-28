#!/bin/bash

#
# Copyright (c) 2019 by Mauro Carvalho Chehab <mchehab@kernel+samsung.org>
#
# Licensed under the terms of the GNU GPL License version 2
#

#
# Script to test vim2m driver and Gstreamer v4l2 plugin
# NOTE: This script assumes that vim2m driver is loaded
#

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
echo "OLD GST"
	if [ "$VDEV" == "" ]; then
		VDEV="video0"
	fi
fi

M2M="v4l2${VDEV}convert"

# If input and output formats are identical, we need to explicitly disable
# passthrough mode. Unfortunately, this is possible only after Gst 1.12

if [ $GST_SUBVER -gt 12 ]; then
	M2M= "$M2M disable-passthrough=1"
fi

# Default is to output Using YUY2. As inputs are RGB, that warrants
# that passthrough mode will be disabled.

if [ "$CAPFMT" == "" ]; then
	CAPFMT="YUY2"
fi

# TODO: use those to check if passed parameters are valid

SUP_CAPFMTS=$(gst-inspect-1.0 $M2M 2>/dev/null|perl -e 'while (<>) { $T="$1" if (m/(SRC|SINK)\s+template/);  $T="" if (m/^\S/); $f{$T}.= $1 if ($T ne "" && m/format:\s+(.*)/); }; $f{SRC}=~s/[\{\}\,]//g; $f{SRC}=~s/\(string\)//g; print $f{SRC}')
SUP_OUTFMTS=$(gst-inspect-1.0 $M2M 2>/dev/null|perl -e 'while (<>) { $T="$1" if (m/(SRC|SINK)\s+template/); $T="" if (m/^\S/); $f{$T}.= $1 if ($T ne "" && m/format:\s+(.*)/); }; $f{SINK}=~s/[\{\}\,]//g; $f{SINK}=~s/\(string\)//g; print $f{SINK}')

#
# Displays all used command line parameters and default values
#
echo "Using ${M2M}, source $OUTFMT ${OUTWIDTH}x${OUTHEIGHT}, sink $CAPFMT ${CAPWIDTH}x${CAPHEIGHT}".
if [ "$SUP_CAPFMTS" != "" ]; then
	echo "Supported capture formats: $SUP_CAPFMTS"
fi
if [ "$SUP_OUTFMTS" != "" ]; then
	echo "Supported output formats: $SUP_OUTFMTS"
fi
echo "Sending stream to ${HOST}:${PORT}"
echo "Don't forget to load vim2m."
echo
echo "Be sure that port ${PORT} is enabled at the ${HOST} host."
echo "At ${HOST} host, you should run:"
echo
echo "  $ while :; do gst-launch-1.0 tcpserversrc port=${PORT} host=0.0.0.0 ! decodebin ! videoconvert ! autovideosink; done"

#
# For each selected capture format, call Gst
#
# In order for the user to check if passthrough mode is disabled, do both
# horizontal and vertical flip
#
BAYERFMTS="bggr gbrg grbg rggb"
for FMT in $CAPFMT; do
	echo
	echo "Format $FMT";

	VIDEOFMT="video/x-raw,format=${FMT},width=${CAPWIDTH},height=${CAPHEIGHT} "
	for i in $BAYERFMTS; do
	      if [ "$FMT" == "$i" ]; then
		VIDEOFMT="video/x-bayer,format=${FMT},width=${CAPWIDTH},height=${CAPHEIGHT} ! bayer2rgb "
	      fi
	done
	gst-launch-1.0 \
	  videotestsrc num-buffers=${COUNT} ! \
	  video/x-raw,format=${OUTFMT},width=${OUTWIDTH},height=${OUTHEIGHT} ! \
	  ${M2M} extra-controls="s,horizontal_flip=1,vertical_flip=1" ! \
	  ${VIDEOFMT} ! videoconvert ! \
	  jpegenc ! tcpclientsink host=${HOST} port=${PORT}

	sleep 3
done
