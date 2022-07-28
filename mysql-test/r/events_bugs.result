drop database if exists events_test;
drop database if exists mysqltest_db1;
drop database if exists mysqltest_db2;
create database events_test;
use events_test;
set @concurrent_insert= @@global.concurrent_insert;
set @@global.concurrent_insert = 0;
select * from performance_schema.global_variables where variable_name like 'event_scheduler';
VARIABLE_NAME	VARIABLE_VALUE
event_scheduler	ON
SET GLOBAL event_scheduler = 'OFF';
CREATE EVENT lower_case ON SCHEDULE EVERY 1 MINUTE DO SELECT 1;
CREATE EVENT Lower_case ON SCHEDULE EVERY 2 MINUTE DO SELECT 2;
ERROR HY000: Event 'Lower_case' already exists
DROP EVENT Lower_case;
SET NAMES cp1251;
CREATE EVENT �����_��������_1251 ON SCHEDULE EVERY 1 YEAR DO SELECT 100;
CREATE EVENT �����_��������_1251 ON SCHEDULE EVERY 2 YEAR DO SELECT 200;
ERROR HY000: Event '�����_��������_1251' already exists
DROP EVENT �����_��������_1251;
SET NAMES utf8;
Warnings:
Warning	3719	'utf8' is currently an alias for the character set UTF8MB3, but will be an alias for UTF8MB4 in a future release. Please consider using UTF8MB4 in order to be unambiguous.
CREATE EVENT долен_регистър_утф8 ON SCHEDULE EVERY 3 YEAR DO SELECT 300;
CREATE EVENT ДОЛЕН_регистър_утф8 ON SCHEDULE EVERY 4 YEAR DO SELECT 400;
ERROR HY000: Event 'ДОЛЕН_регистър_утф8' already exists
DROP EVENT ДОЛЕН_регистър_утф8;
SET NAMES latin1;
set @a=3;
CREATE PROCEDURE p_16 () CREATE EVENT e_16 ON SCHEDULE EVERY @a SECOND DO SET @a=5;
ERROR HY000: Recursion of EVENT DDL statements is forbidden when body is present
create event e_55 on schedule at 99990101000000 do drop table t;
ERROR HY000: Incorrect AT value: '99990101000000'
create event e_55 on schedule every 10 hour starts 99990101000000 do drop table t;
ERROR HY000: Incorrect STARTS value: '99990101000000'
create event e_55 on schedule every 10 minute ends 99990101000000 do drop table t;
ERROR HY000: ENDS is either invalid or before STARTS
create event e_55 on schedule at 10000101000000 do drop table t;
ERROR HY000: Incorrect AT value: '10000101000000'
create event e_55 on schedule at 20000101000000 do drop table t;
Warnings:
Note	1588	Event execution time is in the past and ON COMPLETION NOT PRESERVE is set. The event was dropped immediately after creation.
show events;
Db	Name	Definer	Time zone	Type	Execute at	Interval value	Interval field	Starts	Ends	Status	Originator	character_set_client	collation_connection	Database Collation
create event e_55 on schedule at 20380101000000 starts 10000101000000 do drop table t;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'starts 10000101000000 do drop table t' at line 1
create event e_55 on schedule at 20380101000000 ends 10000101000000 do drop table t;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'ends 10000101000000 do drop table t' at line 1
create event e_55 on schedule at 20380101000000 starts 10000101000000 ends 10000101000000 do drop table t;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'starts 10000101000000 ends 10000101000000 do drop table t' at line 1
create event e_55 on schedule every 10 hour starts 10000101000000 do drop table t;
ERROR HY000: Incorrect STARTS value: '10000101000000'
set global event_scheduler=off;
set global event_scheduler= on;
set @old_sql_mode:=@@sql_mode;
set sql_mode=ansi;
select get_lock('test_bug16407', 60);
get_lock('test_bug16407', 60)
1
create event e_16407 on schedule every 60 second do
begin
select get_lock('test_bug16407', 60);
end|
"Now if everything is fine the event has compiled and is locked"
select /*1*/ user, host, db, info from information_schema.processlist
where state = 'User lock' and info = 'select get_lock(\'test_bug16407\', 60)';
user	host	db	info
root	localhost	events_test	select get_lock('test_bug16407', 60)
select release_lock('test_bug16407');
release_lock('test_bug16407')
1
set global event_scheduler= off;
select event_schema, event_name, sql_mode from information_schema.events order by event_schema, event_name;
EVENT_SCHEMA	EVENT_NAME	SQL_MODE
events_test	e_16407	REAL_AS_FLOAT,PIPES_AS_CONCAT,ANSI_QUOTES,IGNORE_SPACE,ONLY_FULL_GROUP_BY,ANSI
"Let's check whether we change the sql_mode on ALTER EVENT"
set sql_mode='traditional';
alter event e_16407 do select 1;
select event_schema, event_name, sql_mode from information_schema.events order by event_schema, event_name;
EVENT_SCHEMA	EVENT_NAME	SQL_MODE
events_test	e_16407	STRICT_TRANS_TABLES,STRICT_ALL_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,TRADITIONAL,NO_ENGINE_SUBSTITUTION
drop event e_16407;
set sql_mode="ansi";
select get_lock('ee_16407_2', 60);
get_lock('ee_16407_2', 60)
1
set global event_scheduler= 1;
"Another sql_mode test"
set sql_mode="traditional";
create table events_smode_test(ev_name char(10), a date);
"This should never insert something"
create event ee_16407_2 on schedule every 60 second do
begin
select get_lock('ee_16407_2', 60) /*ee_16407_2*/;
select release_lock('ee_16407_2');
insert into events_test.events_smode_test values('ee_16407_2','1980-19-02');
end|
insert into events_test.events_smode_test values ('test','1980-19-02')|
ERROR 22007: Incorrect date value: '1980-19-02' for column 'a' at row 1
"This is ok"
create event ee_16407_3 on schedule every 60 second do
begin
select get_lock('ee_16407_2', 60) /*ee_16407_3*/;
select release_lock('ee_16407_2');
insert into events_test.events_smode_test values ('ee_16407_3','1980-02-19');
insert into events_test.events_smode_test values ('ee_16407_3','1980-02-29');
end|
set sql_mode=""|
"This will insert rows but they will be truncated"
create event ee_16407_4 on schedule every 60 second do
begin
select get_lock('ee_16407_2', 60) /*ee_16407_4*/;
select release_lock('ee_16407_2');
insert into events_test.events_smode_test values ('ee_16407_4','10-11-1956');
end|
select event_schema, event_name, sql_mode from information_schema.events order by event_schema, event_name;
EVENT_SCHEMA	EVENT_NAME	SQL_MODE
events_test	ee_16407_2	STRICT_TRANS_TABLES,STRICT_ALL_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,TRADITIONAL,NO_ENGINE_SUBSTITUTION
events_test	ee_16407_3	STRICT_TRANS_TABLES,STRICT_ALL_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,TRADITIONAL,NO_ENGINE_SUBSTITUTION
events_test	ee_16407_4	
select /*2*/ user, host, db, info from information_schema.processlist
where state = 'User lock' and info = 'select get_lock(\'ee_16407_2\', 60)';
user	host	db	info
root	localhost	events_test	select get_lock('ee_16407_2', 60)
root	localhost	events_test	select get_lock('ee_16407_2', 60)
root	localhost	events_test	select get_lock('ee_16407_2', 60)
select release_lock('ee_16407_2');
release_lock('ee_16407_2')
1
select /*3*/ user, host, db, info from information_schema.processlist
where state = 'User lock' and info = 'select get_lock(\'ee_16407_2\', 60)';
user	host	db	info
set global event_scheduler= off;
select * from events_test.events_smode_test order by ev_name, a;
ev_name	a
ee_16407_3	1980-02-19
ee_16407_3	1980-02-29
ee_16407_4	0000-00-00
"OK, last check before we drop them"
select event_schema, event_name, sql_mode from information_schema.events order by event_schema, event_name;
EVENT_SCHEMA	EVENT_NAME	SQL_MODE
events_test	ee_16407_2	STRICT_TRANS_TABLES,STRICT_ALL_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,TRADITIONAL,NO_ENGINE_SUBSTITUTION
events_test	ee_16407_3	STRICT_TRANS_TABLES,STRICT_ALL_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,TRADITIONAL,NO_ENGINE_SUBSTITUTION
events_test	ee_16407_4	
drop event ee_16407_2;
drop event ee_16407_3;
drop event ee_16407_4;
"And now one last test regarding sql_mode and call of SP from an event"
delete from events_test.events_smode_test;
set sql_mode='ansi';
select get_lock('ee_16407_5', 60);
get_lock('ee_16407_5', 60)
1
set global event_scheduler= on;
set sql_mode='traditional';
create procedure ee_16407_5_pendant() begin insert into events_test.events_smode_test values('ee_16407_5','2001-02-29'); end|
create procedure ee_16407_6_pendant() begin insert into events_test.events_smode_test values('ee_16407_6','2004-02-29'); end|
create event ee_16407_5 on schedule every 60 second do
begin
select get_lock('ee_16407_5', 60) /*ee_16407_5*/;
select release_lock('ee_16407_5');
call events_test.ee_16407_5_pendant();
end|
create event ee_16407_6 on schedule every 60 second do
begin
select get_lock('ee_16407_5', 60) /*ee_16407_6*/;
select release_lock('ee_16407_5');
call events_test.ee_16407_6_pendant();
end|
"Should have 2 locked processes"
select /*4*/ user, host, db, info from information_schema.processlist
where state = 'User lock' and info = 'select get_lock(\'ee_16407_5\', 60)';
user	host	db	info
root	localhost	events_test	select get_lock('ee_16407_5', 60)
root	localhost	events_test	select get_lock('ee_16407_5', 60)
select release_lock('ee_16407_5');
release_lock('ee_16407_5')
1
"Should have 0 processes locked"
select /*5*/ user, host, db, info from information_schema.processlist
where state = 'User lock' and info = 'select get_lock(\'ee_16407_5\', 60)';
user	host	db	info
select * from events_test.events_smode_test order by ev_name, a;
ev_name	a
ee_16407_6	2004-02-29
"And here we check one more time before we drop the events"
select event_schema, event_name, sql_mode from information_schema.events order by event_schema, event_name;
EVENT_SCHEMA	EVENT_NAME	SQL_MODE
events_test	ee_16407_5	STRICT_TRANS_TABLES,STRICT_ALL_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,TRADITIONAL,NO_ENGINE_SUBSTITUTION
events_test	ee_16407_6	STRICT_TRANS_TABLES,STRICT_ALL_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,TRADITIONAL,NO_ENGINE_SUBSTITUTION
drop event ee_16407_5;
drop event ee_16407_6;
drop procedure ee_16407_5_pendant;
drop procedure ee_16407_6_pendant;
set global event_scheduler= off;
drop table events_smode_test;
set sql_mode=@old_sql_mode;
set global event_scheduler=off;
delete from mysql.user where User like 'mysqltest_%';
delete from mysql.db where User like 'mysqltest_%';
flush privileges;
drop database if exists mysqltest_db1;
create user mysqltest_user1@localhost;
create database mysqltest_db1;
grant event on events_test.* to mysqltest_user1@localhost;
create event mysqltest_user1 on schedule every 10 second do select 42;
alter event mysqltest_user1 rename to mysqltest_db1.mysqltest_user1;
ERROR 42000: Access denied for user 'mysqltest_user1'@'localhost' to database 'mysqltest_db1'
"Let's test now rename when there is no select DB"
select database();
database()
NULL
alter event events_test.mysqltest_user1 rename to mysqltest_user1;
ERROR 3D000: No database selected
select event_schema, event_name, definer, event_type, status from information_schema.events;
EVENT_SCHEMA	EVENT_NAME	DEFINER	EVENT_TYPE	STATUS
events_test	mysqltest_user1	mysqltest_user1@localhost	RECURRING	ENABLED
drop event events_test.mysqltest_user1;
drop user mysqltest_user1@localhost;
drop database mysqltest_db1;
create event e_53 on schedule at (select s1 from ttx) do drop table t;
ERROR 42000: This version of MySQL doesn't yet support 'Usage of subqueries or stored function calls as part of this statement'
create event e_53 on schedule every (select s1 from ttx) second do drop table t;
ERROR 42000: This version of MySQL doesn't yet support 'Usage of subqueries or stored function calls as part of this statement'
create event e_53 on schedule every 5 second starts (select s1 from ttx) do drop table t;
ERROR 42000: This version of MySQL doesn't yet support 'Usage of subqueries or stored function calls as part of this statement'
create event e_53 on schedule every 5 second ends (select s1 from ttx) do drop table t;
ERROR 42000: This version of MySQL doesn't yet support 'Usage of subqueries or stored function calls as part of this statement'
drop event if exists e_16;
drop procedure if exists p_16;
create event e_16 on schedule every 1 second do set @a=5;
create procedure p_16 () alter event e_16 on schedule every @a second;
set @a = null;
call p_16();
ERROR HY000: Incorrect INTERVAL value: 'NULL'
call p_16();
ERROR HY000: Incorrect INTERVAL value: 'NULL'
set @a= 6;
call p_16();
drop procedure p_16;
drop event e_16;
drop function if exists f22830;
drop event if exists e22830;
drop event if exists e22830_1;
drop event if exists e22830_2;
drop event if exists e22830_3;
drop event if exists e22830_4;
drop table if exists t1;
drop table if exists t2;
create table t1 (a int);
insert into t1 values (2);
create table t2 (a char(20));
insert into t2 values ("e22830_1");
create function f22830 () returns int return 5;
select get_lock('ee_22830', 60);
get_lock('ee_22830', 60)
1
set global event_scheduler=on;
create procedure p22830_wait()
begin
select get_lock('ee_22830', 60);
select release_lock('ee_22830');
end|
create event e22830 on schedule every f22830() second do
begin
call p22830_wait();
select 123;
end|
ERROR 42000: This version of MySQL doesn't yet support 'Usage of subqueries or stored function calls as part of this statement'
create event e22830_1 on schedule every 1 hour do
begin
call p22830_wait();
alter event e22830_1 on schedule every (select 8 from dual) hour;
end|
create event e22830_2 on schedule every 1 hour do
begin
call p22830_wait();
alter event e22830_2 on schedule every (select 8 from t1) hour;
end|
create event e22830_3 on schedule every 1 hour do
begin
call p22830_wait();
alter event e22830_3 on schedule every f22830() hour;
end|
create event e22830_4 on schedule every 1 hour do
begin
call p22830_wait();
alter event e22830_4 on schedule every (select f22830() from dual) hour;
end|
"All events should be blocked in get_lock()"
select event_name, event_definition, interval_value, interval_field from information_schema.events order by event_name;
EVENT_NAME	EVENT_DEFINITION	INTERVAL_VALUE	INTERVAL_FIELD
e22830_1	begin
call p22830_wait();
alter event e22830_1 on schedule every (select 8 from dual) hour;
end	1	HOUR
e22830_2	begin
call p22830_wait();
alter event e22830_2 on schedule every (select 8 from t1) hour;
end	1	HOUR
e22830_3	begin
call p22830_wait();
alter event e22830_3 on schedule every f22830() hour;
end	1	HOUR
e22830_4	begin
call p22830_wait();
alter event e22830_4 on schedule every (select f22830() from dual) hour;
end	1	HOUR
select release_lock('ee_22830');
release_lock('ee_22830')
1
set global event_scheduler=off;
select event_name, event_definition, interval_value, interval_field from information_schema.events order by event_name;
EVENT_NAME	EVENT_DEFINITION	INTERVAL_VALUE	INTERVAL_FIELD
e22830_1	begin
call p22830_wait();
alter event e22830_1 on schedule every (select 8 from dual) hour;
end	8	HOUR
e22830_2	begin
call p22830_wait();
alter event e22830_2 on schedule every (select 8 from t1) hour;
end	1	HOUR
e22830_3	begin
call p22830_wait();
alter event e22830_3 on schedule every f22830() hour;
end	1	HOUR
e22830_4	begin
call p22830_wait();
alter event e22830_4 on schedule every (select f22830() from dual) hour;
end	1	HOUR
drop procedure p22830_wait;
drop function f22830;
drop event (select a from t2);
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near '(select a from t2)' at line 1
drop event e22830_1;
drop event e22830_2;
drop event e22830_3;
drop event e22830_4;
drop table t1;
drop table t2;
DROP USER mysqltest_u1@localhost;
CREATE USER mysqltest_u1@localhost;
GRANT EVENT ON events_test.* TO mysqltest_u1@localhost;
CREATE EVENT e1 ON SCHEDULE EVERY 1 DAY DO SELECT 1;
SELECT event_name, definer FROM INFORMATION_SCHEMA.EVENTS;
EVENT_NAME	DEFINER
e1	root@localhost
DROP EVENT e1;
CREATE DEFINER=CURRENT_USER EVENT e1 ON SCHEDULE EVERY 1 DAY DO SELECT 1;
SELECT event_name, definer FROM INFORMATION_SCHEMA.EVENTS;
EVENT_NAME	DEFINER
e1	root@localhost
ALTER DEFINER=mysqltest_u1@localhost EVENT e1 ON SCHEDULE EVERY 1 HOUR;
SELECT event_name, definer FROM INFORMATION_SCHEMA.EVENTS;
EVENT_NAME	DEFINER
e1	mysqltest_u1@localhost
DROP EVENT e1;
CREATE DEFINER=CURRENT_USER() EVENT e1 ON SCHEDULE EVERY 1 DAY DO SELECT 1;
SELECT event_name, definer FROM INFORMATION_SCHEMA.EVENTS;
EVENT_NAME	DEFINER
e1	root@localhost
DROP EVENT e1;
CREATE DEFINER=mysqltest_u1@localhost EVENT e1 ON SCHEDULE EVERY 1 DAY DO
SELECT 1;
SELECT event_name, definer FROM INFORMATION_SCHEMA.EVENTS;
EVENT_NAME	DEFINER
e1	mysqltest_u1@localhost
DROP EVENT e1;
CREATE EVENT e1 ON SCHEDULE EVERY 1 DAY DO SELECT 1;
SELECT event_name, definer FROM INFORMATION_SCHEMA.EVENTS;
EVENT_NAME	DEFINER
e1	mysqltest_u1@localhost
DROP EVENT e1;
CREATE DEFINER=CURRENT_USER EVENT e1 ON SCHEDULE EVERY 1 DAY DO SELECT 1;
SELECT event_name, definer FROM INFORMATION_SCHEMA.EVENTS;
EVENT_NAME	DEFINER
e1	mysqltest_u1@localhost
ALTER DEFINER=root@localhost EVENT e1 ON SCHEDULE EVERY 1 HOUR;
ERROR 42000: Access denied; you need (at least one of) the SUPER or SET_USER_ID privilege(s) for this operation
SELECT event_name, definer FROM INFORMATION_SCHEMA.EVENTS;
EVENT_NAME	DEFINER
e1	mysqltest_u1@localhost
DROP EVENT e1;
CREATE DEFINER=CURRENT_USER() EVENT e1 ON SCHEDULE EVERY 1 DAY DO SELECT 1;
SELECT event_name, definer FROM INFORMATION_SCHEMA.EVENTS;
EVENT_NAME	DEFINER
e1	mysqltest_u1@localhost
DROP EVENT e1;
CREATE DEFINER=root@localhost EVENT e1 ON SCHEDULE EVERY 1 DAY DO SELECT 1;
ERROR 42000: Access denied; you need (at least one of) the SUPER or SET_USER_ID privilege(s) for this operation
DROP EVENT e1;
ERROR HY000: Unknown event 'e1'
DROP USER mysqltest_u1@localhost;
SET GLOBAL EVENT_SCHEDULER= OFF;
SET @save_time_zone= @@TIME_ZONE;
SET TIME_ZONE= '+00:00';
SET TIMESTAMP= UNIX_TIMESTAMP('2005-12-31 23:58:59');
CREATE EVENT e1 ON SCHEDULE EVERY 1 DAY DO SELECT 1;
SHOW EVENTS;
Db	Name	Definer	Time zone	Type	Execute at	Interval value	Interval field	Starts	Ends	Status	Originator	character_set_client	collation_connection	Database Collation
events_test	e1	root@localhost	+00:00	RECURRING	NULL	1	DAY	2005-12-31 23:58:59	NULL	ENABLED	1	latin1	latin1_swedish_ci	utf8mb4_0900_ai_ci
SET TIME_ZONE= '-01:00';
ALTER EVENT e1 ON SCHEDULE EVERY 1 DAY STARTS '2000-01-01 00:00:00';
SHOW EVENTS;
Db	Name	Definer	Time zone	Type	Execute at	Interval value	Interval field	Starts	Ends	Status	Originator	character_set_client	collation_connection	Database Collation
events_test	e1	root@localhost	-01:00	RECURRING	NULL	1	DAY	2000-01-01 00:00:00	NULL	ENABLED	1	latin1	latin1_swedish_ci	utf8mb4_0900_ai_ci
SET TIME_ZONE= '+02:00';
ALTER EVENT e1 ON SCHEDULE AT '2000-01-02 00:00:00'
  ON COMPLETION PRESERVE DISABLE;
SHOW EVENTS;
Db	Name	Definer	Time zone	Type	Execute at	Interval value	Interval field	Starts	Ends	Status	Originator	character_set_client	collation_connection	Database Collation
events_test	e1	root@localhost	+02:00	ONE TIME	2000-01-02 00:00:00	NULL	NULL	NULL	NULL	DISABLED	1	latin1	latin1_swedish_ci	utf8mb4_0900_ai_ci
SET TIME_ZONE= '-03:00';
ALTER EVENT e1 ON SCHEDULE EVERY 1 DAY ENDS '2038-01-03 00:00:00'
  ON COMPLETION PRESERVE DISABLE;
SHOW EVENTS;
Db	Name	Definer	Time zone	Type	Execute at	Interval value	Interval field	Starts	Ends	Status	Originator	character_set_client	collation_connection	Database Collation
events_test	e1	root@localhost	-03:00	RECURRING	NULL	1	DAY	2005-12-31 20:58:59	2038-01-03 00:00:00	DISABLED	1	latin1	latin1_swedish_ci	utf8mb4_0900_ai_ci
SET TIME_ZONE= '+04:00';
ALTER EVENT e1 DO SELECT 2;
SHOW EVENTS;
Db	Name	Definer	Time zone	Type	Execute at	Interval value	Interval field	Starts	Ends	Status	Originator	character_set_client	collation_connection	Database Collation
events_test	e1	root@localhost	-03:00	RECURRING	NULL	1	DAY	2005-12-31 20:58:59	2038-01-03 00:00:00	DISABLED	1	latin1	latin1_swedish_ci	utf8mb4_0900_ai_ci
DROP EVENT e1;
SET TIME_ZONE='+05:00';
CREATE EVENT e1 ON SCHEDULE EVERY 1 DAY STARTS '2006-01-01 00:00:00' DO
SELECT 1;
SET TIMESTAMP= @@TIMESTAMP + 1;
SET TIME_ZONE='-05:00';
CREATE EVENT e2 ON SCHEDULE EVERY 1 DAY STARTS '2006-01-01 00:00:00' DO
SELECT 1;
SET TIMESTAMP= @@TIMESTAMP + 1;
SET TIME_ZONE='+00:00';
CREATE EVENT e3 ON SCHEDULE EVERY 1 DAY STARTS '2006-01-01 00:00:00' DO
SELECT 1;
SELECT * FROM INFORMATION_SCHEMA.EVENTS ORDER BY event_name;
EVENT_CATALOG	EVENT_SCHEMA	EVENT_NAME	DEFINER	TIME_ZONE	EVENT_BODY	EVENT_DEFINITION	EVENT_TYPE	EXECUTE_AT	INTERVAL_VALUE	INTERVAL_FIELD	SQL_MODE	STARTS	ENDS	STATUS	ON_COMPLETION	CREATED	LAST_ALTERED	LAST_EXECUTED	EVENT_COMMENT	ORIGINATOR	CHARACTER_SET_CLIENT	COLLATION_CONNECTION	DATABASE_COLLATION
def	events_test	e1	root@localhost	+05:00	SQL	SELECT 1	RECURRING	NULL	1	DAY	ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION	2006-01-01 00:00:00	NULL	ENABLED	NOT PRESERVE	2005-12-31 23:58:59	2005-12-31 23:58:59	NULL		1	latin1	latin1_swedish_ci	utf8mb4_0900_ai_ci
def	events_test	e2	root@localhost	-05:00	SQL	SELECT 1	RECURRING	NULL	1	DAY	ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION	2006-01-01 00:00:00	NULL	ENABLED	NOT PRESERVE	2005-12-31 23:59:00	2005-12-31 23:59:00	NULL		1	latin1	latin1_swedish_ci	utf8mb4_0900_ai_ci
def	events_test	e3	root@localhost	+00:00	SQL	SELECT 1	RECURRING	NULL	1	DAY	ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION	2006-01-01 00:00:00	NULL	ENABLED	NOT PRESERVE	2005-12-31 23:59:01	2005-12-31 23:59:01	NULL		1	latin1	latin1_swedish_ci	utf8mb4_0900_ai_ci
SHOW EVENTS;
Db	Name	Definer	Time zone	Type	Execute at	Interval value	Interval field	Starts	Ends	Status	Originator	character_set_client	collation_connection	Database Collation
events_test	e1	root@localhost	+05:00	RECURRING	NULL	1	DAY	2006-01-01 00:00:00	NULL	ENABLED	1	latin1	latin1_swedish_ci	utf8mb4_0900_ai_ci
events_test	e2	root@localhost	-05:00	RECURRING	NULL	1	DAY	2006-01-01 00:00:00	NULL	ENABLED	1	latin1	latin1_swedish_ci	utf8mb4_0900_ai_ci
events_test	e3	root@localhost	+00:00	RECURRING	NULL	1	DAY	2006-01-01 00:00:00	NULL	ENABLED	1	latin1	latin1_swedish_ci	utf8mb4_0900_ai_ci
SHOW CREATE EVENT e1;
Event	sql_mode	time_zone	Create Event	character_set_client	collation_connection	Database Collation
e1	ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION	+05:00	CREATE DEFINER=`root`@`localhost` EVENT `e1` ON SCHEDULE EVERY 1 DAY STARTS '2006-01-01 00:00:00' ON COMPLETION NOT PRESERVE ENABLE DO SELECT 1	latin1	latin1_swedish_ci	utf8mb4_0900_ai_ci
SHOW CREATE EVENT e2;
Event	sql_mode	time_zone	Create Event	character_set_client	collation_connection	Database Collation
e2	ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION	-05:00	CREATE DEFINER=`root`@`localhost` EVENT `e2` ON SCHEDULE EVERY 1 DAY STARTS '2006-01-01 00:00:00' ON COMPLETION NOT PRESERVE ENABLE DO SELECT 1	latin1	latin1_swedish_ci	utf8mb4_0900_ai_ci
SHOW CREATE EVENT e3;
Event	sql_mode	time_zone	Create Event	character_set_client	collation_connection	Database Collation
e3	ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION	+00:00	CREATE DEFINER=`root`@`localhost` EVENT `e3` ON SCHEDULE EVERY 1 DAY STARTS '2006-01-01 00:00:00' ON COMPLETION NOT PRESERVE ENABLE DO SELECT 1	latin1	latin1_swedish_ci	utf8mb4_0900_ai_ci
The following should fail, and nothing should be altered.
ALTER EVENT e1 ON SCHEDULE EVERY 1 HOUR STARTS '1999-01-01 00:00:00'
  ENDS '1999-01-02 00:00:00';
ERROR HY000: Event execution time is in the past and ON COMPLETION NOT PRESERVE is set. The event was not changed. Specify a time in the future.
ALTER EVENT e1 ON SCHEDULE EVERY 1 HOUR STARTS '1999-01-01 00:00:00'
  ENDS '1999-01-02 00:00:00' DISABLE;
ERROR HY000: Event execution time is in the past and ON COMPLETION NOT PRESERVE is set. The event was not changed. Specify a time in the future.
The following should give warnings, and nothing should be created.
CREATE EVENT e4 ON SCHEDULE EVERY 1 HOUR STARTS '1999-01-01 00:00:00'
  ENDS '1999-01-02 00:00:00'
DO
SELECT 1;
Warnings:
Note	1588	Event execution time is in the past and ON COMPLETION NOT PRESERVE is set. The event was dropped immediately after creation.
CREATE EVENT e4 ON SCHEDULE EVERY 1 HOUR STARTS '1999-01-01 00:00:00'
  ENDS '1999-01-02 00:00:00' DISABLE
DO
SELECT 1;
Warnings:
Note	1588	Event execution time is in the past and ON COMPLETION NOT PRESERVE is set. The event was dropped immediately after creation.
CREATE EVENT e4 ON SCHEDULE AT '1999-01-01 00:00:00' DO
SELECT 1;
Warnings:
Note	1588	Event execution time is in the past and ON COMPLETION NOT PRESERVE is set. The event was dropped immediately after creation.
CREATE EVENT e4 ON SCHEDULE AT '1999-01-01 00:00:00' DISABLE
DO
SELECT 1;
Warnings:
Note	1588	Event execution time is in the past and ON COMPLETION NOT PRESERVE is set. The event was dropped immediately after creation.
SHOW EVENTS;
Db	Name	Definer	Time zone	Type	Execute at	Interval value	Interval field	Starts	Ends	Status	Originator	character_set_client	collation_connection	Database Collation
events_test	e1	root@localhost	+05:00	RECURRING	NULL	1	DAY	2006-01-01 00:00:00	NULL	ENABLED	1	latin1	latin1_swedish_ci	utf8mb4_0900_ai_ci
events_test	e2	root@localhost	-05:00	RECURRING	NULL	1	DAY	2006-01-01 00:00:00	NULL	ENABLED	1	latin1	latin1_swedish_ci	utf8mb4_0900_ai_ci
events_test	e3	root@localhost	+00:00	RECURRING	NULL	1	DAY	2006-01-01 00:00:00	NULL	ENABLED	1	latin1	latin1_swedish_ci	utf8mb4_0900_ai_ci
The following should succeed giving a warning.
ALTER EVENT e1 ON SCHEDULE EVERY 1 HOUR STARTS '1999-01-01 00:00:00'
  ENDS '1999-01-02 00:00:00' ON COMPLETION PRESERVE;
Warnings:
Note	1544	Event execution time is in the past. Event has been disabled
CREATE EVENT e4 ON SCHEDULE EVERY 1 HOUR STARTS '1999-01-01 00:00:00'
  ENDS '1999-01-02 00:00:00' ON COMPLETION PRESERVE
DO
SELECT 1;
Warnings:
Note	1544	Event execution time is in the past. Event has been disabled
CREATE EVENT e5 ON SCHEDULE AT '1999-01-01 00:00:00'
  ON COMPLETION PRESERVE
DO
SELECT 1;
Warnings:
Note	1544	Event execution time is in the past. Event has been disabled
The following should succeed without warnings.
ALTER EVENT e2 ON SCHEDULE EVERY 1 HOUR STARTS '1999-01-01 00:00:00';
ALTER EVENT e3 ON SCHEDULE EVERY 1 HOUR STARTS '1999-01-01 00:00:00'
  ENDS '1999-01-02 00:00:00' ON COMPLETION PRESERVE DISABLE;
CREATE EVENT e6 ON SCHEDULE EVERY 1 HOUR STARTS '1999-01-01 00:00:00' DO
SELECT 1;
CREATE EVENT e7 ON SCHEDULE EVERY 1 HOUR STARTS '1999-01-01 00:00:00'
  ENDS '1999-01-02 00:00:00' ON COMPLETION PRESERVE DISABLE
DO
SELECT 1;
CREATE EVENT e8 ON SCHEDULE AT '1999-01-01 00:00:00'
  ON COMPLETION PRESERVE DISABLE
DO
SELECT 1;
SHOW EVENTS;
Db	Name	Definer	Time zone	Type	Execute at	Interval value	Interval field	Starts	Ends	Status	Originator	character_set_client	collation_connection	Database Collation
events_test	e1	root@localhost	+00:00	RECURRING	NULL	1	HOUR	1999-01-01 00:00:00	1999-01-02 00:00:00	DISABLED	1	latin1	latin1_swedish_ci	utf8mb4_0900_ai_ci
events_test	e2	root@localhost	+00:00	RECURRING	NULL	1	HOUR	1999-01-01 00:00:00	NULL	ENABLED	1	latin1	latin1_swedish_ci	utf8mb4_0900_ai_ci
events_test	e3	root@localhost	+00:00	RECURRING	NULL	1	HOUR	1999-01-01 00:00:00	1999-01-02 00:00:00	DISABLED	1	latin1	latin1_swedish_ci	utf8mb4_0900_ai_ci
events_test	e4	root@localhost	+00:00	RECURRING	NULL	1	HOUR	1999-01-01 00:00:00	1999-01-02 00:00:00	DISABLED	1	latin1	latin1_swedish_ci	utf8mb4_0900_ai_ci
events_test	e5	root@localhost	+00:00	ONE TIME	1999-01-01 00:00:00	NULL	NULL	NULL	NULL	DISABLED	1	latin1	latin1_swedish_ci	utf8mb4_0900_ai_ci
events_test	e6	root@localhost	+00:00	RECURRING	NULL	1	HOUR	1999-01-01 00:00:00	NULL	ENABLED	1	latin1	latin1_swedish_ci	utf8mb4_0900_ai_ci
events_test	e7	root@localhost	+00:00	RECURRING	NULL	1	HOUR	1999-01-01 00:00:00	1999-01-02 00:00:00	DISABLED	1	latin1	latin1_swedish_ci	utf8mb4_0900_ai_ci
events_test	e8	root@localhost	+00:00	ONE TIME	1999-01-01 00:00:00	NULL	NULL	NULL	NULL	DISABLED	1	latin1	latin1_swedish_ci	utf8mb4_0900_ai_ci
DROP EVENT e8;
DROP EVENT e7;
DROP EVENT e6;
DROP EVENT e5;
DROP EVENT e4;
DROP EVENT e3;
DROP EVENT e2;
DROP EVENT e1;
SET TIME_ZONE=@save_time_zone;
SET TIMESTAMP=DEFAULT;
drop event if exists new_event;
CREATE EVENT new_event ON SCHEDULE EVERY 0 SECOND DO SELECT 1;
ERROR HY000: INTERVAL is either not positive or too big
CREATE EVENT new_event ON SCHEDULE EVERY (SELECT 0) SECOND DO SELECT 1;
ERROR HY000: INTERVAL is either not positive or too big
CREATE EVENT new_event ON SCHEDULE EVERY "abcdef" SECOND DO SELECT 1;
ERROR HY000: INTERVAL is either not positive or too big
CREATE EVENT new_event ON SCHEDULE EVERY "0abcdef" SECOND DO SELECT 1;
ERROR HY000: INTERVAL is either not positive or too big
CREATE EVENT new_event ON SCHEDULE EVERY "a1bcdef" SECOND DO SELECT 1;
ERROR HY000: INTERVAL is either not positive or too big
CREATE EVENT new_event ON SCHEDULE EVERY (SELECT "abcdef" UNION SELECT "abcdef") SECOND DO SELECT 1;
ERROR HY000: INTERVAL is either not positive or too big
CREATE EVENT new_event ON SCHEDULE EVERY (SELECT "0abcdef") SECOND DO SELECT 1;
ERROR HY000: INTERVAL is either not positive or too big
CREATE EVENT new_event ON SCHEDULE EVERY (SELECT "a1bcdef") SECOND DO SELECT 1;
ERROR HY000: INTERVAL is either not positive or too big
CREATE EVENT new_event ON SCHEDULE AT "every day" DO SELECT 1;
ERROR HY000: Incorrect AT value: 'every day'
CREATE EVENT new_event ON SCHEDULE AT "0every day" DO SELECT 1;
ERROR HY000: Incorrect AT value: '0every day'
CREATE EVENT new_event ON SCHEDULE AT (SELECT "every day") DO SELECT 1;
ERROR HY000: Incorrect AT value: 'every day'
CREATE EVENT new_event ON SCHEDULE AT NOW() STARTS NOW() DO SELECT 1;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'STARTS NOW() DO SELECT 1' at line 1
CREATE EVENT new_event ON SCHEDULE AT NOW() ENDS NOW() DO SELECT 1;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'ENDS NOW() DO SELECT 1' at line 1
CREATE EVENT new_event ON SCHEDULE AT NOW() STARTS NOW() ENDS NOW() DO SELECT 1;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'STARTS NOW() ENDS NOW() DO SELECT 1' at line 1
USE test;
SHOW GRANTS FOR CURRENT_USER;
Grants for root@localhost
GRANT SELECT, INSERT, UPDATE, DELETE, CREATE, DROP, RELOAD, SHUTDOWN, PROCESS, FILE, REFERENCES, INDEX, ALTER, SHOW DATABASES, SUPER, CREATE TEMPORARY TABLES, LOCK TABLES, EXECUTE, REPLICATION SLAVE, REPLICATION CLIENT, CREATE VIEW, SHOW VIEW, CREATE ROUTINE, ALTER ROUTINE, CREATE USER, EVENT, TRIGGER, CREATE TABLESPACE, CREATE ROLE, DROP ROLE ON *.* TO `root`@`localhost` WITH GRANT OPTION
GRANT APPLICATION_PASSWORD_ADMIN,AUDIT_ABORT_EXEMPT,AUDIT_ADMIN,AUTHENTICATION_POLICY_ADMIN,BACKUP_ADMIN,BINLOG_ADMIN,BINLOG_ENCRYPTION_ADMIN,CLONE_ADMIN,CONNECTION_ADMIN,ENCRYPTION_KEY_ADMIN,FIREWALL_EXEMPT,FLUSH_OPTIMIZER_COSTS,FLUSH_STATUS,FLUSH_TABLES,FLUSH_USER_RESOURCES,GROUP_REPLICATION_ADMIN,GROUP_REPLICATION_STREAM,INNODB_REDO_LOG_ARCHIVE,INNODB_REDO_LOG_ENABLE,PASSWORDLESS_USER_ADMIN,PERSIST_RO_VARIABLES_ADMIN,REPLICATION_APPLIER,REPLICATION_SLAVE_ADMIN,RESOURCE_GROUP_ADMIN,RESOURCE_GROUP_USER,ROLE_ADMIN,SENSITIVE_VARIABLES_OBSERVER,SERVICE_CONNECTION_ADMIN,SESSION_VARIABLES_ADMIN,SET_USER_ID,SHOW_ROUTINE,SYSTEM_USER,SYSTEM_VARIABLES_ADMIN,TABLE_ENCRYPTION_ADMIN,XA_RECOVER_ADMIN ON *.* TO `root`@`localhost` WITH GRANT OPTION
GRANT PROXY ON ``@`` TO `root`@`localhost` WITH GRANT OPTION
SET GLOBAL event_scheduler = ON;
CREATE TABLE events_test.event_log
(id int KEY AUTO_INCREMENT, ev_nm char(40), ev_cnt int, ev_tm timestamp);
SET @save_session_autocommit = @@session.autocommit;
SET @@session.autocommit=0;
CREATE USER evtest1@localhost;
ALTER USER evtest1@localhost IDENTIFIED BY 'ev1';
REVOKE ALL PRIVILEGES, GRANT OPTION FROM evtest1@localhost;
GRANT create, insert, select, event ON events_test.* TO evtest1@localhost;
GRANT select,insert ON test.* TO evtest1@localhost;
SHOW GRANTS FOR evtest1@localhost;
Grants for evtest1@localhost
GRANT USAGE ON *.* TO `evtest1`@`localhost`
GRANT SELECT, INSERT ON `test`.* TO `evtest1`@`localhost`
GRANT SELECT, INSERT, CREATE, EVENT ON `events_test`.* TO `evtest1`@`localhost`
connection e1;
CREATE EVENT ev_sched_1823 ON SCHEDULE EVERY 2 SECOND
DO BEGIN
SET AUTOCOMMIT = 0;
SET @evname = 'ev_sched_1823';
SET @cnt = 0;
SELECT COUNT(*) INTO @cnt FROM events_test.event_log WHERE ev_nm = @evname;
IF @cnt < 6 THEN
INSERT INTO events_test.event_log VALUES (NULL,@evname,@cnt+1,current_timestamp());
COMMIT;
END IF;
SELECT COUNT(*) INTO @cnt FROM events_test.event_log WHERE ev_nm = @evname;
IF @cnt < 6 THEN
INSERT INTO events_test.event_log VALUES (NULL,@evname,@cnt+1,current_timestamp());
ROLLBACK;
END IF;
END;|
Sleep till the first INSERT into events_test.event_log occurred
SELECT COUNT(*) > 0 AS "Expect 1" FROM events_test.event_log;
Expect 1
1
connection default;
Sleep 4 seconds
SELECT COUNT(*) INTO @row_cnt FROM events_test.event_log;
Sleep 4 seconds
SELECT COUNT(*) > @row_cnt AS "Expect 0" FROM events_test.event_log;
Expect 0
0
DROP EVENT events_test.ev_sched_1823;
DROP USER evtest1@localhost;
DROP TABLE events_test.event_log;
SET GLOBAL event_scheduler = OFF;
SET @@session.autocommit = @save_session_autocommit;
SET GLOBAL event_scheduler= ON;
CREATE EVENT bug28641 ON SCHEDULE AT '2038.01.18 03:00:00'
DO BEGIN
SELECT 1;
END;|
Warnings:
Warning	4095	Delimiter '.' in position 4 in datetime value '2038.01.18 03:00:00' at row 1 is deprecated. Prefer the standard '-'.
SET GLOBAL event_scheduler= OFF;
DROP EVENT bug28641;

#####################################################################
#
# BUG#31111: --read-only crashes MySQL (events fail to load).
#
#####################################################################

DROP USER mysqltest_u1@localhost;
DROP EVENT IF EXISTS e1;
DROP EVENT IF EXISTS e2;

CREATE USER mysqltest_u1@localhost;
GRANT EVENT ON *.* TO mysqltest_u1@localhost;

SET GLOBAL READ_ONLY = 1;

#
# Connection: u1_con (mysqltest_u1@localhost/events_test).
#

CREATE EVENT e1 ON SCHEDULE AT '2038-01-01 00:00:00' DO SET @a = 1;
ERROR HY000: The MySQL server is running with the --read-only option so it cannot execute this statement

ALTER EVENT e1 COMMENT 'comment';
ERROR HY000: The MySQL server is running with the --read-only option so it cannot execute this statement

DROP EVENT e1;
ERROR HY000: The MySQL server is running with the --read-only option so it cannot execute this statement

#
# Connection: root_con (root@localhost/events_test).
#

CREATE EVENT e1 ON SCHEDULE AT '2038-01-01 00:00:00' DO SET @a = 1;

ALTER EVENT e1 COMMENT 'comment';

DROP EVENT e1;

SET GLOBAL READ_ONLY = 0;

#
# Connection: u1_con (mysqltest_u1@localhost/test).
#

CREATE EVENT e1 ON SCHEDULE AT CURRENT_TIMESTAMP + INTERVAL 1 SECOND DO SET @a = 1;
CREATE EVENT e2 ON SCHEDULE EVERY 1 SECOND DO SET @a = 1;

SELECT
event_name,
last_executed IS NULL,
definer
FROM INFORMATION_SCHEMA.EVENTS
WHERE event_schema = 'events_test';
EVENT_NAME	last_executed IS NULL	DEFINER
e1	1	mysqltest_u1@localhost
e2	1	mysqltest_u1@localhost

#
# Connection: root_con (root@localhost/events_test).
#

SET GLOBAL READ_ONLY = 1;

SET GLOBAL EVENT_SCHEDULER = ON;

# Waiting for the event scheduler to execute and drop event e1...

# Waiting for the event scheduler to execute and update event e2...

SET GLOBAL EVENT_SCHEDULER = OFF;

SELECT
event_name,
last_executed IS NULL,
definer
FROM INFORMATION_SCHEMA.EVENTS
WHERE event_schema = 'events_test';
EVENT_NAME	last_executed IS NULL	DEFINER
e2	0	mysqltest_u1@localhost

DROP EVENT e1;
ERROR HY000: Unknown event 'e1'

# Cleanup.

DROP EVENT e2;

SET GLOBAL READ_ONLY = 0;

#
# Connection: default
#

DROP USER mysqltest_u1@localhost;

#####################################################################
#
# End of BUG#31111.
#
#####################################################################

drop procedure if exists p;
set @old_mode= @@sql_mode;
set @@sql_mode= cast(pow(2,33)-1 as unsigned integer) & ~0x1003ff00;
Warnings:
Warning	3090	Changing sql mode 'PAD_CHAR_TO_FULL_LENGTH' is deprecated. It will be removed in a future release.
create event e1 on schedule every 1 day do select 1;
select @@sql_mode into @full_mode;
set @@sql_mode= @old_mode;
select event_name from information_schema.events where event_name = 'e1' and sql_mode = @full_mode;
EVENT_NAME
e1
drop event e1;
SET @old_server_id = @@GLOBAL.server_id;
SET GLOBAL server_id = (1 << 32) - 1;
SELECT @@GLOBAL.server_id;
@@GLOBAL.server_id
4294967295
CREATE EVENT ev1 ON SCHEDULE EVERY 1 DAY DO SELECT 1;
SELECT event_name, originator FROM INFORMATION_SCHEMA.EVENTS;
EVENT_NAME	ORIGINATOR
ev1	4294967295
DROP EVENT ev1;
SET GLOBAL server_id = @old_server_id;
CREATE DATABASE event_test12;
USE event_test12;
CREATE EVENT ev1 ON SCHEDULE EVERY 1 DAY DO SELECT 1;
CREATE DATABASE event_test1;
USE event_test1;
SHOW EVENTS;
Db	Name	Definer	Time zone	Type	Execute at	Interval value	Interval field	Starts	Ends	Status	Originator	character_set_client	collation_connection	Database Collation
DROP DATABASE event_test1;
DROP DATABASE event_test12;
#
# Bug#12546938 (formerly known as bug#61005):
# CREATE IF NOT EXIST EVENT WILL CREATE MULTIPLE RUNNING EVENTS
#
USE events_test;
SET GLOBAL event_scheduler = ON;
DROP TABLE IF EXISTS table_bug12546938;
DROP EVENT IF EXISTS event_Bug12546938;
CREATE TABLE table_bug12546938 (i INT);
# Create an event which will be executed with a small delay
# and won't be automatically dropped after that.
CREATE EVENT event_Bug12546938
ON SCHEDULE AT CURRENT_TIMESTAMP + INTERVAL 1 SECOND ON COMPLETION PRESERVE
ENABLE DO
BEGIN 
INSERT INTO table_bug12546938 VALUES(1);
END
|
# Now try to create the same event using CREATE EVENT IF NOT EXISTS.
# A warning should be emitted. A new event should not be created nor
# the old event should be re-executed.
CREATE EVENT IF NOT EXISTS event_bug12546938
ON SCHEDULE AT CURRENT_TIMESTAMP + INTERVAL 1 SECOND ON COMPLETION PRESERVE
ENABLE DO
BEGIN
INSERT INTO table_bug12546938 VALUES (1);
END
|
Warnings:
Note	1537	Event 'event_bug12546938' already exists
# Wait until at least one instance of event is executed.
# Check that only one instance of our event was executed.
SELECT COUNT(*) FROM table_bug12546938;
COUNT(*)
1
# Clean-up.
DROP EVENT IF EXISTS event_Bug12546938;
DROP TABLE table_bug12546938;
SET GLOBAL EVENT_SCHEDULER = OFF;
DROP DATABASE IF EXISTS event_test11764334;
CREATE DATABASE event_test11764334;
USE event_test11764334;
CREATE EVENT ev1 ON SCHEDULE EVERY 3 SECOND DISABLE DO SELECT 1;
SHOW EVENTS IN event_test11764334 WHERE NAME='ev1';
Db	Name	Definer	Time zone	Type	Execute at	Interval value	Interval field	Starts	Ends	Status	Originator	character_set_client	collation_connection	Database Collation
event_test11764334	ev1	root@localhost	SYSTEM	RECURRING	NULL	3	SECOND	#	#	DISABLED	1	latin1	latin1_swedish_ci	utf8mb4_0900_ai_ci
ALTER EVENT ev1 ON SCHEDULE EVERY 4 SECOND;
SHOW EVENTS IN event_test11764334 WHERE NAME='ev1';
Db	Name	Definer	Time zone	Type	Execute at	Interval value	Interval field	Starts	Ends	Status	Originator	character_set_client	collation_connection	Database Collation
event_test11764334	ev1	root@localhost	SYSTEM	RECURRING	NULL	4	SECOND	#	#	DISABLED	1	latin1	latin1_swedish_ci	utf8mb4_0900_ai_ci
DROP EVENT ev1;
DROP DATABASE event_test11764334;
USE test;
#
# Bug#17435114 ASSERTION FAILED: THD->LEX->LOCK_TABLES_STATE == QUERY_TABLES_LIST::LTS_NOT_LOCK
#
ALTER EVENT e1 ON SCHEDULE EVERY
EXISTS (SELECT 1 AS x ORDER BY x LIMIT 0) NOT IN (1) MINUTE_SECOND;
ERROR HY000: Unknown event 'e1'
ALTER EVENT e1 ON SCHEDULE EVERY
EXISTS (SELECT 1 AS x ORDER BY x LIMIT 0) NOT IN (2) MINUTE_SECOND;
ERROR HY000: Unknown event 'e1'
CREATE EVENT e1 ON SCHEDULE EVERY 2 HOUR DO SELECT 1;
ALTER EVENT e1 ON SCHEDULE EVERY
EXISTS (SELECT 1 AS x ORDER BY x LIMIT 0) NOT IN (2) MINUTE_SECOND;
DROP EVENT e1;
#
# Bug#27041552 - M_THD->GET_TRANSACTION()->IS_EMPTY(TRANSACTION_CTX::STMT) ETC. ASSERTION
#
SET GLOBAL event_scheduler= 'ON';
CREATE TABLE t1(_userid CHAR (1)KEY) ENGINE=InnoDB;
SET @save_autocommit_value= @@global.autocommit;
SET @@global.autocommit= OFF;
SET @@session.TIMESTAMP=200;
CREATE EVENT e1 ON SCHEDULE AT CURRENT_TIMESTAMP + INTERVAL 1 DAY DO INSERT INTO t1 VALUES(1);
SELECT SLEEP(3);
SLEEP(3)
0
SET @@global.autocommit=@save_autocommit_value;
SET @@session.TIMESTAMP=default;
DROP TABLE t1;
#
# Bug#28122841 - CREATE EVENT/PROCEDURE/FUNCTION CRASHES WITH ACCENT INSENSITIVE NAMES.
#
SET NAMES utf8;
Warnings:
Warning	3719	'utf8' is currently an alias for the character set UTF8MB3, but will be an alias for UTF8MB4 in a future release. Please consider using UTF8MB4 in order to be unambiguous.
#
# Test case to verify events with case and accent insensitive names.
#
CREATE EVENT cafe ON SCHEDULE EVERY 2 YEAR DO SELECT 1;
# Event names are case and accent insensitive. So from the data-dictionary
# "cafe" is obtained for the following statement. Since MDL key comparison
# is case and accent sensitive, assert condition to verify expected lock
# with name "test.cafe" fails (lock is obtained on the test.café)
CREATE EVENT café ON SCHEDULE EVERY 2 YEAR DO SELECT 1;
ERROR HY000: Event 'café' already exists
# Following statement is to verify operation with the upper case name.
CREATE EVENT CAFE ON SCHEDULE EVERY 2 YEAR DO SELECT 1;
ERROR HY000: Event 'CAFE' already exists
SHOW CREATE EVENT CaFé;
Event	sql_mode	time_zone	Create Event	character_set_client	collation_connection	Database Collation
cafe	#	#	#	#	#	#
DROP EVENT CaFé;
# Event with NAME_LEN size name.
CREATE EVENT очень_очень_очень_очень_очень_очень_очень_очень_длинная_строка_e ON SCHEDULE EVERY 2 YEAR DO SELECT 1;
CREATE EVENT очень_очень_очень_очень_очень_очень_очень_очень_длинная_строка_é ON SCHEDULE EVERY 2 YEAR DO SELECT 1;
ERROR HY000: Event 'очень_очень_очень_очень_очень_очень_очень_очень_длинная_строка_é' already exists
CREATE EVENT очень_очень_очень_очень_очень_очень_очень_очень_длинная_строка_E ON SCHEDULE EVERY 2 YEAR DO SELECT 1;
ERROR HY000: Event 'очень_очень_очень_очень_очень_очень_очень_очень_длинная_строка_E' already exists
SHOW CREATE EVENT очень_очень_очень_очень_очень_очень_очень_очень_длинная_строка_é;
Event	sql_mode	time_zone	Create Event	character_set_client	collation_connection	Database Collation
очень_очень_очень_очень_очень_очень_очень_очень_длинная_строка_e	#	#	#	#	#	#
DROP EVENT очень_очень_очень_очень_очень_очень_очень_очень_длинная_строка_é;
SET NAMES default;
#
# Bug#29871530 - MYSQL 8.0 INFORMATION_SCHEMA.EVENTS NOT
# OBSERVING CUSTOM TIMEZONE FOR LAST_EXECU
#
SET TIMESTAMP= UNIX_TIMESTAMP('2019-08-01 01:20:30');
SET GLOBAL event_scheduler = ON;
SET sql_mode='STRICT_TRANS_TABLES';
Warnings:
Warning	3135	'NO_ZERO_DATE', 'NO_ZERO_IN_DATE' and 'ERROR_FOR_DIVISION_BY_ZERO' sql modes should be used with strict mode. They will be merged with strict mode in a future release.
SET TIME_ZONE= '+03:00';
CREATE EVENT my_event
ON SCHEDULE EVERY 2 SECOND
STARTS '2019-08-01 01:20:30'
      ENDS   '2034-08-30 01:20:30'
    DO DO 1+1;
# Verify value of 'LAST EXECUTED'.
# We do not check last executed against timezone change, as it
# will always be in the timezone at create event, as per:
# WL#3698: Events: execution in local time zone.
2019-08-01 01:20:30
SELECT TIMESTAMP('2019-08-01 01:20:30') < LAST_EXECUTED
FROM INFORMATION_SCHEMA.EVENTS WHERE EVENT_NAME='my_event';
TIMESTAMP('2019-08-01 01:20:30') < LAST_EXECUTED
1
# Verify value of 'LAST ALTERED'.
SET TIMESTAMP= UNIX_TIMESTAMP('2019-08-01 01:20:50');
ALTER EVENT my_event COMMENT 'event comment';
SELECT TIMESTAMP('2019-08-01 01:20:50') = TIMESTAMP('2019-08-01 01:20:50');
TIMESTAMP('2019-08-01 01:20:50') = TIMESTAMP('2019-08-01 01:20:50')
1
# Check that both last altered and created is in the same time zone.
SELECT (CREATED - TIMESTAMP('2019-08-01 01:20:30')),
(LAST_ALTERED - TIMESTAMP('2019-08-01 01:20:50'))
FROM INFORMATION_SCHEMA.EVENTS WHERE EVENT_NAME='my_event';
(CREATED - TIMESTAMP('2019-08-01 01:20:30'))	(LAST_ALTERED - TIMESTAMP('2019-08-01 01:20:50'))
0	0
SET TIME_ZONE= '-02:00';
SELECT (CREATED - CONVERT_TZ('2019-08-01 01:20:30', '+03:00', '-02:00')),
(LAST_ALTERED - CONVERT_TZ('2019-08-01 01:20:50', '+03:00', '-02:00'))
FROM INFORMATION_SCHEMA.EVENTS WHERE EVENT_NAME='my_event';
(CREATED - CONVERT_TZ('2019-08-01 01:20:30', '+03:00', '-02:00'))	(LAST_ALTERED - CONVERT_TZ('2019-08-01 01:20:50', '+03:00', '-02:00'))
0	0
DROP EVENT my_event;
SET TIME_ZONE=default;
SET TIMESTAMP=default;
DROP DATABASE events_test;
SET GLOBAL event_scheduler= 'ON';
SET @@global.concurrent_insert= @concurrent_insert;
