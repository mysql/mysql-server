#!/bin/sh

# Copyright (c) 2011, 2013, Oracle and/or its affiliates. All rights reserved.
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

: ${load:=1}
: ${loops:=100}
: ${queries:=1000}
: ${host:=loki43}
: ${port:=4401}
: ${RQG_HOME:=/net/fimafeng09/export/home/tmp/oleja/mysql/randgen/randgen-2.2.0}


while getopts ":nm:r:l:h:p:" opt; do
  case $opt in
    n)
      load=0
      ;;
    m)
      MYSQLINSTALL=${OPTARG}
      ;;
    r)
      RQG_HOME=${OPTARG}
      ;;
    l)
      loops=${OPTARG}
      ;;
    h)
      host=${OPTARG}
      ;;
    p)
      port=${OPTARG}
      ;;
    \?)
      echo "Usage: `basename $0` [options]"  >&2
      echo "-n : Do not create database (assumed to exist already)."  >&2
      echo "-m <mysql install dir>"  >&2
      echo "-r <rqg installation dir>"  >&2
      echo "-l <no of loops>"  >&2
      echo "-h <host>"  >&2
      echo "-p <port>"  >&2
      exit 1
      ;;
    :)
      echo "Option -$OPTARG requires an argument." >&2
      exit 1
      ;;
  esac
done


pre="spj"
opre="$pre.$$"

: ${data:= --spec=simple.zz}
: ${grammar:=spj_test.yy}

##: ${grammar:=$RQG_HOME/conf/optimizer_no_subquery.yy}   ## a pretty sensible grammar:
##: ${grammar:=$RQG_HOME/conf/optimizer_no_subquery_portable.yy}
##: ${grammar:=$RQG_HOME/conf/subquery_drizzle.yy}
##: ${grammar:=$RQG_HOME/conf/subquery_materialization.yy}
##: ${grammar:=$RQG_HOME/conf/subquery_semijoin.yy}
##: ${grammar:=$RQG_HOME/conf/subquery_semijoin_nested.yy}
##: ${grammar:=$RQG_HOME/conf/outer_join.yy}
##: ${grammar:=$RQG_HOME/conf/outer_join_portable.yy}

## Really simple (or: 'stupid') grammars: Have modified these, used to have nondeterministic behaviour
##: ${grammar:=$RQG_HOME/conf/subquery.yy}
##: ${grammar:=$RQG_HOME/conf/subquery-5.1.yy}

##: ${grammar:=$RQG_HOME/conf/optimizer_subquery.yy}

gensql=${RQG_HOME}/gensql.pl
gendata=${RQG_HOME}/gendata.pl
ecp="set optimizer_switch = 'engine_condition_pushdown=on';"

dsn=dbi:mysql:host=${host}:port=${port}:user=root:database=${pre}_innodb
mysqltest="$MYSQLINSTALL/bin/mysqltest -uroot --host=${host} --port=${port}"
mysql="$MYSQLINSTALL/bin/mysql --host=${host} --port=${port}"

# Create database with a case sensitive collation to ensure a deterministic 
# resultset when 'LIMIT' is specified:
charset_spec="character set latin1 collate latin1_bin"
#charset_spec="default character set utf8 default collate utf8_bin"

export RQG_HOME
if [ "$load" ]
then
	$mysql -uroot -e "drop database if exists ${pre}_innodb; drop database if exists ${pre}_ndb"
	$mysql -uroot -e "create database ${pre}_innodb ${charset_spec}; create database ${pre}_ndb ${charset_spec}"
	${gendata} --dsn=$dsn ${data}
cat > /tmp/sproc.$$ <<EOF
DROP PROCEDURE IF EXISTS copydb;
delimiter |;
CREATE PROCEDURE copydb(dstdb varchar(64), srcdb varchar(64),
                        dstengine varchar(64))
BEGIN

  declare tabname varchar(255);
  declare indextype varchar(32);
  declare _unique varchar(16);
  declare done integer default 0;
  declare cnt integer default 0;
  declare c cursor for 
    SELECT table_name
    FROM INFORMATION_SCHEMA.TABLES where table_schema = srcdb;
  declare continue handler for not found set done = 1;

  open c;
  
  repeat
    fetch c into tabname;
    if not done then
       set @ddl = CONCAT('CREATE TABLE ', dstdb, '.', tabname, 
                         ' LIKE ', srcdb, '.', tabname);
       select @ddl;
       PREPARE stmt from @ddl;
       EXECUTE stmt;
       set @ddl = CONCAT('ALTER TABLE ', dstdb, '.', tabname, 
                         ' engine = ', dstengine);
       PREPARE stmt from @ddl;
       EXECUTE stmt;

       ## Drop the original index 'col_int_unique' - recreate a richer set of indexes below
       set @ddl = CONCAT('DROP INDEX col_int_unique ON ', dstdb, '.', tabname);
       PREPARE stmt from @ddl;
       EXECUTE stmt;

       set cnt = cnt+1;

       set _unique = '';
       set indextype = '';
       ## Create a mix if 'CREATE INDEX' | 'CREATE UNIQUE INDEX [USING HASH] '

       if ((cnt%3) <> 0) then
         set _unique = ' UNIQUE';

         if ((cnt%2) = 0) then
           set indextype = ' USING HASH';
         else
           set indextype = '';
         end if;
       end if;

       ## Add some composite, possibly unique, indexes
       if tabname > 'T' then
          set @ddl = CONCAT('DROP INDEX col_varchar_10_unique ON ', dstdb, '.', tabname);
          PREPARE stmt from @ddl;
          EXECUTE stmt;

          set @ddl = CONCAT('CREATE', _unique, ' INDEX ix1 ', indextype,
                            ' ON ', dstdb, '.', tabname, 
                            '(col_char_16,col_char_16_unique)');
          PREPARE stmt from @ddl;
          EXECUTE stmt;
          set @ddl = CONCAT('CREATE', _unique, ' INDEX ix2 ', indextype,
                            ' ON ', dstdb, '.', tabname, 
                            '(col_varchar_256,col_varchar_10_unique)');
          PREPARE stmt from @ddl;
          EXECUTE stmt;

       elseif tabname > 'O' then
          set @ddl = CONCAT('CREATE', _unique, ' INDEX ix1 ', indextype,
                            ' ON ', dstdb, '.', tabname, 
                            '(col_int,col_int_unique)');
          PREPARE stmt from @ddl;
          EXECUTE stmt;
          set @ddl = CONCAT('CREATE', _unique, ' INDEX ix2 ', indextype,
                            ' ON ', dstdb, '.', tabname, 
                            '(col_int_key,col_int_unique)');
          PREPARE stmt from @ddl;
          EXECUTE stmt;

       elseif tabname > 'H' then
          set @ddl = CONCAT('CREATE', _unique, ' INDEX ix3 ', indextype,
                            ' ON ', dstdb, '.', tabname, 
                            '(col_int,col_int_key,col_int_unique)');
          PREPARE stmt from @ddl;
          EXECUTE stmt;

       else
          set @ddl = CONCAT('CREATE', _unique, ' INDEX ix1', indextype,
                            ' ON ', dstdb, '.', tabname, 
                            '(col_int_unique)');
          PREPARE stmt from @ddl;
          EXECUTE stmt;
       end if;

       ## Modify primary key & partition for some tables
       if ((cnt%3) = 0) then
          set @ddl = CONCAT('UPDATE ', srcdb, '.', tabname, ' SET col_int=0 WHERE col_int IS NULL');
          PREPARE stmt from @ddl;
          EXECUTE stmt;
           set @ddl = CONCAT('ALTER TABLE ', dstdb, '.', tabname,
                               '  CHANGE COLUMN pk pk INT(11) NOT NULL',
                               ', DROP PRIMARY KEY',
                               ', ADD PRIMARY KEY ', '(col_int,pk)');
           PREPARE stmt from @ddl;
           EXECUTE stmt;
           set @ddl = CONCAT('ALTER TABLE ', dstdb, '.', tabname, ' PARTITION BY KEY(col_int)');
           PREPARE stmt from @ddl;
           EXECUTE stmt;

        elseif ((cnt%5) = 0) then
           set @ddl = CONCAT('UPDATE ', srcdb, '.', tabname, ' SET col_int=0 WHERE col_int IS NULL');
           PREPARE stmt from @ddl;
           EXECUTE stmt;
           set @ddl = CONCAT('ALTER TABLE ', dstdb, '.', tabname,
                               '  CHANGE COLUMN pk pk INT(11) NOT NULL',
                               ', DROP PRIMARY KEY',
                               ', ADD PRIMARY KEY', indextype, '(col_int,pk)');
           PREPARE stmt from @ddl;
           EXECUTE stmt;
           set @ddl = CONCAT('ALTER TABLE ', dstdb, '.', tabname, ' PARTITION BY KEY(pk)');
           PREPARE stmt from @ddl;
           EXECUTE stmt;

        elseif ((cnt%7) = 0) then
           set @ddl = CONCAT('DELETE FROM ', srcdb, '.', tabname, ' WHERE col_char_16 IS NULL');
           PREPARE stmt from @ddl;
           EXECUTE stmt;
           set @ddl = CONCAT('ALTER TABLE ', dstdb, '.', tabname,
                               '  CHANGE COLUMN pk pk INT(11) NOT NULL',
                               ', DROP PRIMARY KEY',
                               ', ADD PRIMARY KEY', indextype, '(col_char_16,pk)');
           PREPARE stmt from @ddl;
           EXECUTE stmt;
           set @ddl = CONCAT('ALTER TABLE ', dstdb, '.', tabname, ' PARTITION BY KEY(col_char_16)');
           PREPARE stmt from @ddl;
           EXECUTE stmt;

        elseif ((cnt%11) = 0) then
           set @ddl = CONCAT('DELETE FROM ', srcdb, '.', tabname, ' WHERE col_varchar_10 IS NULL');
           PREPARE stmt from @ddl;
           EXECUTE stmt;
           set @ddl = CONCAT('ALTER TABLE ', dstdb, '.', tabname,
                               '  CHANGE COLUMN pk pk INT(11) NOT NULL',
                               ', DROP PRIMARY KEY',
                               ', ADD PRIMARY KEY', indextype, '(pk,col_varchar_256)');
           PREPARE stmt from @ddl;
           EXECUTE stmt;
           set @ddl = CONCAT('ALTER TABLE ', dstdb, '.', tabname, ' PARTITION BY KEY(col_varchar_256)');
           PREPARE stmt from @ddl;
           EXECUTE stmt;
        end if;

       set @ddl = CONCAT('INSERT INTO ', dstdb, '.', tabname, 
                         ' SELECT * FROM ', srcdb, '.', tabname);
       PREPARE stmt from @ddl;
       EXECUTE stmt;
    end if;
  until done end repeat;
  close c;
END
\G

DROP PROCEDURE IF EXISTS alterengine\G
CREATE PROCEDURE alterengine (db varchar(64), newengine varchar(64))
BEGIN

  declare tabname varchar(255);
  declare done integer default 0;
  declare c cursor for 
  SELECT table_name
  FROM INFORMATION_SCHEMA.TABLES where table_schema = db;
  declare continue handler for not found set done = 1;

  open c;
  
  repeat
    fetch c into tabname;
    if not done then
       set @ddl = CONCAT('ALTER TABLE ', db, '.', tabname, 
                         ' engine = ', newengine);
       select @ddl;
       PREPARE stmt from @ddl;
       EXECUTE stmt;
    end if;
  until done end repeat;
  close c;
END
\G

CREATE PROCEDURE analyzedb(db varchar(64))
BEGIN

  declare tabname varchar(255);
  declare done integer default 0;
  declare cnt integer default 0;
  declare c cursor for 
    SELECT table_name
    FROM INFORMATION_SCHEMA.TABLES where table_schema = db;
  declare continue handler for not found set done = 1;

  set @ddl = 'ANALYZE TABLE ';
  open c;
  
  repeat
    fetch c into tabname;
    if not done then
       set cnt = cnt+1;
       if cnt > 1 then
         set @ddl = CONCAT(@ddl, ', ');
       end if;
       set @ddl = CONCAT(@ddl,  db, '.', tabname);
    end if;
  until done end repeat;
  close c;

  PREPARE stmt from @ddl;
  EXECUTE stmt;
END
\G

CALL copydb('${pre}_ndb', '${pre}_innodb', 'ndb')\G
CALL analyzedb('${pre}_ndb')\G

##CALL alterengine('${pre}_ndb', 'ndb')\G
DROP PROCEDURE copydb\G
DROP PROCEDURE alterengine\G
DROP PROCEDURE analyzedb\G
EOF
	$mysql -uroot test < /tmp/sproc.$$
	rm -f /tmp/sproc.$$
fi

check_query(){
    file=$1
    no=$2
    line=`expr $no \* 3` || true
    line=`expr $line + 3 + 2 + 1 + 1` || true
    sql=`head -n $line $file | tail -n 1`

    tmp=${opre}.$no.sql
    cat > $tmp <<EOF
--disable_warnings
--disable_query_log
--eval set ndb_join_pushdown='\$NDB_JOIN_PUSHDOWN';
$ecp
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
	$mysqltest ${pre}_innodb < $tmp >> ${opre}.$no.innodb.$i.txt
    done

    for t in 1
    do
	$mysqltest ${pre}_ndb < $tmp >> ${opre}.$no.ndb.$i.txt
    done

    NDB_JOIN_PUSHDOWN=on
    export NDB_JOIN_PUSHDOWN
    for t in 1
    do
	$mysqltest ${pre}_ndb < $tmp >> ${opre}.$no.ndbpush.$i.txt
    done

    cnt=`md5sum ${opre}.$no.*.txt | awk '{ print $1;}' | sort | uniq | wc -l`
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

    NDB_JOIN_PUSHDOWN=off
    export NDB_JOIN_PUSHDOWN
    echo "- run innodb"
    $mysqltest ${pre}_innodb < $file > ${opre}_innodb.out
    md5_innodb=`md5sum ${opre}_innodb.out | awk '{ print $1;}'`

    echo "- run ndb without push"
    NDB_JOIN_PUSHDOWN=off
    export NDB_JOIN_PUSHDOWN
    $mysqltest ${pre}_ndb < $file > ${opre}_ndb.out
    md5_ndb=`md5sum ${opre}_ndb.out | awk '{ print $1;}'`

    echo "- run ndb with push"
    NDB_JOIN_PUSHDOWN=on
    export NDB_JOIN_PUSHDOWN
    $mysqltest ${pre}_ndb < $file > ${opre}_ndbpush.out
    md5_ndbpush=`md5sum ${opre}_ndbpush.out | awk '{ print $1;}'`

    if [ "$md5_innodb" != "$md5_ndb" ] || [ "$md5_innodb" != "$md5_ndbpush" ]
    then
	echo "md5 missmatch: $md5_innodb $md5_ndb $md5_ndbpush"
	echo "locating failing query(s)"
	locate_query $file
    fi

    rm ${opre}_innodb.out ${opre}_ndb.out ${opre}_ndbpush.out
}

i=0
while [ $i -ne $loops ]
do
    i=`expr $i + 1`

    echo "** loop $i"
    us="$seed" || true
    if [ -z "$us" ]
    then
    	us=`date '+%N'`
    fi

    echo "- generating sql seed: $us"

    (
	echo "--disable_warnings"
	echo "--disable_query_log"
	echo "--eval set ndb_join_pushdown='\$NDB_JOIN_PUSHDOWN';"
	echo "$ecp"
	${gensql} --seed=$us --queries=$queries --dsn=$dsn --grammar=$grammar|
        awk '{ print "--sorted_result"; print "--error 0,233,1242,4006"; print; }'
	echo "--exit"
    ) > ${opre}_test.sql

    run_all ${opre}_test.sql
    rm ${opre}_test.sql
    echo
done
