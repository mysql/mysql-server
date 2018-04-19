SET @@session.default_storage_engine = 'MyISAM';
create table t1 (a int, 
b int generated always as (a/10) virtual,
c int generated always as (a/10) stored);
create table t2 (a timestamp);
create trigger trg1 before insert on t1 for each row
begin
if (new.b < 10) then
set new.a:= 100;
set new.b:= 9;
set new.c:= 9;
end if;
if (new.c > 50) then
set new.a:= 500; 
end if;
end|
create trigger trg2 after insert on t1 for each row
begin
if (new.b >= 60) then
insert into t2 values (now()); 
end if;
end|
create function f1()
returns int
begin
declare sum1 int default '0';
declare cur1 cursor for select sum(b) from t1;
open cur1;
fetch cur1 into sum1;
close cur1;
return sum1;
end|
set sql_warnings = 1;
insert into t1 (a) values (200);
select * from t1;
a	b	c
200	20	20
select * from t2;
a
insert into t1 (a) values (10);
select * from t1;
a	b	c
100	10	10
200	20	20
select * from t2;
a
insert into t1 (a) values (600);
select * from t1;
a	b	c
100	10	10
200	20	20
500	50	50
select * from t2;
a
select f1();
f1()
80
set sql_warnings = 0;
drop trigger trg1;
drop trigger trg2;
drop table t2;
create procedure p1()
begin
declare i int default '0';
create table t2 like t1;
insert into t2 (a) values (100), (200);
begin
declare cur1 cursor for select sum(c) from t2;
open cur1;
fetch cur1 into i;
close cur1;
if (i=30) then
insert into t1 values (300,default,default);
end if;
end;
end|
delete from t1;
call p1();
select * from t2;
a	b	c
100	10	10
200	20	20
select * from t1;
a	b	c
300	30	30
drop table t1,t2;
drop procedure p1;
DROP VIEW  IF EXISTS v1,v2;
DROP TABLE IF EXISTS t1,t2,t3;
DROP PROCEDURE IF EXISTS p1;
DROP FUNCTION IF EXISTS f1;
DROP TRIGGER IF EXISTS trg1;
DROP TRIGGER IF EXISTS trg2;
set sql_warnings = 0;
