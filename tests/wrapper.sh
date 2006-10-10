#!/bin/sh

# This script is for use with the PLASH_SANDBOX_PROG environment variable

export LD_LIBRARY_PATH=$PLASH_LIBRARY_DIR:$LD_LIBRARY_PATH
exec "$PLASH_LDSO_PATH" "$@"
