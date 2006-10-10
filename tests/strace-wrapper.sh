#!/bin/sh

# This script is for use with the PLASH_SANDBOX_PROG environment variable

# -f: follow forks
# -c: count syscalls
#strace -fc \
#  -E LD_LIBRARY_PATH=/usr/lib/plash/lib \
#  /var/lib/plash-chroot-jail/special/ld-linux.so.2 "$@"

strace -fc -o /dev/null \
  env LD_LIBRARY_PATH=$PLASH_LIBRARY_DIR \
  $PLASH_LDSO_PATH "$@"
