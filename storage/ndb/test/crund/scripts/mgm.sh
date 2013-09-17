#!/bin/sh

# Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved.
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

if [ "$MYSQL_HOME" = "" ] ; then
  source ../env.properties
  echo MYSQL_HOME=$MYSQL_HOME
fi

#set -x

# 4 bash parameter substitution forms: $*, "$*", $@, "$@" (use last here)
#echo nArgs=$#
#echo args=\'"$@"\'
#for p in "$@"; do echo "    [$p]"; done

echo
"$MYSQL_BIN/ndb_mgm" -c "$NDB_CONNECT" "$@"

#set +x
