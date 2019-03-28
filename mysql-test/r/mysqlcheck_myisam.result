create table t1 (a int) engine=myisam;
create view v1 as select * from t1;
test.t1                                            OK
test.t1                                            Table is already up to date
test.t1                                            OK
test.t1                                            Table is already up to date
drop view v1;
drop table t1;
create table `t``1`(a int) engine=myisam;
create table `t 1`(a int) engine=myisam;
test.t 1                                           OK
test.t`1                                           OK
drop table `t``1`, `t 1`;
create database d_bug25347;
use d_bug25347;
create table t_bug25347 (a int) engine=myisam;
create view v_bug25347 as select * from t_bug25347;
insert into t_bug25347 values (1),(2),(3);
flush tables;
removing and creating
d_bug25347.t_bug25347
Error    : Incorrect file format 't_bug25347'
error    : Corrupt
insert into t_bug25347 values (4),(5),(6);
ERROR HY000: Incorrect file format 't_bug25347'
d_bug25347.t_bug25347
warning  : Number of rows changed from 0 to 3
status   : OK
insert into t_bug25347 values (7),(8),(9);
select * from t_bug25347;
a
1
2
3
7
8
9
select * from v_bug25347;
a
1
2
3
7
8
9
drop view v_bug25347;
drop table t_bug25347;
drop database d_bug25347;
use test;
#
# Bug#20868496: MYSQL_UPGRADE IN 5.7.7+ REPAIR LOOKS USER TABLES
#               IN TEST WHEN LOAD FROM 50/51/55
#
CREATE DATABASE db1;
CREATE DATABASE db2;
CREATE TABLE db1.t1 (a INT) ENGINE=MYISAM;
CREATE TABLE db2.t2 (a INT);
db1.t1                                             To be repaired, cause follows:
Server issued Error    : Incorrect file format 't1'
error    : Corrupt
db2.t2                                             OK

Repairing tables
db1.t1
Error    : Incorrect file format 't1'
error    : Corrupt
DROP DATABASE db1;
DROP DATABASE db2;
