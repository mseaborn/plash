#!/bin/bash

set -e

mkdir -p lib
./src/install-libs.pl --dest-dir lib/

(cd python &&
 mkdir -p lib &&
 python setup.py install --install-platlib=lib)

./run-uninstalled.sh sh -c 'cd tests && ./run-tests.pl'
