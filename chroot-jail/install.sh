#!/bin/bash

set -e

JAIL_DIR=/var/lib/plash-chroot-jail
UID_LOCK_DIR=$JAIL_DIR/plash-uid-locks
PLASH_SETUID_BIN_INSTALL=/usr/lib/plash

DEST=debian/plash-chroot-jail


if [ "`id -u`" != 0 ]; then
  echo This script needs to be run as root to make executables setuid root
  exit 1
fi

# Create chroot jail:  including "special" and "plash-uid-locks" dirs
install -d $DEST/$JAIL_DIR
install -d $DEST/$UID_LOCK_DIR
# Create empty lock file
> $DEST/$UID_LOCK_DIR/flock-file

# Set permissions
chmod go-rwx $DEST/$UID_LOCK_DIR

# Install executables into /usr/lib/plash
install -d $DEST/$PLASH_SETUID_BIN_INSTALL
install setuid/run-as-anonymous \
        $DEST/$PLASH_SETUID_BIN_INSTALL/run-as-anonymous
install setuid/gc-uid-locks \
        $DEST/$PLASH_SETUID_BIN_INSTALL/gc-uid-locks
install setuid/run-as-anonymous_static \
        $DEST/$JAIL_DIR/run-as-anonymous
chmod +s $DEST/$PLASH_SETUID_BIN_INSTALL/run-as-anonymous
chmod +s $DEST/$PLASH_SETUID_BIN_INSTALL/gc-uid-locks
chmod +s $DEST/$JAIL_DIR/run-as-anonymous

install elf-chainloader/chainloader $DEST/$JAIL_DIR/chainloader
