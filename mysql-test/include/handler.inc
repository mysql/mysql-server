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
-- source include/not_embedded.inc
#
# test of HANDLER ...
#
# Last update:
# 2006-07-31 ML test refactored (MySQL 5.1)
#               code of t/handler.test and t/innodb_handler.test united
#               main testing code put into include/handler.inc
#

eval SET SESSION DEFAULT_STORAGE_ENGINE = $engine_type;

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

#
# Bug#25856 - HANDLER table OPEN in one connection lock DROP TABLE in another one
#
--disable_warnings
drop table if exists t1;
--enable_warnings
eval create table t1 (a int) ENGINE=$other_engine_type;
--echo --> client 2
connection con2;
--error 1031
handler t1 open;
--echo --> client 1
connection default;
drop table t1;
disconnect con2;

#
# Bug#30632 HANDLER read failure causes hang
#
--disable_warnings
drop table if exists t1;
--enable_warnings
create table t1 (a int);
handler t1 open as t1_alias;
--error 1176
handler t1_alias read a next;
--error 1054
handler t1_alias READ a next where inexistent > 0;
--error 1176
handler t1_alias read a next;
--error 1054
handler t1_alias READ a next where inexistent > 0;
handler t1_alias close;
drop table t1;

#
# Bug#21587 FLUSH TABLES causes server crash when used with HANDLER statements
#

--disable_warnings
drop table if exists t1,t2;
--enable_warnings
create table t1 (c1 int);
create table t2 (c1 int);
insert into t1 values (1);
insert into t2 values (2);
--echo connection: default
handler t1 open;
handler t1 read first;
connect (flush,localhost,root,,);
connection flush;
--echo connection: flush
--send flush tables;
connect (waiter,localhost,root,,);
connection waiter;
--echo connection: waiter 
let $wait_condition=
  select count(*) = 1 from information_schema.processlist
  where state = "Waiting for table flush";
--source include/wait_condition.inc
connection default;
--echo connection: default
handler t2 open;
handler t2 read first;
handler t1 read next;
handler t1 close;
handler t2 close;
connection flush;
reap;
connection default;
drop table t1,t2;
disconnect flush;

#
# Bug#31409 RENAME TABLE causes server crash or deadlock when used with HANDLER statements
#

--disable_warnings
drop table if exists t1, t0;
--enable_warnings
create table t1 (c1 int);
--echo connection: default
handler t1 open;
handler t1 read first;
connect (flush,localhost,root,,);
connection flush;
--echo connection: flush
--send rename table t1 to t0;
connection waiter;
--echo connection: waiter 
let $wait_condition=
  select count(*) = 1 from information_schema.processlist
  where state = "Waiting for table metadata lock" and
        info = "rename table t1 to t0";
--source include/wait_condition.inc
connection default;
--echo connection: default
--echo #
--echo # RENAME placed two pending locks and waits.
--echo # When HANDLER t0 OPEN does open_tables(), it calls
--echo # mysql_ha_flush(), which in turn closes the open HANDLER for t1.
--echo # RENAME TABLE gets unblocked. If it gets scheduled quickly
--echo # and manages to complete before open_tables()
--echo # of HANDLER t0 OPEN, open_tables() and therefore the whole
--echo # HANDLER t0 OPEN succeeds. Otherwise open_tables() 
--echo # notices a pending or active exclusive metadata lock on t2
--echo # and the whole HANDLER t0 OPEN fails with ER_LOCK_DEADLOCK
--echo # error.
--echo #
--error 0, ER_LOCK_DEADLOCK
handler t0 open;
--error 0, ER_UNKNOWN_TABLE
handler t0 close;
--echo connection: flush
connection flush;
reap;
--error ER_UNKNOWN_TABLE
handler t1 read next;
--error ER_UNKNOWN_TABLE
handler t1 close;
connection default;
drop table t0;
connection flush;
disconnect flush;
--source include/wait_until_disconnected.inc
connection waiter;
disconnect waiter;
--source include/wait_until_disconnected.inc
connection default;

#
# Bug#30882 Dropping a temporary table inside a stored function may cause a server crash
#
# Test HANDLER statements in conjunction with temporary tables. While the temporary table
# is open by a HANDLER, no other statement can access it.
#

--disable_warnings
drop table if exists t1;
--enable_warnings
create temporary table t1 (a int, b char(1), key a(a), key b(a,b));
insert into t1 values (0,"a"),(1,"b"),(2,"c"),(3,"d"),(4,"e"),
                      (5,"f"),(6,"g"),(7,"h"),(8,"i"),(9,"j");
select a,b from t1;
handler t1 open as a1;
handler a1 read a first;
handler a1 read a next;
handler a1 read a next;
--error ER_CANT_REOPEN_TABLE
select a,b from t1;
handler a1 read a prev;
handler a1 read a prev;
handler a1 read a=(6) where b="g";
handler a1 close;
select a,b from t1;
handler t1 open as a2;
handler a2 read a first;
handler a2 read a last;
handler a2 read a prev;
handler a2 close;
drop table t1;

#
# Bug#31397 Inconsistent drop table behavior of handler tables.
#

--disable_warnings
drop table if exists t1,t2;
--enable_warnings
create table t1 (a int);
handler t1 open as t1_alias;
drop table t1;
create table t1 (a int);
handler t1 open as t1_alias;
flush tables;
drop table t1;
create table t1 (a int);
handler t1 open as t1_alias;
handler t1_alias close;
drop table t1;
create table t1 (a int);
handler t1 open as t1_alias;
handler t1_alias read first;
drop table t1;
--error ER_UNKNOWN_TABLE
handler t1_alias read next;

# Test that temporary tables associated with handlers are properly dropped.

create table t1 (a int);
create temporary table t2 (a int, key(a));
handler t1 open as a1;
handler t2 open as a2;
handler a2 read a first;
drop table t1, t2;
--error ER_UNKNOWN_TABLE
handler a2 read a next;
--error ER_UNKNOWN_TABLE
handler a1 close;

# Alter table drop handlers

create table t1 (a int, key(a));
create table t2 like t1;
handler t1 open as a1;
handler t2 open as a2;
handler a1 read a first;
handler a2 read a first;
alter table t1 add b int;
--error ER_UNKNOWN_TABLE
handler a1 close;
handler a2 close;
drop table t1, t2;

# Rename table drop handlers

create table t1 (a int, key(a));
handler t1 open as a1;
handler a1 read a first;
rename table t1 to t2;
--error ER_UNKNOWN_TABLE
handler a1 read a first;
drop table t2;

# Optimize table drop handlers

create table t1 (a int, key(a));
create table t2 like t1;
handler t1 open as a1;
handler t2 open as a2;
handler a1 read a first;
handler a2 read a first;
optimize table t1;
--error ER_UNKNOWN_TABLE
handler a1 close;
handler a2 close;
drop table t1, t2;

# Flush tables causes handlers reopen

create table t1 (a int, b char(1), key a(a), key b(a,b));
insert into t1 values (0,"a"),(1,"b"),(2,"c"),(3,"d"),(4,"e"),
                      (5,"f"),(6,"g"),(7,"h"),(8,"i"),(9,"j");
handler t1 open;
handler t1 read a first;
handler t1 read a next;
flush tables;
handler t1 read a next;
handler t1 read a next;
flush tables with read lock;
handler t1 read a next;
unlock tables;
drop table t1;
--error ER_UNKNOWN_TABLE
handler t1 read a next;

#
# Bug#41110: crash with handler command when used concurrently with alter table
# Bug#41112: crash in mysql_ha_close_table/get_lock_data with alter table
#

connect(con1,localhost,root,,);
connect(con2,localhost,root,,);

connection default;
--disable_warnings
drop table if exists t1;
--enable_warnings
--echo # First test case which is supposed trigger the execution
--echo # path on which problem was discovered.
create table t1 (a int);
insert into t1 values (1);
handler t1 open;
connection con1;
lock table t1 write;
send alter table t1 engine=memory;
connection con2;
let $wait_condition=
  select count(*) = 1 from information_schema.processlist
  where state = "Waiting for table metadata lock" and
        info = "alter table t1 engine=memory";
--source include/wait_condition.inc
connection default;
--error ER_ILLEGAL_HA
handler t1 read a next;
handler t1 close;
connection con1;
--reap
unlock tables;
drop table t1;
--echo # Now test case which was reported originally.
connection default;
create table t1 (a int, key(a));
insert into t1 values (1);
handler t1 open;
connection con1;
send alter table t1 engine=memory;
connection con2;
let $wait_condition=
  select count(*) = 1 from information_schema.processlist
  where state = "Waiting for table metadata lock" and
        info = "alter table t1 engine=memory";
--source include/wait_condition.inc
connection default;
--error ER_ILLEGAL_HA
handler t1 read a next;
handler t1 close;
connection con1;
--reap # Since last in this connection was a send
drop table t1;
disconnect con1;
--source include/wait_until_disconnected.inc
connection con2;
disconnect con2;
--source include/wait_until_disconnected.inc
connection default;

#
# Bug#44151 using handler commands on information_schema tables crashes server
#
USE information_schema;
--error ER_WRONG_USAGE
HANDLER COLUMNS OPEN;
USE test;

--echo #
--echo # Add test coverage for HANDLER and LOCK TABLES, HANDLER and DDL.
--echo #
--disable_warnings
drop table if exists t1, t2, t3;
--enable_warnings
create table t1 (a int, key a (a));
insert into t1 (a) values (1), (2), (3), (4), (5);
create table t2 (a int, key a (a)) select * from t1; 
create temporary table t3 (a int, key a (a)) select * from t2;
create temporary table t4 like t3;
handler t1 open;
handler t2 open;
handler t3 open;
--echo #
--echo # No HANDLER sql is allowed under LOCK TABLES.
--echo # But it does not implicitly closes all handlers.
--echo #
lock table t1 read;
--error ER_LOCK_OR_ACTIVE_TRANSACTION
handler t1 open;
--error ER_LOCK_OR_ACTIVE_TRANSACTION
handler t1 read next;
--error ER_LOCK_OR_ACTIVE_TRANSACTION
handler t2 close;
--error ER_LOCK_OR_ACTIVE_TRANSACTION
handler t3 open;
--error ER_LOCK_OR_ACTIVE_TRANSACTION
handler t4 open;
--echo # After UNLOCK TABLES handlers should be around and
--echo # we should be able to continue reading through them.
unlock tables;
handler t1 read next;
handler t1 close;
handler t2 read next;
handler t2 close;
handler t3 read next;
handler t3 close;
drop temporary tables t3, t4;
--echo #
--echo # Other operations that implicitly close handler:
--echo #
--echo # TRUNCATE
--echo #
handler t1 open;
truncate table t1;
--error ER_UNKNOWN_TABLE
handler t1 read next;
handler t1 open;
--echo #
--echo # CREATE TRIGGER
--echo #
create trigger t1_ai after insert on t1 for each row set @a=1;
--error ER_UNKNOWN_TABLE
handler t1 read next;
--echo #
--echo # DROP TRIGGER
--echo #
handler t1 open;
drop trigger t1_ai;
--error ER_UNKNOWN_TABLE
handler t1 read next;
--echo #
--echo # ALTER TABLE
--echo # 
handler t1 open;
alter table t1 add column b int;
--error ER_UNKNOWN_TABLE
handler t1 read next;
--echo #
--echo # ANALYZE TABLE
--echo # 
handler t1 open;
analyze table t1;
--error ER_UNKNOWN_TABLE
handler t1 read next;
--echo #
--echo # OPTIMIZE TABLE
--echo # 
handler t1 open;
optimize table t1;
--error ER_UNKNOWN_TABLE
handler t1 read next;
--echo #
--echo # REPAIR TABLE
--echo # 
handler t1 open;
repair table t1;
--error ER_UNKNOWN_TABLE
handler t1 read next;
--echo #
--echo # DROP TABLE, naturally.
--echo # 
handler t1 open;
drop table t1;
--error ER_UNKNOWN_TABLE
handler t1 read next;
create table t1 (a int, b int, key a (a)) select a from t2;
--echo #
--echo # RENAME TABLE, naturally
--echo # 
handler t1 open;
rename table t1 to t3;
--error ER_UNKNOWN_TABLE
handler t1 read next;
--echo #
--echo # CREATE TABLE (even with IF NOT EXISTS clause,
--echo # and the table exists).
--echo # 
handler t2 open;
create table if not exists t2 (a int);
--error ER_UNKNOWN_TABLE
handler t2 read next;
rename table t3 to t1;
drop table t2;
--echo #
--echo # FLUSH TABLE doesn't close the table but loses the position
--echo # 
handler t1 open;
handler t1 read a prev;
flush table t1;
handler t1 read a prev;
handler t1 close;
--echo #
--echo # FLUSH TABLES WITH READ LOCK behaves like FLUSH TABLE.
--echo # 
handler t1 open;
handler t1 read a prev;
flush tables with read lock;
handler t1 read a prev;
handler t1 close;
unlock tables;
--echo #
--echo # Let us also check that these operations behave in similar
--echo # way under LOCK TABLES.
--echo #
--echo # TRUNCATE under LOCK TABLES.
--echo #
handler t1 open;
lock tables t1 write;
truncate table t1;
unlock tables;
--error ER_UNKNOWN_TABLE
handler t1 read next;
handler t1 open;
--echo #
--echo # CREATE TRIGGER under LOCK TABLES.
--echo #
lock tables t1 write;
create trigger t1_ai after insert on t1 for each row set @a=1;
unlock tables;
--error ER_UNKNOWN_TABLE
handler t1 read next;
--echo #
--echo # DROP TRIGGER under LOCK TABLES.
--echo #
handler t1 open;
lock tables t1 write;
drop trigger t1_ai;
unlock tables;
--error ER_UNKNOWN_TABLE
handler t1 read next;
--echo #
--echo # ALTER TABLE under LOCK TABLES.
--echo # 
handler t1 open;
lock tables t1 write;
alter table t1 drop column b;
unlock tables;
--error ER_UNKNOWN_TABLE
handler t1 read next;
--echo #
--echo # ANALYZE TABLE under LOCK TABLES.
--echo # 
handler t1 open;
lock tables t1 write;
analyze table t1;
unlock tables;
--error ER_UNKNOWN_TABLE
handler t1 read next;
--echo #
--echo # OPTIMIZE TABLE under LOCK TABLES.
--echo # 
handler t1 open;
lock tables t1 write;
optimize table t1;
unlock tables;
--error ER_UNKNOWN_TABLE
handler t1 read next;
--echo #
--echo # REPAIR TABLE under LOCK TABLES.
--echo # 
handler t1 open;
lock tables t1 write;
repair table t1;
unlock tables;
--error ER_UNKNOWN_TABLE
handler t1 read next;
--echo #
--echo # DROP TABLE under LOCK TABLES, naturally.
--echo # 
handler t1 open;
lock tables t1 write;
drop table t1;
unlock tables;
--error ER_UNKNOWN_TABLE
handler t1 read next;
create table t1 (a int, b int, key a (a));
insert into t1 (a) values (1), (2), (3), (4), (5);
--echo #
--echo # FLUSH TABLE doesn't close the table but loses the position
--echo # 
handler t1 open;
handler t1 read a prev;
lock tables t1 write;
flush table t1;
unlock tables;
handler t1 read a prev;
handler t1 close;
--echo #
--echo # Explore the effect of HANDLER locks on concurrent DDL
--echo #
handler t1 open;
--echo # Establishing auxiliary connections con1, con2, con3
connect(con1, localhost, root,,);
connect(con2, localhost, root,,);
connect(con3, localhost, root,,);
--echo # --> connection con1;
connection con1;
--echo # Sending:
--send drop table t1 
--echo # We can't use connection 'default' as wait_condition will 
--echo # autoclose handlers.
--echo # --> connection con2 
connection con2;
--echo # Waitng for 'drop table t1' to get blocked...
let $wait_condition=select count(*)=1 from information_schema.processlist
                    where state='Waiting for table metadata lock' and
                          info='drop table t1';
--source include/wait_condition.inc
--echo # --> connection default
connection default;
--echo # Attempt to upgrade metadata locks to SR from S will lead to
--echo # deadlock which will result in table being automatically closed.
--error ER_NO_SUCH_TABLE
handler t1 read a prev;
handler t1 close;
--echo # --> connection con1
connection con1;
--echo # Reaping 'drop table t1'...
--reap
--echo # --> connection default
connection default;
--echo #
--echo # Explore the effect of HANDLER locks in parallel with SELECT 
--echo #
create table t1 (a int, key a (a));
insert into t1 (a) values (1), (2), (3), (4), (5);
begin;
select * from t1;
handler t1 open;
handler t1 read a prev;
handler t1 read a prev;
handler t1 close;
--echo # --> connection con1;
connection con1;
--echo # Sending:
--send drop table t1 
--echo # --> connection con2 
connection con2;
--echo # Waiting for 'drop table t1' to get blocked...
let $wait_condition=select count(*)=1 from information_schema.processlist
                    where state='Waiting for table metadata lock' and
                          info='drop table t1';
--source include/wait_condition.inc
--echo # --> connection default
connection default;
--echo # We can still use the table, it's part of the transaction
select * from t1;
--echo # Such are the circumstances that t1 is a part of transaction,
--echo # thus we can reopen it in the handler
handler t1 open;
--echo # We can commit the transaction, it doesn't close the handler
--echo # and doesn't let DROP to proceed immediately.
commit;
connection con2;
--echo # Waiting for 'drop table t1' to get blocked...
let $wait_condition=select count(*)=1 from information_schema.processlist
                    where state='Waiting for table metadata lock' and
                          info='drop table t1';
--source include/wait_condition.inc
--echo # --> connection default
connection default;
--echo # OTOH the first attempt to read from HANDLER will lead to metadata
--echo # locks deadlock and thus to HANDLER being automatically closed.
--error ER_NO_SUCH_TABLE
handler t1 read a prev;
handler t1 close;
--echo # --> connection con1
connection con1;
--echo # Reaping 'drop table t1'...
--reap
--echo # --> connection default
connection default;
--echo #
--echo # Demonstrate that HANDLER locks and transaction locks
--echo # reside in the same context.
--echo #
create table t1 (a int, key a (a));
insert into t1 (a) values (1), (2), (3), (4), (5);
create table t0 (a int, key a (a));
insert into t0 (a) values (1), (2), (3), (4), (5);
begin;
select * from t1;
--echo # --> connection con2 
connection con2;
--echo # Sending:
send rename table t0 to t3, t1 to t0, t3 to t1;
--echo # --> connection con1 
connection con1;
--echo # Waiting for 'rename table ...' to get blocked...
let $wait_condition=select count(*)=1 from information_schema.processlist
                    where state='Waiting for table metadata lock' and
                          info='rename table t0 to t3, t1 to t0, t3 to t1';
--source include/wait_condition.inc
--echo # --> connection default
connection default;
--echo # We back-off on hitting deadlock condition.
--error ER_LOCK_DEADLOCK
handler t0 open;
select * from t0;
handler t1 open;
commit;
handler t1 close;
--echo # --> connection con2
connection con2;
--echo # Reaping 'rename table ...'...
--reap
--echo # --> connection default
connection default;
handler t1 open;
handler t1 read a prev;
handler t1 close;
drop table t0;
--echo #
--echo # Originally there was a deadlock error in this test.
--echo # With implementation of deadlock detector
--echo # we no longer deadlock, but block and wait on a lock.
--echo # The HANDLER is auto-closed as soon as the connection
--echo # sees a pending conflicting lock against it.
--echo #
create table t2 (a int, key a (a));
handler t1 open;
--echo # --> connection con1
connection con1;
lock tables t2 read;
--echo # --> connection con2
connection con2;
--echo # Sending 'drop table t2'...
--send drop table t2
--echo # --> connection con1
connection con1;
--echo # Waiting for 'drop table t2' to get blocked...
let $wait_condition=select count(*)=1 from information_schema.processlist
                    where state='Waiting for table metadata lock' and
                          info='drop table t2';
--source include/wait_condition.inc
--echo # --> connection default
connection default;
--echo # Sending 'select * from t2'
send select * from t2;
--echo # --> connection con1
connection con1;
--echo # Waiting for 'select * from t2' to get blocked...
let $wait_condition=select count(*)=1 from information_schema.processlist
                    where state='Waiting for table metadata lock' and
                          info='select * from t2';
unlock tables;
--echo # --> connection con2
connection con2;
--echo # Reaping 'drop table t2'...
--reap
--echo # --> connection default
connection default;
--echo # Reaping 'select * from t2'
--error ER_NO_SUCH_TABLE
reap;
handler t1 close;

--echo #
--echo # ROLLBACK TO SAVEPOINT releases transactional locks,
--echo # but has no effect on open HANDLERs
--echo #
create table t2 like t1;
create table t3 like t1;
begin;
--echo # Have something before the savepoint
select * from t3;
savepoint sv;
handler t1 open;
handler t1 read a first;
handler t1 read a next;
select * from t2;
--echo # --> connection con1
connection con1;
--echo # Sending:
--send drop table t1
--echo # --> connection con2
connection con2;
--echo # Sending:
--send drop table t2
--echo # --> connection default
connection default;
--echo # Let DROP TABLE statements sync in. We must use
--echo # a separate connection for that, because otherwise SELECT
--echo # will auto-close the HANDLERs, becaues there are pending
--echo # exclusive locks against them.
--echo # --> connection con3
connection con3;
--echo # Waiting for 'drop table t1' to get blocked...
let $wait_condition=select count(*)=1 from information_schema.processlist
                    where state='Waiting for table metadata lock' and
                          info='drop table t1';
--source include/wait_condition.inc
--echo # Waiting for 'drop table t2' to get blocked...
let $wait_condition=select count(*)=1 from information_schema.processlist
                    where state='Waiting for table metadata lock' and
                          info='drop table t2';
--source include/wait_condition.inc
--echo # Demonstrate that t2 lock was released and t2 was dropped
--echo # after ROLLBACK TO SAVEPOINT
--echo # --> connection default
connection default;
rollback to savepoint sv;
--echo # --> connection con2
connection con2;
--echo # Reaping 'drop table t2'...
--reap
--echo # Demonstrate that ROLLBACK TO SAVEPOINT didn't release the handler
--echo # lock.
--echo # --> connection con3
connection con3;
--echo # Check if 'drop table t1' still blocked...
let $wait_condition=select count(*)=1 from information_schema.processlist
                    where state='Waiting for table metadata lock' and
                          info='drop table t1';
--source include/wait_condition.inc
--echo # --> connection default
connection default;
--echo # Demonstrate that the drop will go through as soon as we close
--echo # or will try to access HANDLER
--error ER_NO_SUCH_TABLE
handler t1 read a next;
handler t1 close;
--echo # connection con1
connection con1;
--echo # Reaping 'drop table t1'...
--reap
--echo # --> connection default
connection default;
commit;
drop table t3;
--echo #
--echo # A few special cases when using SAVEPOINT/ROLLBACK TO
--echo # SAVEPOINT and HANDLER.
--echo #
--echo # Show that rollback to the savepoint taken in the beginning
--echo # of the transaction doesn't release mdl lock on
--echo # the HANDLER that was opened later.
--echo #
create table t1 (a int, key a(a));
insert into t1 (a) values (1), (2), (3), (4), (5);
create table t2 like t1;
begin;
savepoint sv;
handler t1 open;
handler t1 read a first;
handler t1 read a next;
select * from t2;
--echo # --> connection con1
connection con1;
--echo # Sending:
--send drop table t1
--echo # --> connection con2
connection con2;
--echo # Sending:
--send drop table t2
--echo # --> connection default
connection default;
--echo # Let DROP TABLE statements sync in. We must use
--echo # a separate connection for that, because otherwise SELECT
--echo # will auto-close the HANDLERs, becaues there are pending
--echo # exclusive locks against them.
--echo # --> connection con3
connection con3;
--echo # Waiting for 'drop table t1' to get blocked...
let $wait_condition=select count(*)=1 from information_schema.processlist
                    where state='Waiting for table metadata lock' and
                          info='drop table t1';
--source include/wait_condition.inc
--echo # Waiting for 'drop table t2' to get blocked...
let $wait_condition=select count(*)=1 from information_schema.processlist
                    where state='Waiting for table metadata lock' and
                          info='drop table t2';
--source include/wait_condition.inc
--echo # Demonstrate that t2 lock was released and t2 was dropped
--echo # after ROLLBACK TO SAVEPOINT
--echo # --> connection default
connection default;
rollback to savepoint sv;
--echo # --> connection con2
connection con2;
--echo # Reaping 'drop table t2'...
--reap
--echo # Demonstrate that ROLLBACK TO SAVEPOINT didn't release the handler
--echo # lock.
--echo # --> connection con3
connection con3;
--echo # Check if 'drop table t1' is still blocked...
let $wait_condition=select count(*)=1 from information_schema.processlist
                    where state='Waiting for table metadata lock' and
                          info='drop table t1';
--source include/wait_condition.inc
--echo # --> connection default
connection default;
--echo # Demonstrate that the drop will go through as soon as we access or
--echo # close the HANDLER
--error ER_NO_SUCH_TABLE
handler t1 read a next;
handler t1 close;
--echo # connection con1
connection con1;
--echo # Reaping 'drop table t1'...
--reap
--echo # --> connection default
connection default;
commit;
--echo #
--echo # Show that rollback to the savepoint taken in the beginning
--echo # of the transaction works properly (no valgrind warnins, etc),
--echo # even though it's done after the HANDLER mdl lock that was there
--echo # at the beginning is released and added again.
--echo #
create table t1 (a int, key a(a));
insert into t1 (a) values (1), (2), (3), (4), (5);
create table t2 like t1;
create table t3 like t1;
insert into t3 (a) select a from t1;
begin;
handler t1 open;
savepoint sv;
handler t1 read a first;
select * from t2;
handler t1 close;
handler t3 open;
handler t3 read a first;
rollback to savepoint sv;
--echo # --> connection con1
connection con1;
drop table t1, t2;
--echo # Sending:
--send drop table t3
--echo # Let DROP TABLE statement sync in.
--echo # --> connection con2
connection con2;
--echo # Waiting for 'drop table t3' to get blocked...
let $wait_condition=select count(*)=1 from information_schema.processlist
                    where state='Waiting for table metadata lock' and
                          info='drop table t3';
--source include/wait_condition.inc
--echo # The fact that DROP TABLE is blocked means that ROLLBACK TO SAVEPOINT
--echo # didn't release the handler lock.
--echo # --> connection default
connection default;
--echo # Drop will go through as soon as we access or close the HANDLER
--error ER_NO_SUCH_TABLE
handler t3 read a next;
handler t3 close;
--echo # connection con1
connection con1;
--echo # Reaping 'drop table t3'...
--reap
--echo # --> connection default
connection default;
commit;

--echo # 
--echo # If we have to wait on an exclusive locks while having
--echo # an open HANDLER, ER_LOCK_DEADLOCK is reported.
--echo #
create table t1 (a int, key a(a));
create  table t2 like t1;
handler t1 open;
--echo # --> connection con1
connection con1;
lock table t1 write, t2 write;
--echo # --> connection default
connection default;
send drop table t2;
--echo # --> connection con2
connection con2;
--echo # Waiting for 'drop table t2' to get blocked...
let $wait_condition=select count(*)=1 from information_schema.processlist
                    where state='Waiting for table metadata lock' and
                          info='drop table t2';
--source include/wait_condition.inc
--echo # --> connection con1
connection con1;
--error ER_LOCK_DEADLOCK
drop table t1;
unlock tables;
--echo # --> connection default 
connection default;
reap;

--echo # Demonstrate that there is no deadlock with FLUSH TABLE,
--echo # even though it is waiting for the other table to go away
create table t2 like t1;
--echo # Sending:
--send flush table t2
--echo # --> connection con2
connection con2;
drop table t1;
--echo # --> connection con1
connection con1;
unlock tables;
--echo # --> connection default
connection default;
--echo # Reaping 'flush table t2'...
--reap
drop table t2;

--echo # 
--echo # Bug #46224 HANDLER statements within a transaction might 
--echo #            lead to deadlocks
--echo #
create table t1 (a int, key a(a));
insert into t1 values (1), (2);

--echo # --> connection default
connection default;
begin;
select * from t1;
handler t1 open;

--echo # --> connection con1
connection con1;
--echo # Sending:
--send lock tables t1 write

--echo # --> connection con2
connection con2;
--echo # Check that 'lock tables t1 write' waits until transaction which
--echo # has read from the table commits.
let $wait_condition=
  select count(*) = 1 from information_schema.processlist
  where state = "Waiting for table metadata lock" and
        info = "lock tables t1 write";
--source include/wait_condition.inc

--echo # --> connection default
connection default;
--echo # The below 'handler t1 read ...' should not be blocked as
--echo # 'lock tables t1 write' has not succeeded yet.
handler t1 read a next;

--echo # Unblock 'lock tables t1 write'.
commit;

--echo # --> connection con1
connection con1;
--echo # Reap 'lock tables t1 write'.
--reap

--echo # --> connection default
connection default;
--echo # Sending:
--send handler t1 read a next

--echo # --> connection con1
connection con1;
--echo # Waiting for 'handler t1 read a next' to get blocked...
let $wait_condition=
  select count(*) = 1 from information_schema.processlist
  where state = "Waiting for table metadata lock" and
        info = "handler t1 read a next";
--source include/wait_condition.inc

--echo # The below 'drop table t1' should be able to proceed without
--echo # waiting as it will force HANDLER to be closed.
drop table t1;
unlock tables;

--echo # --> connection default 
connection default;
--echo # Reaping 'handler t1 read a next'...
--error ER_NO_SUCH_TABLE
--reap
handler t1 close;

--echo # --> connection con1
connection con1;
disconnect con1;
--source include/wait_until_disconnected.inc
--echo # --> connection con2
connection con2;
disconnect con2;
--source include/wait_until_disconnected.inc
--echo # --> connection con3
connection con3;
disconnect con3;
--source include/wait_until_disconnected.inc
connection default;

--echo #
--echo # A temporary table test.
--echo # Check that we don't loose positions of HANDLER opened
--echo # against a temporary table.
--echo #
create table t1 (a int, b int, key a (a));
insert into t1 (a) values (1), (2), (3), (4), (5);
create temporary table t2 (a int, b int, key a (a));
insert into t2 (a) select a from t1;
handler t1 open;
handler t1 read a next;
handler t2 open;
handler t2 read a next;
flush table t1;
handler t2 read a next;
--echo # Sic: the position is lost
handler t1 read a next;
select * from t1;
--echo # Sic: the position is not lost
handler t2 read a next;
--error ER_CANT_REOPEN_TABLE
select * from t2;
handler t2 read a next;
drop table t1;
drop temporary table t2;

--echo #
--echo # A test for lock_table_names()/unlock_table_names() function.
--echo # It should work properly in presence of open HANDLER.
--echo #
create table t1 (a int, b int, key a (a));
create table t2 like t1;
create table t3 like t1;
create table t4 like t1;
handler t1 open;
handler t2 open;
rename table t4 to t5, t3 to t4, t5 to t3;
handler t1 read first;
handler t2 read first;
drop table t1, t2, t3, t4;

--echo #
--echo # A test for FLUSH TABLES WITH READ LOCK and HANDLER statements.
--echo #
set autocommit=0;
create table t1 (a int, b int, key a (a));
insert into t1 (a, b) values (1, 1), (2, 1), (3, 2), (4, 2), (5, 5);
create table t2 like t1;
insert into t2 (a, b) select a, b from t1;
create table t3 like t1;
insert into t3 (a, b) select a, b from t1;
commit;
flush tables with read lock;
handler t1 open;
lock table t1 read;
--error ER_LOCK_OR_ACTIVE_TRANSACTION
handler t1 read next;
--echo # This implicitly leaves LOCK TABLES but doesn't drop the GLR
--error ER_NO_SUCH_TABLE
lock table not_exists_write read;
--echo # We still have the read lock.
--error ER_CANT_UPDATE_WITH_READLOCK
drop table t1;
handler t1 open;
select a from t2;
handler t1 read next;
flush tables with read lock;
handler t2 open;
flush tables with read lock;
handler t1 read next;
select a from t3;
handler t2 read next;
handler t1 close;
rollback;
handler t2 close;
--error ER_CANT_UPDATE_WITH_READLOCK
drop table t1;
commit;
flush tables;
--error ER_CANT_UPDATE_WITH_READLOCK
drop table t1;
unlock tables;
drop table t1;
set autocommit=default;
drop table t2, t3;

--echo # 
--echo # HANDLER statement and operation-type aware metadata locks.
--echo # Check that when we clone a ticket for HANDLER we downrade
--echo # the lock.
--echo #
--echo # Establish an auxiliary connection con1.
connect (con1,localhost,root,,);
--echo # -> connection default
connection default;
create table t1 (a int, b int, key a (a));
insert into t1 (a, b) values (1, 1), (2, 1), (3, 2), (4, 2), (5, 5);
begin;
insert into t1 (a, b) values (6, 6);
handler t1 open;
handler t1 read a last;
insert into t1 (a, b) values (7, 7);
handler t1 read a last;
commit;
--echo # -> connection con1
connection con1;
--echo # Demonstrate that the HANDLER doesn't hold MDL_SHARED_WRITE.
lock table t1 write;
unlock tables;
--echo # -> connection default
connection default;
handler t1 read a prev;
handler t1 close;
--echo # Cleanup.
drop table t1;
--echo # -> connection con1
connection con1;
disconnect con1;
--source include/wait_until_disconnected.inc
--echo # -> connection default
connection default;

--echo #
--echo # A test for Bug#50555 "handler commands crash server in
--echo # my_hash_first()". 
--echo #
--error ER_UNKNOWN_TABLE
handler no_such_table read no_such_index first;
--error ER_UNKNOWN_TABLE
handler no_such_table close;


--echo #
--echo # Bug#50907 Assertion `hash_tables->table->next == __null' on 
--echo #           HANDLER OPEN
--echo #

--disable_warnings
DROP TABLE IF EXISTS t1, t2;
--enable_warnings

CREATE TEMPORARY TABLE t1 (i INT);
CREATE TEMPORARY TABLE t2 (i INT);

# This used to trigger the assert
HANDLER t2 OPEN;

# This also used to trigger the assert
HANDLER t2 READ FIRST;

HANDLER t2 CLOSE;
DROP TABLE t1, t2;


--echo #
--echo # Bug#50912 Assertion `ticket->m_type >= mdl_request->type'
--echo #           failed on HANDLER + I_S
--echo #

--disable_warnings
DROP TABLE IF EXISTS t1;
--enable_warnings

CREATE TABLE t1 (id INT);
HANDLER t1 OPEN;

# This used to trigger the assert.
SELECT table_name, table_comment FROM information_schema.tables
  WHERE table_schema= 'test' AND table_name= 't1';

HANDLER t1 CLOSE;
DROP TABLE t1;


--echo #
--echo # Test for bug #50908 "Assertion `handler_tables_hash.records == 0'
--echo #                      failed in enter_locked_tables_mode".
--echo #
--disable_warnings
drop tables if exists t1, t2;
drop function if exists f1;
--enable_warnings
create table t1 (i int);
insert into t1 values (1), (2);
create table t2 (j int);
insert into t2 values (1);
create function f1() returns int return (select count(*) from t2);
--echo # Check that open HANDLER survives statement executed in
--echo # prelocked mode.
handler t1 open;
handler t1 read next;
--echo # The below statement were aborted due to an assertion failure.
select f1() from t2;
handler t1 read next;
handler t1 close;
--echo # Check that the same happens under GLOBAL READ LOCK.
flush tables with read lock;
handler t1 open;
handler t1 read next;
select f1() from t2;
handler t1 read next;
unlock tables;
handler t1 close;
--echo # Now, check that the same happens if LOCK TABLES is executed.
handler t1 open;
handler t1 read next;
lock table t2 read;
select * from t2;
unlock tables;
handler t1 read next;
handler t1 close;
--echo # Finally, check scenario with GRL and LOCK TABLES.
flush tables with read lock;
handler t1 open;
handler t1 read next;
lock table t2 read;
select * from t2;
--echo # This unlocks both tables and GRL.
unlock tables;
handler t1 read next;
handler t1 close;
--echo # Clean-up.
drop function f1;
drop tables t1, t2;


--echo #
--echo # Test for bug #51136 "Crash in pthread_rwlock_rdlock on TEMPORARY +
--echo #                      HANDLER + LOCK + SP".
--echo # Also see additional coverage for this bug in flush.test.
--echo #
--disable_warnings
drop tables if exists t1, t2;
--enable_warnings
create table t1 (i int);
create temporary table t2 (j int);
handler t1 open;
lock table t2 read;
--echo # This commit should not release any MDL locks.
commit;
unlock tables;
--echo # The below statement crashed before the bug fix as it
--echo # has attempted to release metadata lock which was
--echo # already released by commit.
handler t1 close;
drop tables t1, t2;


--echo #
--echo # Bug#51355 handler stmt cause assertion in
--echo #           bool MDL_context::try_acquire_lock(MDL_request*)
--echo #

--disable_warnings
DROP TABLE IF EXISTS t1;
--enable_warnings

connect(con51355, localhost, root);

--echo # Connection default
connection default;
CREATE TABLE t1(id INT, KEY id(id));
HANDLER t1 OPEN;

--echo # Connection con51355
connection con51355;
--echo # Sending:
--send DROP TABLE t1

--echo # Connection default
connection default;
--echo # This I_S query will cause the handler table to be closed and
--echo # the metadata lock to be released. This will allow DROP TABLE
--echo # to proceed. Waiting for the table to be removed.
let $wait_condition=
  SELECT COUNT(*) = 0 FROM information_schema.tables WHERE table_name = "t1";
--source include/wait_condition.inc

--echo # Connection con51355
connection con51355;
--echo # Reaping: DROP TABLE t1
--reap

--echo # Connection default
connection default;
--error ER_NO_SUCH_TABLE
HANDLER t1 READ id NEXT;
# This caused an assertion
--error ER_NO_SUCH_TABLE
HANDLER t1 READ id NEXT;

HANDLER t1 CLOSE;
--echo # Connection con51355
connection con51355;
disconnect con51355;
--source include/wait_until_disconnected.inc
--echo # Connection default
connection default;


--echo #
--echo # Bug#54401 assert in Diagnostics_area::set_eof_status , HANDLER
--echo #

--disable_warnings
DROP TABLE IF EXISTS t1, t2;
DROP FUNCTION IF EXISTS f1;
--enable_warnings

delimiter |;
CREATE FUNCTION f1() RETURNS INTEGER
BEGIN
  SELECT 1 FROM t2 INTO @a;
  RETURN 1;
END|
delimiter ;|

# Get f1() parsed and cached
--error ER_NO_SUCH_TABLE
SELECT f1();

CREATE TABLE t1(a INT);
INSERT INTO t1 VALUES (1);
HANDLER t1 OPEN;
# This used to cause the assert
--error ER_NOT_SUPPORTED_YET
HANDLER t1 READ FIRST WHERE f1() = 1;
HANDLER t1 CLOSE;

DROP FUNCTION f1;
DROP TABLE t1;


--echo #
--echo # Bug#54920 Stored functions are allowed in HANDLER statements,
--echo #           but broken.
--echo #

--disable_warnings
DROP TABLE IF EXISTS t1;
DROP FUNCTION IF EXISTS f1;
--enable_warnings

CREATE TABLE t1 (a INT);
INSERT INTO t1 VALUES (1), (2);
CREATE FUNCTION f1() RETURNS INT RETURN 1;
HANDLER t1 OPEN;

--error ER_NOT_SUPPORTED_YET
HANDLER t1 READ FIRST WHERE f1() = 1;

HANDLER t1 CLOSE;
DROP FUNCTION f1;
DROP TABLE t1;

--echo #
--echo # Bug#13008220 HANDLER SQL STATEMENT CAN MISS TO INITIALIZE
--echo #              FOR RANDOM READ
--echo #
--echo # A handler can only have one active 'cursor' at a time,
--echo # so switching between index and/or random should restart the cursor.

CREATE TABLE t1(a INT, b INT, KEY b(b), KEY ab(a, b));
INSERT INTO t1 VALUES (2, 20), (1, 10), (4, 40), (3, 30);
HANDLER t1 OPEN;
HANDLER t1 READ b FIRST;
HANDLER t1 READ NEXT;
HANDLER t1 READ NEXT;
HANDLER t1 READ b FIRST;
HANDLER t1 READ b NEXT;
HANDLER t1 READ b NEXT;
HANDLER t1 READ FIRST;
HANDLER t1 READ b FIRST;
HANDLER t1 READ NEXT;
HANDLER t1 READ NEXT;
HANDLER t1 READ NEXT;
HANDLER t1 READ NEXT;
HANDLER t1 READ NEXT;
HANDLER t1 READ b NEXT;
HANDLER t1 READ b NEXT;
HANDLER t1 READ b NEXT;
HANDLER t1 READ b NEXT;
HANDLER t1 READ b NEXT;
HANDLER t1 READ NEXT;
HANDLER t1 READ b NEXT;
HANDLER t1 READ FIRST;
HANDLER t1 READ b PREV;
HANDLER t1 READ b LAST;
HANDLER t1 READ NEXT;
HANDLER t1 READ ab FIRST;
HANDLER t1 READ b NEXT;
HANDLER t1 READ ab LAST;
HANDLER t1 READ b PREV;
HANDLER t1 CLOSE;
DROP TABLE t1;

--echo #
--echo # Bug#21650603 ASSERT `0' FAILED AT THD::SEND_STATEMENT_STATUS() ON
--echo #              HANDLER READ + INVALID DATE
--echo #
CREATE TABLE t (
  col_date DATE
  , col_datetime DATETIME
  , col_time TIME
  , col_year YEAR
  , KEY col_date_key (col_date)
  , KEY col_datetime_key (col_datetime)
  , KEY col_year_key (col_year));

HANDLER t OPEN;

--error ER_WRONG_ARGUMENTS
HANDLER t READ col_date_key <= ('p');

--error ER_WRONG_ARGUMENTS
HANDLER t READ col_datetime_key <= ('p');

--error ER_WRONG_ARGUMENTS
HANDLER t READ col_year_key <= ('p');

DROP TABLE t;
