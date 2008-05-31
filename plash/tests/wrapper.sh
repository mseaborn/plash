#!/bin/sh

# This script is for use with the PLASH_SANDBOX_PROG environment variable

if test "$LD_LIBRARY_PATH" = ""; then
  export LD_LIBRARY_PATH=$PLASH_LIBRARY_DIR
else
  export LD_LIBRARY_PATH=$PLASH_LIBRARY_DIR:$LD_LIBRARY_PATH
fi

exec "$PLASH_LDSO_PATH" "$@"
