#!/bin/bash

set -e

./src/install-libs.pl --dest-dir lib/

(cd python &&
 mkdir -p lib &&
 python setup.py install --install-platlib=lib)

# Create wrapper script for pola-run that sets the library path, etc.
DIR=`pwd`
(
 echo "#!/bin/sh"
 echo "$DIR/bin/pola-run -fl \$PLASH_LIBRARY_DIR \"\$@\""
) > lib/pola-run
chmod +x lib/pola-run

export PLASH_SANDBOX_PROG=$DIR/tests/strace-wrapper.sh
export PLASH_P_SANDBOX_PROG=$DIR/shobj/ld.so
export PLASH_LDSO_PATH=$DIR/shobj/ld.so
export PLASH_LIBRARY_DIR=$DIR/lib

export PATH=$DIR/lib:$DIR/bin:$PATH
export PYTHONPATH=$DIR/python:$DIR/python/lib:$PYTHONPATH

cd tests
./run-tests.pl
