#!/bin/bash

# Copyright (c) 2010, 2012, Oracle and/or its affiliates. All rights reserved.
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
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

source ../env.properties
echo MYSQL_HOME=$MYSQL_HOME

echo shut down NDB...
"$MYSQL_BIN/ndb_mgm" -e shutdown -t 1

timeout=60
echo
echo waiting up to $timeout s for ndb_mgmd to shut down...
#"$MYSQL_BIN/ndb_waiter" -t $timeout -c "localhost:1186" --no-contact
"$MYSQL_BIN/ndb_waiter" -t $timeout --no-contact

# need some extra time
for ((i=0; i<6; i++)) ; do echo "." ; sleep 1; done
