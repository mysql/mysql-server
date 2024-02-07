#!/bin/sh

# Copyright (c) 2003, 2024, Oracle and/or its affiliates.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is designed to work with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have either included with
# the program or referenced in the documentation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

MACHINE=$1
LOCAL_DIR=$2
REMOTE_DIR=$3
verbose=

if [ `uname | grep -ic cygwin || true` -ne 0 ]
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

if [ `uname | grep -ic cygwin || true` -ne 0 ]
then
    LOCAL_DIR=`cygpath -u $LOCAL_DIR`
    REMOTE_DIR=`cygpath -u $REMOTE_DIR`
fi

set -e
ssh $MACHINE rm -rf $REMOTE_DIR
ssh $MACHINE mkdir -p $REMOTE_DIR
rsync -a --delete --force --ignore-errors $LOCAL_DIR $MACHINE:$REMOTE_DIR
