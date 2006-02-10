#!/bin/sh

echo $#
if [ "$#" -ne 1 ]; then
  echo "Usage: $0 <dest-dir>"
  exit 1
fi
DEST=$1

# Install executables
STRIP_ARGS="--remove-section=.comment --remove-section=.note"
# cp -v bin/plash           $DEST/usr/bin/
# cp -v bin/plash-chroot    $DEST/usr/bin/
# cp -v bin/plash-opts      $DEST/usr/bin/
# cp -v bin/plash-opts-gtk  $DEST/usr/bin/
# cp -v bin/exec-object     $DEST/usr/bin/
strip $STRIP_ARGS bin/plash           -o $DEST/usr/bin/plash
strip $STRIP_ARGS bin/plash-chroot    -o $DEST/usr/bin/plash-chroot
strip $STRIP_ARGS bin/plash-opts      -o $DEST/usr/bin/plash-opts
strip $STRIP_ARGS bin/plash-opts-gtk  -o $DEST/usr/bin/plash-opts-gtk
strip $STRIP_ARGS bin/exec-object     -o $DEST/usr/bin/exec-object
strip $STRIP_ARGS bin/socket-connect  -o $DEST/usr/bin/plash-socket-connect
strip $STRIP_ARGS bin/socket-publish  -o $DEST/usr/bin/plash-socket-publish
strip $STRIP_ARGS bin/run-emacs       -o $DEST/usr/bin/plash-run-emacs
strip $STRIP_ARGS bin/pola-run        -o $DEST/usr/bin/pola-run
