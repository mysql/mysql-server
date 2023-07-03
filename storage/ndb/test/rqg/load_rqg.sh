#!/bin/sh

# Copyright (c) 2011, 2022, Oracle and/or its affiliates.
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

set -e

base="`dirname $0`"
source "$base"/parseargs.sh

# Create database with a case sensitive collation to ensure a deterministic 
# resultset when 'LIMIT' is specified:
charset_spec=""
if [ "$charset" ]
then
    charset_spec="character set $charset"
fi

sprocs="copydb alter_engine analyze_db"
if [ "$oj" ]
then
    sprocs="oj_schema_mod copydb alter_engine oj_schema_mod_ndb analyze_db"
fi

$mysql_exe -e "drop database if exists ${innodb_db};"
$mysql_exe -e "drop database if exists ${ndb_db};"
$mysql_exe -e "create database ${innodb_db} ${charset_spec};"
$mysql_exe -e "create database ${ndb_db} ${charset_spec}"

# Call RANDGEN
${gendata} --dsn="$dsn:database=${innodb_db}" --spec ${data}

for i in $sprocs
do
    $mysql_exe ${ndb_db} < $base/$i.sproc.sql
done

for i in $sprocs
do
    if [ "$i" = "oj_schema_mod" ]
    then
	$mysql_exe ${ndb_db} -e "call $i('${innodb_db}');"
    elif [ "$i" = "copydb" ]
    then
	$mysql_exe ${ndb_db} -e "call copydb('${ndb_db}', '${innodb_db}');"
    elif [ "$i" = "alter_engine" ]
    then
	$mysql_exe ${ndb_db} -e "call $i('${ndb_db}', 'ndb');"
    else
	$mysql_exe ${ndb_db} -e "call $i('${ndb_db}');"
    fi
done


for i in $sprocs
do
    $mysql_exe ${ndb_db} -e "drop procedure $i;"
done
