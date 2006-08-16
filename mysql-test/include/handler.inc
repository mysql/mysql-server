# include/handler.inc
#
# The variables
#     $engine_type       -- storage engine to be tested
#     $other_engine_type -- storage engine <> $engine_type
#     $other_handler_engine_type -- storage engine <> $engine_type, if possible
#                           1. $other_handler_engine_type must support handler
#                           2. $other_handler_engine_type must point to an all
#                              time available storage engine
#                              2006-08 MySQL 5.1 MyISAM and MEMORY only
# have to be set before sourcing this script.
#
# test of HANDLER ...
#
# Last update:
# 2006-07-31 ML test refactored (MySQL 5.1)
#               code of t/handler.test and t/innodb_handler.test united
#               main testing code put into include/handler.inc
#

eval SET SESSION STORAGE_ENGINE = $engine_type;

--disable_warnings
drop table if exists t1,t3,t4,t5;
--enable_warnings

create table t1 (a int, b char(10), key a(a), key b(a,b));
insert into t1 values
(17,"ddd"),(18,"eee"),(19,"fff"),(19,"yyy"),
(14,"aaa"),(15,"bbb"),(16,"ccc"),(16,"xxx"),
(20,"ggg"),(21,"hhh"),(22,"iii");
handler t1 open as t2;
-- error 1064
handler t2 read a=(SELECT 1);
handler t2 read a first;
handler t2 read a next;
handler t2 read a next;
handler t2 read a prev;
handler t2 read a last;
handler t2 read a prev;
handler t2 read a prev;

handler t2 read a first;
handler t2 read a prev;

handler t2 read a last;
handler t2 read a prev;
handler t2 read a next;
handler t2 read a next;

handler t2 read a=(15);
handler t2 read a=(16);

--error 1070
handler t2 read a=(19,"fff");

handler t2 read b=(19,"fff");
handler t2 read b=(19,"yyy");
handler t2 read b=(19);

--error 1109
handler t1 read a last;

handler t2 read a=(11);
handler t2 read a>=(11);

handler t2 read a=(18);
handler t2 read a>=(18);
handler t2 read a>(18);
handler t2 read a<=(18);
handler t2 read a<(18);

handler t2 read a first limit 5;
handler t2 read a next  limit 3;
handler t2 read a prev  limit 10;

handler t2 read a>=(16) limit 4;
handler t2 read a>=(16) limit 2,2;
handler t2 read a last  limit 3;

handler t2 read a=(19);
handler t2 read a=(19) where b="yyy";

handler t2 read first;
handler t2 read next;
handler t2 read next;
--error 1064
handler t2 read last;
handler t2 close;

handler t1 open;
handler t1 read a next; # this used to crash as a bug#5373
handler t1 read a next;
handler t1 close;

handler t1 open;
handler t1 read a prev; # this used to crash as a bug#5373
handler t1 read a prev;
handler t1 close;

handler t1 open as t2;
handler t2 read first;
eval alter table t1 engine = $engine_type;
--error 1109
handler t2 read first;

#
# DROP TABLE / ALTER TABLE
#
handler t1 open as t2;
drop table t1;
create table t1 (a int);
insert into t1 values (17);
--error 1109
handler t2 read first;
handler t1 open as t2;
eval alter table t1 engine=$other_engine_type;
--error 1109
handler t2 read first;
drop table t1;

#
# Test case for the bug #787
#
create table t1 (a int);
insert into t1 values (1),(2),(3),(4),(5),(6);
delete from t1 limit 2;
handler t1 open;
handler t1 read first;
handler t1 read first limit 1,1;
handler t1 read first limit 2,2;
delete from t1 limit 3;
handler t1 read first;
drop table t1;

#
# Test for #751
#
create table t1(a int, index(a));
insert into t1 values (1), (2), (3);
handler t1 open;
--error 1054
handler t1 read a=(W);
--error 1210
handler t1 read a=(a);
drop table t1;
#
# BUG#2304
#
create table t1 (a char(5));
insert into t1 values ("Ok");
handler t1 open as t;
handler t read first;
use mysql;
handler t read first;
handler t close;
handler test.t1 open as t;
handler t read first;
handler t close;
use test;
drop table t1;

#
# BUG#3649
#
create table t1 ( a int, b int, INDEX a (a) );
insert into t1 values (1,2), (2,1);
handler t1 open;
handler t1 read a=(1) where b=2;
handler t1 read a=(1) where b=3;
handler t1 read a=(1) where b=1;
handler t1 close;
drop table t1;

#
# Check if two database names beginning the same are seen as different.
#
# This database begins like the usual 'test' database.
#
--disable_warnings
drop database if exists test_test;
--enable_warnings
create database test_test;
use test_test;
create table t1(table_id char(20) primary key);
insert into t1 values ('test_test.t1');
insert into t1 values ('');
handler t1 open;
handler t1 read first limit 9;
create table t2(table_id char(20) primary key);
insert into t2 values ('test_test.t2');
insert into t2 values ('');
handler t2 open;
handler t2 read first limit 9;
#
# This is the usual 'test' database.
#
use test;
--disable_warnings
drop table if exists t1;
--enable_warnings
create table t1(table_id char(20) primary key);
insert into t1 values ('test.t1');
insert into t1 values ('');
--error 1066
handler t1 open;
#
# Check accesibility of all the tables.
#
use test;
--error 1064
handler test.t1 read first limit 9;
--error 1064
handler test_test.t1 read first limit 9;
handler t1 read first limit 9;
--error 1064
handler test_test.t2 read first limit 9;
handler t2 read first limit 9;

#
# Cleanup.
#

--error 1064
handler test_test.t1 close;
handler t1 close;
drop table test_test.t1;
--error 1064
handler test_test.t2 close;
handler t2 close;
drop table test_test.t2;
drop database test_test;

#
use test;
--error 1064
handler test.t1 close;
--error 1109
handler t1 close;
drop table test.t1;

#
# BUG#4335
#
--disable_warnings
drop database if exists test_test;
drop table if exists t1;
drop table if exists t2;
drop table if exists t3;
--enable_warnings
create database test_test;
use test_test;
create table t1 (c1 char(20));
insert into t1 values ('test_test.t1');
create table t3 (c1 char(20));
insert into t3 values ('test_test.t3');
handler t1 open;
handler t1 read first limit 9;
handler t1 open h1;
handler h1 read first limit 9;
use test;
create table t1 (c1 char(20));
create table t2 (c1 char(20));
create table t3 (c1 char(20));
insert into t1 values ('t1');
insert into t2 values ('t2');
insert into t3 values ('t3');
--error 1066
handler t1 open;
--error 1066
handler t2 open t1;
--error 1066
handler t3 open t1;
handler t1 read first limit 9;
--error 1064
handler test.t1 close;
--error 1066
handler test.t1 open h1;
--error 1066
handler test_test.t1 open h1;
handler test_test.t3 open h3;
handler test.t1 open h2;
handler t1 read first limit 9;
handler h1 read first limit 9;
handler h2 read first limit 9;
handler h3 read first limit 9;
handler h2 read first limit 9;
--error 1064
handler test.h1 close;
handler t1 close;
handler h1 close;
handler h2 close;
--error 1109
handler t1 read first limit 9;
--error 1109
handler h1 read first limit 9;
--error 1109
handler h2 read first limit 9;
handler h3 read first limit 9;
handler h3 read first limit 9;
use test_test;
handler h3 read first limit 9;
--error 1064
handler test.h3 read first limit 9;
handler h3 close;
use test;
drop table t3;
drop table t2;
drop table t1;
drop database test_test;

#
# Test if fix for BUG#4286 correctly closes handler tables.
#
create table t1 (c1 char(20));
insert into t1 values ("t1");
handler t1 open as h1;
handler h1 read first limit 9;
create table t2 (c1 char(20));
insert into t2 values ("t2");
handler t2 open as h2;
handler h2 read first limit 9;
create table t3 (c1 char(20));
insert into t3 values ("t3");
handler t3 open as h3;
handler h3 read first limit 9;
create table t4 (c1 char(20));
insert into t4 values ("t4");
handler t4 open as h4;
handler h4 read first limit 9;
create table t5 (c1 char(20));
insert into t5 values ("t5");
handler t5 open as h5;
handler h5 read first limit 9;
# close first
eval alter table t1 engine=$other_handler_engine_type;
--error 1109
handler h1 read first limit 9;
handler h2 read first limit 9;
handler h3 read first limit 9;
handler h4 read first limit 9;
handler h5 read first limit 9;
# close last
eval alter table t5 engine=$other_handler_engine_type;
--error 1109
handler h1 read first limit 9;
handler h2 read first limit 9;
handler h3 read first limit 9;
handler h4 read first limit 9;
--error 1109
handler h5 read first limit 9;
# close middle
eval alter table t3 engine=$other_handler_engine_type;
--error 1109
handler h1 read first limit 9;
handler h2 read first limit 9;
--error 1109
handler h3 read first limit 9;
handler h4 read first limit 9;
--error 1109
handler h5 read first limit 9;
handler h2 close;
handler h4 close;
# close all depending handler tables
handler t1 open as h1_1;
handler t1 open as h1_2;
handler t1 open as h1_3;
handler h1_1 read first limit 9;
handler h1_2 read first limit 9;
handler h1_3 read first limit 9;
eval alter table t1 engine=$engine_type;
--error 1109
handler h1_1 read first limit 9;
--error 1109
handler h1_2 read first limit 9;
--error 1109
handler h1_3 read first limit 9;
drop table t1;
drop table t2;
drop table t3;
drop table t4;
drop table t5;

#
# Bug#14397 - OPTIMIZE TABLE with an open HANDLER causes a crash
#
create table t1 (c1 int);
insert into t1 values (1);
# client 1
handler t1 open;
handler t1 read first;
# client 2
connect (con2,localhost,root,,);
connection con2;
--exec echo send the below to another connection, do not wait for the result
send optimize table t1;
--sleep 1
# client 1
--exec echo proceed with the normal connection
connection default;
handler t1 read next;
handler t1 close;
# client 2
--exec echo read the result from the other connection
connection con2;
reap;
# client 1
--exec echo proceed with the normal connection
connection default;
drop table t1;

CREATE TABLE t1 (  no1 smallint(5) NOT NULL default '0',  no2 int(10) NOT NULL default '0',  PRIMARY KEY  (no1,no2));
INSERT INTO t1 VALUES (1,274),(1,275),(2,6),(2,8),(4,1),(4,2);
HANDLER t1 OPEN;
HANDLER t1 READ `primary` = (1, 1000);
HANDLER t1 READ `primary` PREV;
DROP TABLE t1;

# End of 4.1 tests

#
# Addendum to Bug#14397 - OPTIMIZE TABLE with an open HANDLER causes a crash
# Show that DROP TABLE can no longer deadlock against
# FLUSH TABLES WITH READ LOCK. This is a 5.0 issue.
#
create table t1 (c1 int);
insert into t1 values (14397);
flush tables with read lock;
# The thread with the global read lock cannot drop the table itself:
--error 1223
drop table t1;
#
# client 2
# We need a second connection to try the drop.
# The drop waits for the global read lock to go away.
# Without the addendum fix it locked LOCK_open before entering the wait loop.
connection con2;
--exec echo send the below to another connection, do not wait for the result
send drop table t1;
--sleep 1
#
# client 1
# Now we need something that wants LOCK_open. A simple table access which
# opens the table does the trick.
--exec echo proceed with the normal connection
connection default;
# This would hang on LOCK_open without the 5.0 addendum fix.
select * from t1;
# Release the read lock. This should make the DROP go through.
unlock tables;
#
# client 2
# Read the result of the drop command.
connection con2;
--exec echo read the result from the other connection
reap;
#
# client 1
# Now back to normal operation. The table should not exist any more.
--exec echo proceed with the normal connection
connection default;
--error 1146
select * from t1;
# Just to be sure and not confuse the next test case writer.
drop table if exists t1;

