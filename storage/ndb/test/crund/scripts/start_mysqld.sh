#!/bin/bash

# Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved.
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
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

source ../env.properties
echo MYSQL_HOME=$MYSQL_HOME

echo
echo start mysqld...

mylogdir="ndblog"
mkdir -p "$mylogdir"

user="`whoami`"
cwd="`pwd`"
mycnf="$cwd/../my.cnf"
myerr="$cwd/$mylogdir/mysqld.log.err"
echo defaults-file=$mycnf
( cd $MYSQL_HOME ; "$MYSQL_BIN/mysqld_safe" --defaults-file="$mycnf" --user="$user" --log-error="$myerr" & )

# need some extra time
for ((i=0; i<3; i++)) ; do echo "." ; sleep 1; done

echo
echo show cluster...
"$MYSQL_BIN/ndb_mgm" -e show
