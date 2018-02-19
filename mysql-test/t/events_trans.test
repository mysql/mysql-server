#
# Tests that require transactions
#

--disable_warnings
drop database if exists events_test;
drop database if exists mysqltest_no_such_database;
--enable_warnings
create database events_test;
use events_test;

--echo
--echo Test that Events DDL issue an implicit COMMIT
--echo
--echo
set autocommit=off;
# Sanity check
select @@autocommit;
create table t1 (a varchar(255)) engine=innodb;
# Basic: check that successful Events DDL commits pending transaction
begin work;
insert into t1 (a) values ("OK: create event");
create event e1 on schedule every 1 day do select 1;
rollback work;
select * from t1;
delete from t1;
commit work;
#
begin work;
insert into t1 (a) values ("OK: alter event");
alter event e1 on schedule every 2 day do select 2;
rollback work;
select * from t1;
delete from t1;
commit work;
#
begin work;
insert into t1 (a) values ("OK: alter event rename");
alter event e1 rename to e2;
rollback work;
select * from t1;
delete from t1;
commit work;
#
begin work;
insert into t1 (a) values ("OK: drop event");
drop event e2;
rollback work;
select * from t1;
delete from t1;
commit work;
#
begin work;
insert into t1 (a) values ("OK: drop event if exists");
drop event if exists e2;
rollback work;
select * from t1;
delete from t1;
commit work;
#
create event e1 on schedule every 1 day do select 1;
begin work;
insert into t1 (a) values ("OK: create event if not exists");
create event if not exists e1 on schedule every 2 day do select 2;
rollback work;
select * from t1;
delete from t1;
commit work;
--echo
--echo Now check various error conditions: make sure we issue an
--echo implicit commit anyway
--echo
#
begin work;
insert into t1 (a) values ("OK: create event: event already exists");
--error ER_EVENT_ALREADY_EXISTS
create event e1 on schedule every 2 day do select 2;
rollback work;
select * from t1;
delete from t1;
commit work;
#
begin work;
insert into t1 (a) values ("OK: alter event rename: rename to same name");
--error ER_EVENT_SAME_NAME
alter event e1 rename to e1;
rollback work;
select * from t1;
delete from t1;
commit work;
#
create event e2 on schedule every 3 day do select 3;
begin work;
insert into t1 (a) values ("OK: alter event rename: destination exists");
--error ER_EVENT_ALREADY_EXISTS
alter event e2 rename to e1;
rollback work;
select * from t1;
delete from t1;
commit work;
#
begin work;
insert into t1 (a) values ("OK: create event: database does not exist");
--error ER_BAD_DB_ERROR
create event mysqltest_no_such_database.e1 on schedule every 1 day do select 1;
rollback work;
select * from t1;
delete from t1;
commit work;

#
# Cleanup
#

let $wait_condition=
  select count(*) = 0 from information_schema.processlist
  where db='events_test' and command = 'Connect' and user=current_user();
--source include/wait_condition.inc



--echo #
--echo # Bug#54105 assert in MDL_context::release_locks_stored_before
--echo #

USE test;

--disable_warnings
DROP TABLE IF EXISTS t1, t2;
DROP EVENT IF EXISTS e1;
--enable_warnings

CREATE TABLE t1 (a INT) ENGINE=InnoDB;
CREATE TABLE t2 (a INT);
CREATE EVENT e1 ON SCHEDULE EVERY 1 DAY DO SELECT 1;

START TRANSACTION;
INSERT INTO t1 VALUES (1);
SAVEPOINT A;
--replace_regex /STARTS '[^']+'/STARTS '#'/
SHOW CREATE EVENT e1;
SELECT * FROM t2;
ROLLBACK WORK TO SAVEPOINT A;

DROP TABLE t1, t2;
DROP EVENT e1;
#
# Tests that require transactions
#
--disable_warnings
drop database if exists mysqltest_db2;
--enable_warnings
use events_test;
#
# Privilege checks
#
create user mysqltest_user1@localhost;
grant create, insert, select, delete on mysqltest_db2.*
  to mysqltest_user1@localhost;
create database mysqltest_db2;
connect (conn1,localhost,mysqltest_user1,,mysqltest_db2);
set autocommit=off;
# Sanity check
select @@autocommit;
create table t1 (a varchar(255)) engine=innodb;
# Not enough privileges to CREATE EVENT
begin work;
insert into t1 (a) values ("OK: create event: insufficient privileges");
--error ER_DBACCESS_DENIED_ERROR
create event e1 on schedule every 1 day do select 1;
rollback work;
select * from t1;
delete from t1;
commit work;
# Not enough privileges to ALTER EVENT
begin work;
insert into t1 (a) values ("OK: alter event: insufficient privileges");
--error ER_DBACCESS_DENIED_ERROR
alter event e1 on schedule every 1 day do select 1;
rollback work;
select * from t1;
delete from t1;
commit work;
# Not enough privileges to DROP EVENT
begin work;
insert into t1 (a) values ("OK: drop event: insufficient privileges");
--error ER_DBACCESS_DENIED_ERROR
drop event e1;
rollback work;
select * from t1;
delete from t1;
commit work;
# Cleanup
disconnect conn1;
--source include/wait_until_disconnected.inc
connection default;
drop user mysqltest_user1@localhost;
drop database mysqltest_db2;

#
# Cleanup
#
let $wait_condition=
  select count(*) = 0 from information_schema.processlist
  where db='events_test' and command = 'Connect' and user=current_user();
--source include/wait_condition.inc

drop database events_test;
