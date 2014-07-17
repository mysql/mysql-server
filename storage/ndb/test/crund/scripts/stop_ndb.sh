#!/bin/bash

# Copyright (c) 2010, 2014, Oracle and/or its affiliates. All rights reserved.
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
  PATH="$MYSQL_LIBEXEC:$MYSQL_BIN:$PATH"
fi

#set -x

#./show_cluster.sh

echo shut down NDB...
./mgm.sh -e shutdown -t 1

timeout=60
echo
echo "waiting ($timeout s) for ndb to shut down..."
"ndb_waiter" -c "$NDB_CONNECT" -t $timeout --no-contact

# need some extra time for ndb_mgmd to terminate
for ((i=0; i<10; i++)) ; do printf "." ; sleep 1 ; done ; echo

#echo
#ps -efa | grep ndb

#set +x
