#
# Bug #25807393 "IMPROPER HANDLING OF DATABASE NAMES WHEN LOCKING TABLES
#                FOR TRIGGER EXECUTION.".
#
create table t1 (i int);
create table t2 (i int);
create database mysqltest_A;
use mysqltest_A;
create table t1 (i int);
create trigger trg before insert on t1 for each row
insert into test.t1 values (1);
create database mysqltest_a;
use mysqltest_a;
create table t2 (i int);
create trigger trg before insert on t2 for each row
insert into test.t2 values (1);
use test;
create function f1() returns int
begin
insert into mysqltest_A.t1 values (1);
insert into mysqltest_a.t2 values (1);
return 0;
end|
select f1();
f1()
0
drop database mysqltest_A;
drop database mysqltest_a;
drop tables t1, t2;
drop function f1;
