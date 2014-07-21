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

cwd="$(pwd)"
mylogdir="$cwd/ndblog"
mkdir -p "$mylogdir"
user="$(whoami)"
mycnf="$cwd/../my.cnf"
myerr="$mylogdir/mysqld.log.err"
mysock="/tmp/mysql.sock"
#mysock="$mylogdir/mysql.sock"

echo
echo start mysqld...
( cd $MYSQL_HOME ; "mysqld_safe" --defaults-file="$mycnf" --user="$user" --log-error="$myerr" --socket="$mysock" & )
#
# debug:
#( cd $MYSQL_HOME ; "mysqld_safe" --defaults-file="$mycnf" --user="$user" --log-error="$myerr" -#d & )
# crashes when --debug/-# at beginning:
#( cd $MYSQL_HOME ; "$mysqld" --debug --defaults-file="$mycnf" --user="$user" --log-error="$myerr" & )

# need some extra time
for ((i=0; i<10; i++)) ; do printf "." ; sleep 1 ; done ; echo

#echo
#ps -efa | grep mysqld

./show_cluster.sh

#set +x
