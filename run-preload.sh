#!/bin/sh

if [ $# = 0 ]; then
  echo "Usage: $0 prog args..."
  exit 1
fi


DIR=`pwd`

# Create wrapper script for pola-run that sets the library path, etc.
(
 echo "#!/bin/sh"
 echo "exec $DIR/bin/pola-run \"\$@\""
) > lib/pola-run
chmod +x lib/pola-run

export PLASH_SANDBOX_PROG=$DIR/tests/wrapper-preload.sh
export PLASH_P_SANDBOX_PROG=$DIR/tests/wrapper-preload.sh
export PLASH_LDSO_PATH=$DIR/bin/kernel-exec
export PLASH_DIR=$DIR

export PATH=$DIR/lib:$DIR/bin:$PATH
export PYTHONPATH=$DIR/python:$DIR/python/lib:$PYTHONPATH

exec "$@"
