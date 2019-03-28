drop table if exists t1,t2;
drop database if exists mysqltest;
create temporary table t1(n int not null primary key);
create table t2(n int);
insert into t2 values(3);
select * from t1;
n
3
flush tables with read lock;
drop table t2;
ERROR HY000: Can't execute the query because you have a conflicting read lock
drop table t2;
unlock tables;
create database mysqltest;
create table mysqltest.t1(n int);
insert into mysqltest.t1 values (23);
flush tables with read lock;
drop database mysqltest;
select * from mysqltest.t1;
n
23
unlock tables;
create table t1 (n int);
flush tables with read lock;
insert into t1 values (345);
select * from t1;
n
345
drop table t1;
create table t1 (c1 int);
lock table t1 write;
flush tables with read lock;
ERROR HY000: Can't execute the given command because you have active locked tables or an active transaction
lock table t1 read;
flush tables with read lock;
ERROR HY000: Can't execute the given command because you have active locked tables or an active transaction
unlock tables;
flush tables with read lock;
lock table t1 write;
ERROR HY000: Can't execute the query because you have a conflicting read lock
lock table t1 read;
lock table t1 write;
ERROR HY000: Can't execute the query because you have a conflicting read lock
unlock tables;
create table t2 (c1 int);
create table t3 (c1 int);
lock table t1 read, t2 read, t3 write;
flush tables with read lock;
ERROR HY000: Can't execute the given command because you have active locked tables or an active transaction
lock table t1 read, t2 read, t3 read;
flush tables with read lock;
ERROR HY000: Can't execute the given command because you have active locked tables or an active transaction
unlock tables;
drop table t1, t2, t3;
create table t1 (c1 int);
create table t2 (c1 int);
lock table t1 write;
flush tables with read lock;
insert into t2 values(1);
unlock tables;
drop table t1, t2;
drop table if exists t1, t2;
set session low_priority_updates=1;
create table t1 (a int);
create table t2 (b int);
lock tables t1 write;
flush tables with read lock;
ERROR HY000: Can't execute the given command because you have active locked tables or an active transaction
unlock tables;
lock tables t1 read, t2 write;
flush tables with read lock;
ERROR HY000: Can't execute the given command because you have active locked tables or an active transaction
unlock tables;
lock tables t1 read;
flush tables with read lock;
ERROR HY000: Can't execute the given command because you have active locked tables or an active transaction
unlock tables;
drop table t1, t2;
set session low_priority_updates=default;
End of 5.0 tests
set @old_general_log= @@general_log;
set @old_read_only= @@read_only;
set global general_log= on;
flush tables with read lock;
flush logs;
unlock tables;
set global read_only=1;
flush logs;
unlock tables;
flush tables with read lock;
flush logs;
unlock tables;
set global general_log= @old_general_log;
set global read_only= @old_read_only;
End of 5.1 tests
#
# Additional test for bug #51136 "Crash in pthread_rwlock_rdlock
#                                 on TEMPORARY + HANDLER + LOCK + SP".
# Also see the main test for this bug in include/handler.inc.
#
drop tables if exists t1, t2;
create table t1 (i int);
create temporary table t2 (j int);
flush tables with read lock;
lock table t2 read;
# This commit should not release any MDL locks.
commit;
# The below statement crashed before the bug fix as it
# has attempted to release global shared metadata lock
# which was already released by commit.
unlock tables;
drop tables t1, t2;
# 
# Tests for WL#5000  FLUSH TABLES|TABLE table_list WITH READ LOCK
#
# I. Check the incompatible changes in the grammar.
#
flush tables with read lock, hosts;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near ', hosts' at line 1
flush privileges, tables;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'tables' at line 1
flush privileges, tables with read lock;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'tables with read lock' at line 1
flush privileges, tables;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'tables' at line 1
flush tables with read lock, tables;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near ', tables' at line 1
show tables;
Tables_in_test
#
# II. Check the allowed syntax.
#
drop table if exists t1, t2, t3;
create table t1 (a int);
create table t2 (a int);
create table t3 (a int);
lock table t1 read, t2 read;
flush tables with read lock;
ERROR HY000: Can't execute the given command because you have active locked tables or an active transaction
unlock tables;
flush tables with read lock;
flush tables t1, t2 with read lock;
flush tables t1, t2 with read lock;
ERROR HY000: Can't execute the given command because you have active locked tables or an active transaction
flush tables with read lock;
ERROR HY000: Can't execute the given command because you have active locked tables or an active transaction
select * from t1;
a
select * from t2;
a
select * from t3;
ERROR HY000: Table 't3' was not locked with LOCK TABLES
insert into t1 (a) values (1);
ERROR HY000: Table 't1' was locked with a READ lock and can't be updated
insert into t2 (a) values (1);
ERROR HY000: Table 't2' was locked with a READ lock and can't be updated
insert into t3 (a) values (1);
ERROR HY000: Table 't3' was not locked with LOCK TABLES
lock table no_such_table read;
ERROR 42S02: Table 'test.no_such_table' doesn't exist
#
# We implicitly left the locked tables
# mode but still have the read lock.
# 
insert into t2 (a) values (1);
ERROR HY000: Can't execute the query because you have a conflicting read lock
unlock tables;
insert into t1 (a) values (1);
insert into t2 (a) values (1);
flush table t1, t2 with read lock;
select * from t1;
a
1
select * from t2;
a
1
select * from t3;
ERROR HY000: Table 't3' was not locked with LOCK TABLES
insert into t1 (a) values (2);
ERROR HY000: Table 't1' was locked with a READ lock and can't be updated
insert into t2 (a) values (2);
ERROR HY000: Table 't2' was locked with a READ lock and can't be updated
insert into t3 (a) values (2);
ERROR HY000: Table 't3' was not locked with LOCK TABLES
lock table no_such_table read;
ERROR 42S02: Table 'test.no_such_table' doesn't exist
insert into t3 (a) values (2);
# 
# III. Concurrent tests.
#
# --> connection default
#
# Check that flush tables <list> with read lock
# does not affect non-locked tables.
#
flush tables t1 with read lock;
# --> connection con1;
select * from t1;
a
1
select * from t2;
a
1
insert into t2 (a) values (3);
# --> connection default;
unlock tables;
#
# Check that "FLUSH TABLES <list> WITH READ LOCK" is
# compatible with active "FLUSH TABLES WITH READ LOCK".
# Vice versa it is not true, since tables read-locked by
# "FLUSH TABLES <list> WITH READ LOCK" can't be flushed.
flush tables with read lock;
# --> connection con1;
flush table t1 with read lock;
select * from t1;
a
1
unlock tables;
# --> connection default;
unlock tables;
#
# Check that FLUSH TABLES t1 WITH READ LOCK
# does not conflict with an existing FLUSH TABLES t2
# WITH READ LOCK.
#
flush table t1 with read lock;
# --> connection con1
flush table t2 with read lock;
unlock tables;
# --> connection default
unlock tables;
#
# Check that FLUSH TABLES t1 WITH READ LOCK
# does not conflict with SET GLOBAL read_only=1.
#
set global read_only=1;
# connection con1
flush table t1 with read lock;
unlock tables;
# connection default
set global read_only=0;
#
# Check that it's possible to read-lock 
# tables locked with FLUSH TABLE <list> WITH READ LOCK.
#
flush tables t1, t2 with read lock;
# connection con1
lock table t1 read, t2 read;
unlock tables;
# connection default
unlock tables;
# --> connection con1
drop table t1, t2, t3;
#
# Bug#51710 FLUSH TABLES <view> WITH READ LOCK kills the server
#
drop view if exists v1, v2, v3;
drop table if exists t1, v1;
create table t1 (a int);
create view v1 as select 1;
create view v2 as select * from t1;
create view v3 as select * from v2;
flush table v1, v2, v3 with read lock;
ERROR HY000: 'test.v1' is not BASE TABLE
flush table v1 with read lock;
ERROR HY000: 'test.v1' is not BASE TABLE
flush table v2 with read lock;
ERROR HY000: 'test.v2' is not BASE TABLE
flush table v3 with read lock;
ERROR HY000: 'test.v3' is not BASE TABLE
create temporary table v1 (a int);
flush table v1 with read lock;
ERROR HY000: 'test.v1' is not BASE TABLE
drop view v1;
create table v1 (a int);
flush table v1 with read lock;
drop temporary table v1;
unlock tables;
drop view v2, v3;
drop table t1, v1;
# 
# FLUSH TABLES <list> WITH READ LOCK and HANDLER
# 
drop table if exists t1;
create table t1 (a int, key a (a));
insert into t1 (a) values (1), (2), (3);
handler t1 open;
handler t1 read a next;
a
1
handler t1 read a next;
a
2
flush tables t1 with read lock;
handler t1 read a next;
ERROR HY000: Can't execute the given command because you have active locked tables or an active transaction
unlock tables;
#
# Sic: lost handler position.
#
handler t1 read a next;
a
1
handler t1 close;
drop table t1;
#
# Bug#52117 Pending FLUSH TALBES <list> aborts
# transactions unnecessarily.
#
drop table if exists t1;
# --> conection default
create table t1 (a int);
begin;
select * from t1;
a
# --> connection con1
# 
# Issue a LOCK TABLE t1 READ. We could use HANDLER t1 OPEN
# or a long-running select -- anything that
# prevents FLUSH TABLE t1 from immediate completion would do.
#
lock table t1 read;
# --> connection con2
#
# FLUSH TABLE expels the table definition from the cache.
# Sending 'flush table t1'...
flush table t1;
# --> connection default
# Let flush table sync in.
select * from t1;
# --> connection con1 
select * from t1;
a
unlock tables;
# --> connection con2 
# Reaping 'flush table t1'...
# --> connection default 
# Reaping 'select * from t1'...
a
commit;
#
# Repeat the same test but with FLUSH TABLES
#
begin;
select * from t1;
a
# --> connection con1
# 
# Issue a LOCK TABLE t1 READ.
#
lock table t1 read;
# --> connection con2
#
# FLUSH TABLES expels the table definition from the cache.
# Sending 'flush tables'...
flush tables;
# --> connection default
# Let flush table sync in.
select * from t1;
# --> connection con1 
select * from t1;
a
unlock tables;
# --> connection con2 
# Reaping 'flush tables'...
# --> connection default 
# Reaping 'select * from t1'...
a
commit;
# Cleanup
# --> connection con1
# --> connection con2
# --> connection default
drop table t1;
#
# Test for bug #57006 "Deadlock between HANDLER and
#                      FLUSH TABLES WITH READ LOCK".
#
drop table if exists t1, t2;
create table t1 (i int);
create table t2 (i int);
handler t1 open;
# Switching to connection 'con1'.
# Sending:
flush tables with read lock;
# Switching to connection 'con2'.
# Wait until FTWRL starts waiting for 't1' to be closed.
# Switching to connection 'default'.
# The below statement should not cause deadlock.
# Sending:
insert into t2 values (1);
# Switching to connection 'con2'.
# Wait until INSERT starts to wait for FTWRL to go away.
# Switching to connection 'con1'.
# FTWRL should be able to continue now.
# Reap FTWRL.
unlock tables;
# Switching to connection 'default'.
# Reap INSERT.
handler t1 close;
# Cleanup.
drop tables t1, t2;
#
# Bug#57649 FLUSH TABLES under FLUSH TABLES <list> WITH READ LOCK leads
#           to assert failure.
#
DROP TABLE IF EXISTS t1;
CREATE TABLE t1 (a INT);
FLUSH TABLES t1 WITH READ LOCK;
FLUSH TABLES;
ERROR HY000: Table 't1' was locked with a READ lock and can't be updated
CREATE TRIGGER t1_bi BEFORE INSERT ON t1 FOR EACH ROW SET @a= 1;
ERROR HY000: Table 't1' was locked with a READ lock and can't be updated
ALTER TABLE t1 COMMENT 'test';
ERROR HY000: Table 't1' was locked with a READ lock and can't be updated
UNLOCK TABLES;
DROP TABLE t1;
#
# Test for bug #12641342 - "61401: UPDATE PERFORMANCE DEGRADES
#                           GRADUALLY IF A TRIGGER EXISTS".
#
# One of side-effects of this bug was that a transaction which
# involved DML statements requiring prelocking blocked concurrent
# FLUSH TABLES WITH READ LOCK for the whole its duration, while
# correct behavior in this case is to block FTWRL only for duration
# of individual DML statements.
DROP TABLE IF EXISTS t1;
CREATE TABLE t1 (id INT PRIMARY KEY, value INT);
INSERT INTO t1 VALUES (1, 1);
CREATE TRIGGER t1_au AFTER UPDATE ON t1 FOR EACH ROW SET @var = "a";
BEGIN;
UPDATE t1 SET value= value + 1 WHERE id = 1;
# Switching to connection 'con1'.
# The below FLUSH TABLES WITH READ LOCK should succeed and
# should not be blocked by the transaction in default connection.
FLUSH TABLES WITH READ LOCK;
UNLOCK TABLES;
# Switching to connection 'default'.
COMMIT;
DROP TABLE t1;
