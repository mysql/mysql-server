drop database if exists events_test;
drop database if exists mysqltest_no_such_database;
create database events_test;
use events_test;

Test that Events DDL issue an implicit COMMIT


set autocommit=off;
select @@autocommit;
@@autocommit
0
create table t1 (a varchar(255)) engine=innodb;
begin work;
insert into t1 (a) values ("OK: create event");
create event e1 on schedule every 1 day do select 1;
rollback work;
select * from t1;
a
OK: create event
delete from t1;
commit work;
begin work;
insert into t1 (a) values ("OK: alter event");
alter event e1 on schedule every 2 day do select 2;
rollback work;
select * from t1;
a
OK: alter event
delete from t1;
commit work;
begin work;
insert into t1 (a) values ("OK: alter event rename");
alter event e1 rename to e2;
rollback work;
select * from t1;
a
OK: alter event rename
delete from t1;
commit work;
begin work;
insert into t1 (a) values ("OK: drop event");
drop event e2;
rollback work;
select * from t1;
a
OK: drop event
delete from t1;
commit work;
begin work;
insert into t1 (a) values ("OK: drop event if exists");
drop event if exists e2;
Warnings:
Note	1305	Event e2 does not exist
rollback work;
select * from t1;
a
OK: drop event if exists
delete from t1;
commit work;
create event e1 on schedule every 1 day do select 1;
begin work;
insert into t1 (a) values ("OK: create event if not exists");
create event if not exists e1 on schedule every 2 day do select 2;
Warnings:
Note	1537	Event 'e1' already exists
rollback work;
select * from t1;
a
OK: create event if not exists
delete from t1;
commit work;

Now check various error conditions: make sure we issue an
implicit commit anyway

begin work;
insert into t1 (a) values ("OK: create event: event already exists");
create event e1 on schedule every 2 day do select 2;
ERROR HY000: Event 'e1' already exists
rollback work;
select * from t1;
a
OK: create event: event already exists
delete from t1;
commit work;
begin work;
insert into t1 (a) values ("OK: alter event rename: rename to same name");
alter event e1 rename to e1;
ERROR HY000: Same old and new event name
rollback work;
select * from t1;
a
OK: alter event rename: rename to same name
delete from t1;
commit work;
create event e2 on schedule every 3 day do select 3;
begin work;
insert into t1 (a) values ("OK: alter event rename: destination exists");
alter event e2 rename to e1;
ERROR HY000: Event 'e1' already exists
rollback work;
select * from t1;
a
OK: alter event rename: destination exists
delete from t1;
commit work;
begin work;
insert into t1 (a) values ("OK: create event: database does not exist");
create event mysqltest_no_such_database.e1 on schedule every 1 day do select 1;
ERROR 42000: Unknown database 'mysqltest_no_such_database'
rollback work;
select * from t1;
a
OK: create event: database does not exist
delete from t1;
commit work;
#
# Bug#54105 assert in MDL_context::release_locks_stored_before
#
USE test;
DROP TABLE IF EXISTS t1, t2;
DROP EVENT IF EXISTS e1;
CREATE TABLE t1 (a INT) ENGINE=InnoDB;
CREATE TABLE t2 (a INT);
CREATE EVENT e1 ON SCHEDULE EVERY 1 DAY DO SELECT 1;
START TRANSACTION;
INSERT INTO t1 VALUES (1);
SAVEPOINT A;
SHOW CREATE EVENT e1;
Event	sql_mode	time_zone	Create Event	character_set_client	collation_connection	Database Collation
e1	ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION	SYSTEM	CREATE DEFINER=`root`@`localhost` EVENT `e1` ON SCHEDULE EVERY 1 DAY STARTS '#' ON COMPLETION NOT PRESERVE ENABLE DO SELECT 1	utf8mb4	utf8mb4_0900_ai_ci	utf8mb4_0900_ai_ci
SELECT * FROM t2;
a
ROLLBACK WORK TO SAVEPOINT A;
DROP TABLE t1, t2;
DROP EVENT e1;
drop database if exists mysqltest_db2;
use events_test;
create user mysqltest_user1@localhost;
grant create, insert, select, delete on mysqltest_db2.*
to mysqltest_user1@localhost;
create database mysqltest_db2;
set autocommit=off;
select @@autocommit;
@@autocommit
0
create table t1 (a varchar(255)) engine=innodb;
begin work;
insert into t1 (a) values ("OK: create event: insufficient privileges");
create event e1 on schedule every 1 day do select 1;
ERROR 42000: Access denied for user 'mysqltest_user1'@'localhost' to database 'mysqltest_db2'
rollback work;
select * from t1;
a
OK: create event: insufficient privileges
delete from t1;
commit work;
begin work;
insert into t1 (a) values ("OK: alter event: insufficient privileges");
alter event e1 on schedule every 1 day do select 1;
ERROR 42000: Access denied for user 'mysqltest_user1'@'localhost' to database 'mysqltest_db2'
rollback work;
select * from t1;
a
OK: alter event: insufficient privileges
delete from t1;
commit work;
begin work;
insert into t1 (a) values ("OK: drop event: insufficient privileges");
drop event e1;
ERROR 42000: Access denied for user 'mysqltest_user1'@'localhost' to database 'mysqltest_db2'
rollback work;
select * from t1;
a
OK: drop event: insufficient privileges
delete from t1;
commit work;
drop user mysqltest_user1@localhost;
drop database mysqltest_db2;
drop database events_test;
