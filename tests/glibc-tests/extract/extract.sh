#!/bin/bash

# Extract test cases from glibc.
# This involves unpacking glibc and applying a patch to the makefiles
# that lets us enumerate the test cases.

set -e

DIR=/tmp/glibc
GLIBC=glibc-2.3.5
SOURCE=$DIR/$GLIBC
GLIBC_ARCHIVE=$GLIBC.tar.bz2
DEST=dest


action_unpack ()
{
  echo unpacking $GLIBC in $DIR
  bzip2 -cd $GLIBC_ARCHIVE | tar -C $DIR -xf -
}

action_patch ()
{
  echo patching
  patch -p1 -d $SOURCE <glibc-2.3.5.patch
}

action_get_list ()
{
  mkdir -p build
  
  # Use --disable-sanity-checks because we've not included Linuxthreads
  (cd build && $SOURCE/configure --disable-sanity-checks)
  
  for SUBDIR in `cd $SOURCE && echo *`; do
    if [ -e $SOURCE/$SUBDIR/Makefile ] &&
       [ $SUBDIR != hurd ] &&
       [ $SUBDIR != manual ]
    then
      echo -- $SUBDIR
      mkdir -p build/$SUBDIR
      make -C $SOURCE/$SUBDIR \
        objdir=`pwd`/build \
        test-list test-list-params
    fi
  done
}

action_copy ()
{
  mkdir -p $DEST
  : > $DEST/test-list

  # Remove absolute pathnames
  filter ()
  {
    sed "s,`pwd`/$FROM,GLIBC_BUILD_DIR,"
  }
  
  for SUBDIR in `cd build && echo *`; do
    if [ -e build/$SUBDIR/test-list ]; then
      for TEST in `cat build/$SUBDIR/test-list`; do
        if [ -e "$SOURCE/$SUBDIR/$TEST.c" ]; then
          echo "copying $SUBDIR/$TEST"

          mkdir -p $DEST/$SUBDIR
          cp -a $SOURCE/$SUBDIR/$TEST.c $DEST/$SUBDIR
          for SUFFIX in args cflags env; do
            filter < build/$SUBDIR/$TEST.$SUFFIX > $DEST/$SUBDIR/$TEST.$SUFFIX
          done
	  for FILE in $TEST.input $TEST.exp; do
	    if [ -e $SOURCE/$SUBDIR/$FILE ]; then
	      cp -av $SOURCE/$SUBDIR/$FILE $DEST/$SUBDIR
	    fi
	  done

          # Add test to list
          echo $SUBDIR/$TEST >>$DEST/test-list
        else
          echo "skipping $SUBDIR/$TEST (not present: $SUBDIR/$TEST.c)"
        fi
      done
    fi
  done
  echo `wc -l < $DEST/test-list` tests copied
}


if [ $# -eq 0 ]; then
  ACTIONS="unpack patch get_list copy"
else
  ACTIONS=$@
fi

for ACTION in $ACTIONS; do
  case $ACTION in
    unpack|patch|get_list|copy)
      action_$ACTION
      ;;
    *)
      echo "$0: Unknown action: $ACTION"
      exit 1
      ;;
  esac
done
