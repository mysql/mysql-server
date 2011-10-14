#!/bin/sh

# Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.
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

$mysql_exe -e "drop database if exists ${myisam_db};"
$mysql_exe -e "drop database if exists ${ndb_db};"
$mysql_exe -e "create database ${myisam_db} ${charset_spec};"
$mysql_exe -e "create database ${ndb_db} ${charset_spec}"

# Call RANDGEN
${gendata} --dsn="$dsn:database=${myisam_db}" --spec ${data}

for i in $sprocs
do
    $mysql_exe ${ndb_db} < $base/$i.sproc.sql
done

for i in $sprocs
do
    if [ "$i" = "oj_schema_mod" ]
    then
	$mysql_exe ${ndb_db} -e "call $i('${myisam_db}');"
    elif [ "$i" = "copydb" ]
    then
	$mysql_exe ${ndb_db} -e "call copydb('${ndb_db}', '${myisam_db}');"
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
