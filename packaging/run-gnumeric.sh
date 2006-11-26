#!/bin/sh

echo 'mrs:x:502:502:,,,:/home/mrs:/bin/bash' > unpacked-gnumeric/etc/passwd

DIR=gnumeric
mkdir $DIR/home
mkdir $DIR/tmp

exec ~/projects/plash/bin/pola-run \
  --env LD_LIBRARY_PATH=/usr/X11R6/lib \
  -f /proc --x11 \
  -f /usr/lib/plash/lib/ \
  -tw / unpacked-gnumeric/ \
  -tw ~ $DIR/home/ \
  -tw /tmp/ $DIR/tmp/ \
  -fw /dev/log \
  -fw /dev/null \
  -f /dev/random \
  -f /dev/urandom \
  -f /usr/lib/gconv -f /etc/gtk-2.0 -f /etc/pango -f /etc "$@" \
  -e /usr/bin/gnumeric
