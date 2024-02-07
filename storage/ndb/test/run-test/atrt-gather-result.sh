#!/bin/bash

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

set -e

GATHER_TYPE="$1"

case "${GATHER_TYPE}" in
--result) shift;;
--coverage) shift;;
--*) echo "Bad gather type exiting.." >&2; exit 1;;
*) GATHER_TYPE="--result";;
esac

if [ -d result ]; then
  rm -rf result
fi

if [ -d coverage_result ]; then
  rm -rf coverage_result
fi

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

  if [ "${GATHER_TYPE}" = "--result" ]; then
    mkdir -p result
    #
    # The below commented out lines can be used if we want to keep the file
    # as part of the result from a faulty test in autotest. The first line
    # also keeps the BACKUP files as part of a faulty test case. These lines
    # can be used in special autotest runs when a the file contents are
    # needed to debug issues in test cases.
    #
    # rsync -a "$SRC_PATH" result/
    # rsync -a --exclude='BACKUP' "$SRC_PATH" result/
    # rsync -a --exclude='BACKUP' --exclude='ndb_*_fs/D*' "$SRC_PATH" result/
    # rsync -a --exclude='BACKUP' --exclude='ndb_*_fs/D*' --exclude='ndb_*_fs/*.dat' "$SRC_PATH" result/
    # rsync -a --exclude='BACKUP' --exclude='ndb_*_fs' "$SRC_PATH" result/
    rsync -a --exclude='BACKUP' --exclude='ndb_*_fs' --exclude='mysqld.*/data' \
      --exclude='gcov' "${SRC_PATH}" result/
    RESULT="$?"
  elif [ "${GATHER_TYPE}" = "--coverage" ]; then
    mkdir -p "coverage_result/${HOST}/"
    rsync -am --include "*.gcda" --include "*/" --exclude "*" "${SRC_PATH}" \
      "coverage_result/${HOST}/"
    RESULT="$?"
  fi
  set -e
  if [ ${RESULT} -ne 0 -a ${RESULT} -ne 24 ] ; then
    echo "rsync error: ${RESULT}"
    exit 1
  fi 
  shift
done
