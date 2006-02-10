#!/bin/sh

gcc run-as-nobody.c -o run-as-nobody
gcc run-as-nobody+chroot.c -o run-as-nobody+chroot

chown root:root run-as-nobody
chmod +s run-as-nobody
chown root:root run-as-nobody+chroot
chmod +s run-as-nobody+chroot

# mkdir /usr/local/chroot-jail
# mkdir /usr/local/chroot-jail/special
# chown mrs: /usr/local/chroot-jail/special
# mkdir /usr/local/mrs
# chown mrs: /usr/local/mrs
