#!/bin/sh

OPTS="-Wall -L/usr/X11R6/lib -lX11"

gcc $OPTS key-injector.c -o key-injector
gcc $OPTS key-sniffer.c -o key-sniffer
gcc $OPTS grab-server.c -o grab-server 
gcc $OPTS background-none.c -o background-none
gcc $OPTS warp-pointer.c -lm -o warp-pointer
