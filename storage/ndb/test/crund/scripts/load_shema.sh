#!/bin/bash

# Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved.
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

echo
"$MYSQL_BIN/ndb_mgm" -e show

# retry reloading the schema
# seems that dropping the tables fails if
# - the data nodes haven't fully come up yet
# - ndb and mysqld have gotten out of sync (e.g., may happen when ndb was
#   (re)started with option "--initial", see bug.php?id=42107)
for ((i=3; i>=0; i--)) ; do

  user="`whoami`"
  echo
  echo "grant privileges to users..."
  # MySQL does not support wildcards in user names, only anonymous users
  # specified by the empty user name ''; any user who connects from the
  # local host with the correct password for the anonymous user will be
  # allowed access then. 
  echo "GRANT ALL ON *.* TO ''@localhost;" \
       "GRANT ALL ON *.* TO $user@localhost;" \
       | "$MYSQL_BIN/mysql" -v -u root
  s=$?
  echo "mysql exit status: $s"

  if [[ $s == 0 ]]; then
    echo "successfully granted privileges"
  else
    echo
    echo "failed granting privileges; retrying up to $i times..."
    for ((j=0; j<3; j++)) ; do echo "." ; sleep 1; done
    continue
  fi

  echo
  echo "load crund schema..."
  "$MYSQL_BIN/mysql" -v < ../src/tables_mysql.sql
  s=$?
  echo "mysql exit status: $s"

  if [[ $s == 0 ]]; then
    echo "successfully loaded crunddb schema"
  else
    echo
    echo "failed loading schema; retrying up to $i times..."
    for ((j=0; j<3; j++)) ; do echo "." ; sleep 1; done
    continue
  fi

  echo
  echo "load tws schema..."
  "$MYSQL_BIN/mysql" -v < ../tws/schema.sql
  s=$?
  echo "mysql exit status: $s"

  if [[ $s == 0 ]]; then
    echo "successfully loaded testdb schema"
  else
    echo
    echo "failed loading schema; retrying up to $i times..."
    for ((j=0; j<3; j++)) ; do echo "." ; sleep 1; done
    continue
  fi

  break

done

echo
echo "show tables..."
"$MYSQL_BIN/mysql" -e "USE crunddb; SHOW TABLES;"
"$MYSQL_BIN/mysql" -e "USE testdb; SHOW TABLES;"

echo
echo done.
