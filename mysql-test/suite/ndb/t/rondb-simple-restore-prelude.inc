-- source include/have_ndb.inc
-- source suite/ndb/include/backup_restore_setup.inc

--echo Create table t1 with a unique index, and table t2 without.
use test;

create table t1 (
  id int primary key,
  val1 int unique) engine=ndb;

create table t2 (
  id int primary key,
  val1 int) engine=ndb;

insert into t1 values
        (1, 6),
        (3, 4),
        (2, 5);

insert into t2 values
        (8, 11),
        (7, 12),
        (9, 10);

# Save table definitions and data to temporary text files, to be used for comparisons
--let $t1m= $MYSQL_TMP_DIR/t1m.sql
--output $t1m
show create table t1;
--let $t1d= $MYSQL_TMP_DIR/t1d.txt
--output $t1d
select * from t1 order by id;
--let $t2m= $MYSQL_TMP_DIR/t2m.sql
--output $t2m
show create table t2;
--let $t2d= $MYSQL_TMP_DIR/t2d.txt
--output $t2d
select * from t2 order by id;

# Temporary files to compare to
--let $t1m_compare= $MYSQL_TMP_DIR/t1m_compare.sql
--let $t1d_compare= $MYSQL_TMP_DIR/t1d_compare.txt
--let $t2m_compare= $MYSQL_TMP_DIR/t2m_compare.sql
--let $t2d_compare= $MYSQL_TMP_DIR/t2d_compare.txt

--echo Original database
--cat_file $t1m
--cat_file $t1d
--cat_file $t2m
--cat_file $t2d

--echo Backing up data
--source include/ndb_backup.inc
disable_query_log; drop table t1; drop table t2; enable_query_log;

--let $restore= $NDB_RESTORE -b $the_backup_id
--let $backup= $NDB_BACKUPS-$the_backup_id
--let $trash= $NDB_TOOLS_OUTPUT
--let $t2_only= 0
