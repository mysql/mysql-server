#!/bin/bash

# Copyright (c) 2013, 2022, Oracle and/or its affiliates.
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

mysock="/tmp/mysql.sock"
#mysock="$mylogdir/mysql.sock"

# 4 bash parameter substitution forms: $*, "$*", $@, "$@" (use last here)
#echo nArgs=$#
#echo args=\'"$@"\'
#for p in "$@"; do echo "    [$p]"; done

#echo
#"mysql" "$@"
# for localhost connections:
"mysql" --socket="$mysock" "$@"
# for remote connections:
#"mysql" -h localhost -P 3307 "$@"
#"mysql" -host=localhost -port=3307 "$@"

#set +x
