#!/bin/sh

# Requires libxtst-dev

set -e

mkdir -p bin

OPTS="-Wall -L/usr/X11R6/lib -lX11"

gcc $OPTS key-injector.c -o bin/key-injector
gcc $OPTS key-injector-xtest.c -o bin/key-injector-xtest -lXtst
gcc $OPTS key-sniffer.c -o bin/key-sniffer
gcc $OPTS grab-server.c -o bin/grab-server
gcc $OPTS grab-keyboard.c -o bin/grab-keyboard
gcc $OPTS grab-pointer.c -o bin/grab-pointer
gcc $OPTS background-none.c -o bin/background-none
gcc $OPTS warp-pointer.c -lm -o bin/warp-pointer
gcc $OPTS steal-focus.c -o bin/steal-focus
gcc $OPTS screen-grab.c -o bin/screen-grab
