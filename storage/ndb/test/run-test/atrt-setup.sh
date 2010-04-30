#!/bin/sh

MACHINE=$1
LOCAL_DIR=$2
REMOTE_DIR=$3
verbose=

if uname | grep -iq cygwin
then
    verbose=1
fi

name="`uname -n | sed 's!\..*!!g'`"

# Local copy
if [ "$MACHINE" = "$name" -o "$MACHINE" = "localhost" ]
then
    if [ "$REMOTE_DIR" = "$LOCAL_DIR" -o "$REMOTE_DIR/" = "$LOCAL_DIR" ]
    then
	if [ "$verbose" ]
	then
	    echo "$0: Same directory on localhost. Skipping setup..."
	fi
    else
	if [ "$verbose" ]
	then
	    echo "$0: Local machine setup from '$REMOTE_DIR' to '$LOCAL_DIR'..."
	fi
	cp -r "$REMOTE_DIR" "$LOCAL_DIR"
    fi
    exit 0;
fi

if uname | grep -iq cygwin
then
    LOCAL_DIR=`cygpath -u $LOCAL_DIR`
    REMOTE_DIR=`cygpath -u $REMOTE_DIR`
fi

set -e
ssh $MACHINE rm -rf $REMOTE_DIR
ssh $MACHINE mkdir -p $REMOTE_DIR
rsync -a --delete --force --ignore-errors $LOCAL_DIR $MACHINE:$REMOTE_DIR
