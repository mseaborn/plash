#!/bin/bash

# Runs a program with a modified environment.
# This will use the installed chroot and /usr/lib/plash/lib,
# but it uses pola-run and pola-shell from the build tree.

set -e

if [ $# = 0 ]; then
  echo "Usage: $0 prog args..."
  exit 1
fi


DIR=`pwd`

source set-env.sh

(
 echo "#!/bin/sh"
 echo "exec $DIR/bin/pola-run \"\$@\""
) > lib/pola-run-c
chmod +x lib/pola-run-c

(
 echo "#!/bin/sh"
 echo "exec $DIR/python/scripts/pola-run \"\$@\""
) > lib/pola-run
chmod +x lib/pola-run

export PATH=$DIR/lib:$DIR/bin:$PATH
export PYTHONPATH=$DIR/python:$DIR/python/lib:$PYTHONPATH

exec "$@"
