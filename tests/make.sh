#!/bin/sh

gcc -Wall thread-open.c -o thread-open -lpthread
gcc -Wall thread-dirlist.c -o thread-dirlist -lpthread
gcc -Wall segfault-open.c -o segfault-open
