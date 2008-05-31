
# Usage: source set-env.sh

# Set up environment variables so that Plash can be tested in-place
# before installing.

export PLASH_BUILD_DIR=$(pwd)
export PATH=$PLASH_BUILD_DIR/python/scripts:$PATH
export PYTHONPATH=$PLASH_BUILD_DIR/python:$PYTHONPATH
export PLASH_LIBRARY_DIR=$PLASH_BUILD_DIR/lib
