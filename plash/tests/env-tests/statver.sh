#!/bin/sh

set -e

print_info ()
{
  echo
  echo "With flags: \"$@\""
  gcc statver.c "$@" -o statver
  ./statver
  nm statver | grep stat
}

print_info
print_info -DTRY_STAT64 -D_LARGEFILE64_SOURCE
print_info -D_FILE_OFFSET_BITS=64
