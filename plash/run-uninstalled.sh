#!/bin/bash

set -e

if [ $# = 0 ]; then
  echo "Usage: $0 prog args..."
  exit 1
fi


DIR=`pwd`

source set-env.sh

# Create wrapper script for pola-run that sets the library path, etc.
(
 echo "#!/bin/sh"
 echo "exec $DIR/bin/pola-run -fl \$PLASH_LIBRARY_DIR \"\$@\""
) > lib/pola-run-c
chmod +x lib/pola-run-c

# Wrapper for Python pola-run
(
 echo "#!/bin/sh"
 echo "exec $DIR/python/scripts/pola-run -fl \$PLASH_LIBRARY_DIR \"\$@\""
) > lib/pola-run
chmod +x lib/pola-run

export PLASH_SANDBOX_PROG=$DIR/tests/wrapper.sh
export PLASH_P_SANDBOX_PROG=$DIR/shobj/ld.so
export PLASH_LDSO_PATH=$DIR/shobj/ld.so
export PLASH_LIBRARY_DIR=$DIR/lib

export PATH=$DIR/lib:$DIR/bin:$PATH
export PYTHONPATH=$DIR/python:$DIR/python/lib:$PYTHONPATH

exec "$@"
