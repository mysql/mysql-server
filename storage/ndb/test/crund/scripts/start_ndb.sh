#!/bin/bash

# Copyright (c) 2010, 2014, Oracle and/or its affiliates. All rights reserved.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is also distributed with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have included with MySQL.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

if [ "$MYSQL_HOME" = "" ] ; then
  source ../env.properties
  echo MYSQL_HOME=$MYSQL_HOME
  PATH="$MYSQL_LIBEXEC:$MYSQL_BIN:$PATH"
fi

#set -x

./start_mgmd.sh

# need some extra time
for ((i=0; i<10; i++)) ; do printf "." ; sleep 1 ; done ; echo

cwd="$(pwd)"
mylogdir="$cwd/ndblog"
mkdir -p "$mylogdir"
myini="$cwd/../config.ini"
nNodes=$(grep -c '^ *\[ndbd\]' "$myini")

echo
echo "starting $nNodes data nodes..."
for ((i=0; i<nNodes; i++)) ; do
    echo
    echo "start ndbd..."
    # apparently, these options need to come first:
    # --print-defaults        Print the program argument list and exit.
    # --no-defaults           Don't read default options from any option file.
    # --defaults-file=#       Only read default options from the given file #.
    # --defaults-extra-file=# Read this file after the global files are read.
    #( cd "$mylogdir" ; "ndbd" --initial )
    ( cd "$mylogdir" ; "ndbd" -c "$NDB_CONNECT" --initial )
done

#echo
#ps -efa | grep ndb

# need some extra time
for ((i=0; i<3; i++)) ; do printf "." ; sleep 1 ; done ; echo

timeout=60
echo
echo "waiting ($timeout s) for ndbd to start up..."
"ndb_waiter" -c "$NDB_CONNECT" -t $timeout

# need some extra time
for ((i=0; i<3; i++)) ; do printf "." ; sleep 1 ; done ; echo

./show_cluster.sh

#set +x
