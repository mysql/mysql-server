drop database if exists mysqltest;
drop table if exists t1, t2, t3, t4;
drop procedure if exists sp1;
drop procedure if exists sp2;
drop procedure if exists sp3;
drop procedure if exists sp4;
drop function if exists f1;
drop function if exists f2;
drop function if exists f3;
create database mysqltest;
use mysqltest//
create procedure sp1 () 
begin
drop table if exists t1;
select 1 as "my-col";
end;
//
select database();
database()
mysqltest
call sp1();
my-col
1
select database();
database()
mysqltest
use test;
select database();
database()
test
call mysqltest.sp1();
my-col
1
select database();
database()
test
drop procedure mysqltest.sp1;
drop database mysqltest;
create procedure sp1() 
begin 
create table t1 (a int); 
insert into t1 values (10); 
end//
create procedure sp2()
begin
create table t2(a int);
insert into t2 values(1);
call sp1();
end//
create function f1() returns int
begin 
return (select max(a) from t1);
end//
create procedure sp3()
begin 
call sp1();
select 'func', f1();
end//
call sp1();
select 't1',a from t1;
t1	a
t1	10
drop table t1;
call sp2();
select 't1',a from t1;
t1	a
t1	10
select 't2',a from t2;
t2	a
t2	1
drop table t1, t2;
call sp3();
func	f1()
func	10
select 't1',a from t1;
t1	a
t1	10
drop table t1;
drop procedure sp1;
drop procedure sp2;
drop procedure sp3;
drop function f1;
create procedure sp1()
begin
create temporary table t2(a int);
insert into t2 select * from t1;
end//
create procedure sp2()
begin
create temporary table t1 (a int);
insert into t1 values(1);
call sp1();
select 't1', a from t1;
select 't2', a from t2;
drop table t1;
drop table t2;
end//
call sp2();
t1	a
t1	1
t2	a
t2	1
drop procedure sp1;
drop procedure sp2;
create table t1 (a int);
insert into t1 values(1),(2);
create table t2 as select * from t1;
create table t3 as select * from t1;
create table t4 as select * from t1;
create procedure sp1(a int)
begin
select a;
end //
create function f1() returns int
begin
return (select max(a) from t1);
end //
CALL sp1(f1());
a
2
create procedure sp2(a int)
begin
select * from t3;
select a;
end //
create procedure sp3()
begin 
select * from t1;
call sp2(5);
end //
create procedure sp4()
begin 
select * from t2;
call sp3();
end //
call sp4();
a
1
2
a
1
2
a
1
2
a
5
drop procedure sp1;
drop procedure sp2;
drop procedure sp3;
drop procedure sp4;
drop function f1;
drop view if exists v1;
create function f1(ab int) returns int
begin
declare i int;
set i= (select max(a) from t1 where a < ab) ;
return i;
end //
create function f2(ab int) returns int
begin
declare i int;
set i= (select max(a) from t2 where a < ab) ;
return i;
end //
create view v1 as 
select t3.a as x, t4.a as y, f2(3) as z
from t3, t4 where t3.a = t4.a //
create procedure sp1()
begin
declare a int;
set a= (select f1(4) + count(*) A from t1, v1);
end //
create function f3() returns int
begin
call sp1();
return 1;
end //
call sp1() //
select f3() //
f3()
1
select f3() //
f3()
1
call sp1() //
drop procedure sp1//
drop function f3//
create procedure sp1() 
begin 
declare x int;
declare c cursor for select f1(3) + count(*) from v1;
open c;
fetch c into x;
end;//
create function f3() returns int
begin
call sp1();
return 1;
end //
call sp1() //
call sp1() //
select f3() //
f3()
1
call sp1() //
drop view v1;
drop table t1,t2,t3,t4;
drop function f1;
drop function f2;
drop function f3;
drop procedure sp1;
drop table if exists t1;
drop view if exists v1, v2, v3;
drop function if exists bug15683;
create table t1 (f1 bigint, f2 varchar(20), f3 bigint);
insert into t1 set f1 = 1, f2 = 'schoenenbourg', f3 = 1;
create view v1 as select 1 from t1 union all select 1;
create view v2 as select 1 from v1;
create view v3 as select 1 as f1 from v2;
create function bug15683() returns bigint
begin
return (select count(*) from v3);
end|
prepare stmt from "select bug15683()";
execute stmt;
bug15683()
2
execute stmt;
bug15683()
2
deallocate prepare stmt;
drop table t1;
drop view v1, v2, v3;
drop function bug15683;
drop table if exists t1, t2, t3;
drop function if exists bug19634;
create table t1 (id int, data int);
create table t2 (id int);
create table t3 (data int);
create function bug19634() returns int return (select count(*) from t3);
prepare stmt from "delete t1 from t1, t2 where t1.id = t2.id and bug19634()";
execute stmt;
execute stmt;
deallocate prepare stmt;
create trigger t1_bi before delete on t1 for each row insert into t3 values (old.data);
prepare stmt from "delete t1 from t1, t2 where t1.id = t2.id";
execute stmt;
execute stmt;
deallocate prepare stmt;
drop function bug19634;
drop table t1, t2, t3;
drop table if exists bug_27907_logs;
drop table if exists bug_27907_t1;
create table bug_27907_logs (a int);
create table bug_27907_t1 (a int);
create trigger bug_27907_t1_ai after insert on bug_27907_t1
for each row
begin
insert into bug_27907_logs (a) values (1);
end|
drop table bug_27907_logs;
insert into bug_27907_t1(a) values (1);
ERROR 42S02: Table 'test.bug_27907_logs' doesn't exist
drop table bug_27907_t1;

Bug#22427 create table if not exists + stored function results in
inconsistent behavior

Add a test case, the bug itself was fixed by the patch for
Bug#20662

drop table if exists t1;
drop function if exists f_bug22427;
create table t1 (i int);
insert into t1 values (1);
create function f_bug22427() returns int return (select max(i) from t1);
select f_bug22427();
f_bug22427()
1
create table if not exists t1 select f_bug22427() as i;
Warnings:
Note	1050	Table 't1' already exists
create table t1 select f_bug22427() as i;
ERROR 42S01: Table 't1' already exists
drop table t1;
drop function f_bug22427;
#
# Bug #29929 LOCK TABLES does not pre-lock tables used in triggers of the locked tables
#
DROP table IF EXISTS t1,t2;
CREATE TABLE t1 (c1 INT);
CREATE TABLE t2 (c2 INT);
INSERT INTO t1 VALUES (1);
INSERT INTO t2 VALUES (2);
CREATE TRIGGER t1_ai AFTER INSERT ON t1 FOR EACH ROW
BEGIN
UPDATE t2 SET c2= c2 + 1;
END//
# Take a table lock on t1.
# This should pre-lock t2 through the trigger.
LOCK TABLE t1 WRITE;
INSERT INTO t1 VALUES (3);
UNLOCK TABLES;
LOCK TABLE t1 READ;
INSERT INTO t2 values(4);
ERROR HY000: Table 't2' was not locked with LOCK TABLES
UNLOCK TABLES;
SELECT * FROM t1;
c1
1
3
SELECT * FROM t2;
c2
3
DROP TRIGGER t1_ai;
DROP TABLE t1, t2;
End of 5.0 tests
#
# Bug#21142859: FUNCTION UPDATING A VIEW FAILS TO FIND TABLE THAT ACTUALLY EXISTS
#
CREATE TABLE t1 SELECT 1 AS fld1, 'A' AS fld2;
CREATE TABLE t2 (fld3 INT, fld4 CHAR(1));
CREATE VIEW v1 AS SELECT * FROM t1;
CREATE TRIGGER t1_au AFTER UPDATE ON t1
FOR EACH ROW INSERT INTO t2 VALUES (new.fld1, new.fld2);
CREATE FUNCTION f1() RETURNS INT
BEGIN
UPDATE v1 SET fld2='B' WHERE fld1=1;
RETURN row_count();
END !
# Without the patch, an error was getting reported.
SELECT f1();
f1()
1
DROP FUNCTION f1;
DROP VIEW v1;
DROP TABLE t1,t2;
#
# Bug #16672723 "CAN'T FIND TEMPORARY TABLE".
#
CREATE FUNCTION f1() RETURNS INT RETURN 1;
CREATE TEMPORARY TABLE tmp1(a INT);
PREPARE stmt1 FROM "CREATE TEMPORARY TABLE tmp2 AS SELECT b FROM (SELECT f1() AS b FROM tmp1) AS t";
# The below statement failed before the fix.
EXECUTE stmt1;
DROP TEMPORARY TABLES tmp1, tmp2;
DEALLOCATE PREPARE stmt1;
DROP FUNCTION f1;
