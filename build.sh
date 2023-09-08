#!/bin/sh
meson setup build
ninja -C build dotkeeper
