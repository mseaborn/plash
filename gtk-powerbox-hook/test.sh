#!/bin/bash

set -e

gcc $(pkg-config gtk+-2.0 --cflags --libs) \
	test-gtk-powerbox.c -o test-gtk-powerbox

LD_PRELOAD=.libs/libgtk-powerbox-hook.so xvfb-run -a python powerbox_test.py
