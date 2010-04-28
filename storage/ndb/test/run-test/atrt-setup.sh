#!/bin/sh

MACHINE=$1
LOCAL_DIR=$2
REMOTE_DIR=$3

# Local copy
if [ `uname -n` = "$MACHINE" ]; then
  if [ "$REMOTE_DIR" = "$LOCAL_DIR" -o "$REMOTE_DIR/" = "$LOCAL_DIR" ]; then
    echo "$0: Same directoty on local host. Skipping setup..."
  else
    echo "$0: Local machine setup from '$REMOTE_DIR' to '$LOCAL_DIR'..."
    cp -r "$REMOTE_DIR" "$LOCAL_DIR"
  fi
  exit 0;
fi

if uname | grep -iq cygwin; then
  LOCAL_DIR=`cygpath -u $LOCAL_DIR`
  REMOTE_DIR=`cygpath -u $REMOTE_DIR`
fi

set -e
ssh $MACHINE rm -rf $REMOTE_DIR
ssh $MACHINE mkdir -p $REMOTE_DIR
rsync -a --delete --force --ignore-errors $LOCAL_DIR $MACHINE:$REMOTE_DIR
