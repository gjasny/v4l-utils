#!/bin/bash

#
# Copyright (c) 2019 by Mauro Carvalho Chehab <mchehab@kernel+samsung.org>
#
# Licensed under the terms of the GNU GPL License version 2
#

#
# Script to test vim2m driver with qvidcap
# NOTE: This script assumes that vim2m driver is loaded
#

#
# Default values
#
COUNT=45
unset CAPFMT
VDEV=video0
HOST=127.0.0.1
PORT=8632
OUTFMT=RGB3
CAPWIDTH=340
OUTWIDTH=320
CAPHEIGHT=240
OUTHEIGHT=200
unset HELP

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
	--capfmt)
		shift
		CAPFMT="${CAPFMT} $1"
		;;
	--outfmt)
		shift
		OUTFMT="$1"
		;;
	--count)
		shift
		COUNT="$1"
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
	echo "$0 [--help] [--vdev videodev] [--host host_name_or_ip] [--port tcp
_port] [--count num_frames]"
	echo "		[--capfmt capture_format]"
	echo "		[--capwidth capture_width] [--capheight capture_height]"
	exit -1
fi

#
# By default, if no capture format is specified, test all
#
if [ "$CAPFMT" == "" ]; then
	C=$(v4l2-ctl -d /dev/$VDEV --list-formats|grep "\["|cut -d"'" -f 2)
	for FMT in $C; do
		CAPFMT="${CAPFMT} $FMT"
	done
fi

#
# Displays all used command line parameters and default values
#
echo "Using /dev/${VDEV} ${CAPWIDTH}x${CAPHEIGHT}, ${COUNT} frames."
echo "Format(s)${CAPFMT}".
echo "Sending stream to ${HOST}:${PORT}"
echo
echo "Ensure that firewall is opened at ${HOST} host for port ${PORT}."
echo "Run this command at the $HOST host:"
echo "  $ while :; do qvidcap -p${PORT}; done"

#
# For each selected capture format, call v4l2-ctl to stream video
#
for FMT in $CAPFMT; do
	echo
	echo "Format $FMT";
	v4l2-ctl --stream-mmap --stream-out-mmap \
		--stream-to-host ${HOST}:${PORT} \
		--stream-lossless \
		--stream-out-hor-speed 1 \
		--set-fmt-video-out pixelformat=${OUTFMT},width=${OUTWIDTH},height=${OUTHEIGHT} \
		--set-fmt-video pixelformat=${FMT},width=${CAPWIDTH},height=${CAPHEIGHT} \
		-d /dev/${VDEV} --stream-count=${COUNT}
	echo
	sleep 3
done
