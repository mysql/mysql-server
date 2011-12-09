#!/bin/sh

# Copyright (c) 2003, 2010, Oracle and/or its affiliates. All rights reserved.
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

mkdir -p result
cd result
rm -rf *

if [ `uname | grep -ic cygwin || true` -ne 0 ]
then
  while [ $# -gt 0 ]
  do
    SAVE_IFS=$IFS
    IFS=":"
    declare -a ARR="($1)"
    IFS=$SAVE_IFS
    DIR=`dirname "${ARR[1]}"`
    REMOTE_DIR=`cygpath -u $DIR`
    HOST="${ARR[0]}"
    rsync -a --exclude='BACKUP' --exclude='ndb_*_fs' "$HOST:$REMOTE_DIR" .
    shift
  done
else
  while [ $# -gt 0 ]
  do
    rsync -a --exclude='BACKUP' --exclude='ndb_*_fs' "$1" .
    shift
  done

  #
  # clean tables...not to make results too large
  #
  lst=`find . -name '*.frm'`
  if [ "$lst" ]
  then
 	  basename=`echo $i | sed 's!\.frm!!'`
	  if [ "$basename" ]
	  then
	    rm -f $basename.*
	  fi
  fi

fi
