#!/bin/sh

# Copyright (c) 2011, 2024, Oracle and/or its affiliates.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is designed to work with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have either included with
# the program or referenced in the documentation.
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

ecp="set optimizer_switch = 'engine_condition_pushdown=on';"
# Remove ONLY_FULL_GROUP_BY from sql_mode since the
# syntax have been written with non aggregated columns in
# the select list
disable_only_full_group_by="set sql_mode=(select replace(@@sql_mode,'ONLY_FULL_GROUP_BY',''));"

check_query(){
    file=$1
    no=$2
    line=`expr $no \* 3` || true
    line=`expr $line + 3 + 2 + 1 + 1` || true
    sql=`head -n $line $file | tail -1`

    tmp=${opre}.$no.sql
    cat > $tmp <<EOF
--disable_warnings
--disable_query_log
--eval set ndb_join_pushdown='\$NDB_JOIN_PUSHDOWN';
$ecp
$disable_only_full_group_by
--echo kalle
--sorted_result
--error 0,233,1242,4006
$sql
--exit
EOF

    NDB_JOIN_PUSHDOWN=off
    export NDB_JOIN_PUSHDOWN
    for t in 1
    do
	$mysqltest_exe ${innodb_db} < $tmp >> ${opre}.$no.innodb.$i.txt
    done

    for t in 1
    do
	$mysqltest_exe ${ndb_db} < $tmp >> ${opre}.$no.ndb.$i.txt
    done

    NDB_JOIN_PUSHDOWN=on
    export NDB_JOIN_PUSHDOWN
    for t in 1
    do
	$mysqltest_exe ${ndb_db} < $tmp >> ${opre}.$no.ndbpush.$i.txt
    done

    cnt=`$md5sum ${opre}.$no.*.txt | $awk_exe '{ print $1;}' | sort | uniq | wc -l`
    if [ $cnt -ne 1 ]
    then
	echo -n "$no "
	echo $sql >> ${opre}.failing.sql
    fi

    rm $tmp ${opre}.$no.*.txt
}

locate_query (){
    file=$1
    rows=`cat $file | wc -l`
    queries=`expr $rows - 4`
    queries=`expr $queries / 3`
    q=0
    echo -n "checking queries..."
    while [ $q -ne $queries ]
    do
	check_query $file $q
	q=`expr $q + 1`
    done
    echo
}

run_all() {
    file=$1

    tmpfiles=""
    NDB_JOIN_PUSHDOWN=off
    export NDB_JOIN_PUSHDOWN
    if [ `echo $mode | grep -c m` -ne 0 ]
    then
	echo "- run innodb"
	start=`eval $getepochtime`
	$mysqltest_exe ${innodb_db} < $file > ${opre}_innodb.out
	stop=`eval $getepochtime`
	echo "  elapsed: `expr $stop - $start`s"
	tmpfiles="$tmpfiles ${opre}_innodb.out"
    fi

    if [ `echo $mode | grep -c nv` -ne 0 ]
    then
	echo "- run ndb without push"
	NDB_JOIN_PUSHDOWN=off
	export NDB_JOIN_PUSHDOWN
	start=`eval $getepochtime`
	$mysqltest_exe ${ndb_db} < $file > ${opre}_ndb.out
	stop=`eval $getepochtime`
	echo "  elapsed: `expr $stop - $start`s"
	tmpfiles="$tmpfiles ${opre}_ndb.out"
    fi

    if [ `echo $mode | grep -c np` -ne 0 ]
    then
	echo "- run ndb with push"
	NDB_JOIN_PUSHDOWN=on
	export NDB_JOIN_PUSHDOWN
	start=`eval $getepochtime`
	$mysqltest_exe ${ndb_db} < $file > ${opre}_ndbpush.out
	stop=`eval $getepochtime`
	echo "  elapsed: `expr $stop - $start`s"
	tmpfiles="$tmpfiles ${opre}_ndbpush.out"
    fi

    md5s=""
    for f in $tmpfiles
    do
	md5s="$md5s `$md5sum $f | $awk_exe '{ print $1;}'`"
    done

    ###
    # Check that they are all equal
    check=""
    fail=""
    for s in $md5s
    do
	if [ -z "$check" ]
	then
	    check="$s"
	elif [ "$check" != "$s" ]
	then
	    fail="yes"
	fi
    done

    if [ "$fail" ]
    then
	echo "md5 missmatch: $md5s"
	echo "locating failing query(s)"
	locate_query $file
    fi

    rm -f $tmpfiles
}

i=0
stoptime=""
if [ "$runtime" ]
then
    stoptime=`eval $getepochtime`
    stoptime=`expr $stoptime + $runtime`
    loops=1000000
fi

while [ $i -ne $loops ]
do
    i=`expr $i + 1`

    echo "** loop $i"
    us="$seed" || true
    if [ -z "$us" ]
    then
    	us=`eval $getepochtime`
    fi

    echo "- generating sql seed: $us queries: $queries"

    (
	echo "--disable_warnings"
	echo "--disable_query_log"
	echo "--eval set ndb_join_pushdown='\$NDB_JOIN_PUSHDOWN';"
	echo "$ecp"
	echo "$disable_only_full_group_by"
	${gensql} --seed=$us --queries=$queries --dsn="$dsn:database=${innodb_db}" --grammar=$grammar | grep -v "#" |
        $awk_exe '{ print "--sorted_result"; print "--error 0,233,1242,4006"; print; }'
	echo "--exit"
    ) > ${opre}_test.sql

    run_all ${opre}_test.sql
    rm ${opre}_test.sql
    echo

    if [ "$stoptime" ]
    then
	if [ `eval $getepochtime` -ge $stoptime ]
	then
	    break;
	fi
    fi
done

if [ -f "${opre}.failing.sql" ]
then
    echo "*** FAILING QUERIES ***"
    cat "${opre}.failing.sql"
    exit 1
fi
