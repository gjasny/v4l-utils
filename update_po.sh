#!/bin/bash

if [ ! -d "build" ]; then
    meson setup build
fi

( cd build && \
 ( meson compile v4l-utils-update-po;
   meson compile libdvbv5-update-po; )
)
