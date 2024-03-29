--source include/force_myisam_default.inc
--source include/have_myisam.inc

#
# Bug #16502: mysqlcheck tries to check views
#
create table t1 (a int) engine=myisam;
create view v1 as select * from t1;
--replace_result 'Table is already up to date' OK
--exec $MYSQL_CHECK --analyze --databases test
--exec $MYSQL_CHECK --optimize --databases test
--replace_result 'Table is already up to date' OK
--exec $MYSQL_CHECK --all-in-1 --analyze --databases test
--exec $MYSQL_CHECK --all-in-1 --optimize --databases test
drop view v1;
drop table t1;

#
# Bug #30654: mysqlcheck fails during upgrade of tables whose names include backticks
#
create table `t``1`(a int) engine=myisam;
create table `t 1`(a int) engine=myisam;
--replace_result 'Table is already up to date' OK
--exec $MYSQL_CHECK --databases test
drop table `t``1`, `t 1`;

#
# Bug#25347: mysqlcheck -A -r doesn't repair table marked as crashed
#
create database d_bug25347;
use d_bug25347;
create table t_bug25347 (a int) engine=myisam;
create view v_bug25347 as select * from t_bug25347;
insert into t_bug25347 values (1),(2),(3);
flush tables;
let $MYSQLD_DATADIR= `select @@datadir`;
--echo removing and creating
--remove_file $MYSQLD_DATADIR/d_bug25347/t_bug25347.MYI
--write_file $MYSQLD_DATADIR/d_bug25347/t_bug25347.MYI
EOF
--exec $MYSQL_CHECK --repair --databases d_bug25347
--error 130
insert into t_bug25347 values (4),(5),(6);
--exec $MYSQL_CHECK --repair --use-frm --databases d_bug25347
insert into t_bug25347 values (7),(8),(9);
select * from t_bug25347;
select * from v_bug25347;
drop view v_bug25347;
drop table t_bug25347;
drop database d_bug25347;
use test;


--echo #
--echo # Bug#20868496: MYSQL_UPGRADE IN 5.7.7+ REPAIR LOOKS USER TABLES
--echo #               IN TEST WHEN LOAD FROM 50/51/55
--echo #
CREATE DATABASE db1;
CREATE DATABASE db2;
CREATE TABLE db1.t1 (a INT) ENGINE=MYISAM;
--remove_file $MYSQLD_DATADIR/db1/t1.MYI
--write_file $MYSQLD_DATADIR/db1/t1.MYI
EOF
CREATE TABLE db2.t2 (a INT);
--exec $MYSQL_CHECK --auto-repair --databases db1 db2 2>&1
DROP DATABASE db1;
DROP DATABASE db2;
