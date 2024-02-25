use test;
create temporary table t1
(i int, b mediumblob,
primary key pk(i), index sk(b(100))
) engine=innodb;
create procedure populate_t1()
begin
declare i int default 1;
while (i <= 100) do
insert into t1 values (i, repeat(i,1000));
set i = i + 1;
end while;
end|
create procedure populate_t1_small()
begin
declare i int default 1;
while (i <= 8) do
insert into t1 values (i, repeat(i,1000));
set i = i + 1;
end while;
end|
begin;
select count(*) from t1;
count(*)
0
call populate_t1();
select count(*) from t1;
count(*)
100
delete from t1 where i < 10;
select count(*) from t1;
count(*)
91
commit;
set global innodb_purge_stop_now=ON;
set global innodb_purge_run_now=ON;
delete from t1 where i < 20;
select count(*) from t1;
count(*)
81
begin;
call populate_t1_small();
select count(*) from t1;
count(*)
89
rollback;
select count(*) from t1;
count(*)
81
begin;
select count(*) from t1;
count(*)
81
update t1 set b = repeat('a', 100) where i < 100;
select * from t1 limit 1;
i	b
20	aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa
select count(*) from t1;
count(*)
81
rollback;
update t1 set b = repeat('a', 100) where i < 100;
select * from t1 limit 1;
i	b
20	aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa
truncate table t1;
select count(*) from t1;
count(*)
0
drop table if exists t1;
drop procedure populate_t1;
drop procedure populate_t1_small;
