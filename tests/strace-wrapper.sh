#!/bin/sh

# This script is for use with pola-run's --sandbox-prog option

# -f: follow forks
# -c: count syscalls
#strace -fc \
#  -E LD_LIBRARY_PATH=/usr/lib/plash/lib \
#  /var/lib/plash-chroot-jail/special/ld-linux.so.2 "$@"

strace -fc \
  env LD_LIBRARY_PATH=$PLASH_DIR/lib \
  $PLASH_DIR/shobj/ld.so "$@"
