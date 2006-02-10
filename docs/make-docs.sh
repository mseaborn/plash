#!/bin/sh

if false; then
  ARGS="--center Plash"

  pod2man $ARGS docs/plash.pod docs/plash.1
  pod2man $ARGS docs/plash-opts.pod docs/plash-opts.1
  pod2man $ARGS docs/plash-chroot.pod docs/plash-chroot.1
  pod2man $ARGS docs/exec-object.pod docs/exec-object.1
fi


cd docs && (
./parse.pl index.xxml >out-doc.xml

mkdir -p html
time xmlto html -m config.xsl -o html/ out-doc.xml

mkdir -p man
(cd man && docbook2x-man ../out-doc.xml)
)
