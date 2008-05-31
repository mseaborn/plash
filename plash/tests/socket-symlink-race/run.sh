#!/bin/bash

rm -fv socket race-socket tmp-symlink

../../bin/pola-run -B -f listen    --prog ./listen    -fw socket & PID1=$!

# ../../bin/pola-run -B -f symlinker --prog ./symlinker -fw race-socket -fw tmp-symlink & PID2=$!
../../bin/pola-run -B -f symlinker --prog ./symlinker -fw . & PID2=$!
# ./symlinker & PID2=$!

# Using "nice" doesn't seem to make much difference
nice ../../bin/pola-run -B -f connect   --prog ./connect   -fw race-socket & PID3=$!
echo $PID1 $PID2 $PID3

while true; do sleep 10s; done
