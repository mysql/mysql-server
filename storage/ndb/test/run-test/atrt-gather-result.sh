#!/bin/bash

# Copyright (c) 2003, 2017, Oracle and/or its affiliates. All rights reserved.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 2 of the License.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

set -e

if [ -d result ]; then
  rm -rf result
fi

mkdir result
cd result

name="`uname -n | sed 's!\..*!!g'`"
cygwin="`uname | grep -ic cygwin || true`"

while [ $# -gt 0 ]; do
  IFS=':' read -r HOST DIR <<< "$1"

  if [ -z "$DIR" ]; then
    echo "Error: input must be either 'HOST:/some/path' or 'HOST:c:\some\path'"
    exit 1
  fi

  if (( $cygwin )); then
    DIR=`cygpath -u "$DIR"`
  fi

  # Add trailing slash to prevent rsync creating an extra folder level
  DIR="$DIR/"

  if [[ -z "$HOST" || "$HOST" == "$name" || "$HOST" == "localhost" ]]; then
    SRC_PATH="$DIR"
  else
    SRC_PATH="$HOST:$DIR"
  fi

  set +e
  #
  # The below commented out lines can be used if we want to keep the file
  # as part of the result from a faulty test in autotest. The first line
  # also keeps the BACKUP files as part of a faulty test case. These lines
  # can be used in special autotest runs when a the file contents are
  # needed to debug issues in test cases.
  #
  # rsync -a "$SRC_PATH" .
  # rsync -a --exclude='BACKUP' "$SRC_PATH" .
  # rsync -a --exclude='BACKUP' --exclude='ndb_*_fs/D*' "$SRC_PATH" .
  # rsync -a --exclude='BACKUP' --exclude='ndb_*_fs/D*' --exclude='ndb_*_fs/*.dat' "$SRC_PATH" .
  rsync -a --exclude='BACKUP' --exclude='ndb_*_fs' "$SRC_PATH" .
  RESULT="$?"
  set -e
  if [ ${RESULT} -ne 0 -a ${RESULT} -ne 24 ] ; then
    echo "rsync error: $RESULT"
    exit 1
  fi 
  shift
done
