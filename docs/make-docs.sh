#!/bin/sh

ARGS="--center Plash"

pod2man $ARGS docs/plash.pod docs/plash.1
pod2man $ARGS docs/plash-opts.pod docs/plash-opts.1
pod2man $ARGS docs/plash-chroot.pod docs/plash-chroot.1
pod2man $ARGS docs/exec-object.pod docs/exec-object.1
