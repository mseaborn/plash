#!/bin/bash

PACKAGE=leafpad

mkdir -p $PACKAGE/logs
mkdir -p $PACKAGE/unpacked/var/lib/dpkg
mkdir -p $PACKAGE/unpacked/var/lib/dpkg/info
# : > $PACKAGE/unpacked/var/lib/dpkg/status
# : > $PACKAGE/unpacked/var/lib/dpkg/available

SETTINGS="
      --env PATH=/sbin:/bin:/usr/sbin:/usr/bin:/usr/bin/X11:/usr/local/sbin:/usr/local/bin
      -f /usr/lib/plash/lib
      -f,objrw /dev/tty
      -f,objrw /dev/null
      -tw / $PACKAGE/unpacked
      -tw /tmp $PACKAGE/tmp"

for P in `cat $PACKAGE/package-list`; do
  if [ -e out-control/$P/preinst ]; then
    echo preinst $P
    pola-run $SETTINGS \
      -t /control out-control/$P \
      --cwd / \
      --log >$PACKAGE/logs/preinst_$P 2>&1 \
      -e /control/preinst install
  fi
done

for P in `cat $PACKAGE/package-list`; do
  if [ -e out-control/$P/postinst ]; then
    echo postinst $P
    pola-run $SETTINGS \
      -t /control out-control/$P \
      --cwd / \
      --log >$PACKAGE/logs/postinst_$P 2>&1 \
      -e /control/postinst configure
  fi
done
