#!/bin/bash

set -e

(cd python &&
 mkdir -p lib &&
 python setup.py install --install-platlib=lib)

./run-uninstalled.sh sh -c "cd tests && ./run-tests.pl $@"
