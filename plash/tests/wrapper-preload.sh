#!/bin/sh

# This script is for use with the PLASH_SANDBOX_PROG environment variable

export LD_PRELOAD=$PLASH_DIR/shobj/preload-libc.so

exec "$@"
