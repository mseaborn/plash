#!/bin/sh

# Quick hack for forcing rebuilding of Python-related code

rm build/temp.linux-i686-2.3/*.o
rm build/lib.linux-i686-2.3/plash.so
python setup.py build
