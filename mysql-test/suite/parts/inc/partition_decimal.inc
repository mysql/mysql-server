eval create table t1 (a decimal(10,4) not null, primary key(a)) engine=$engine 
partition by key (a) (
partition pa1 max_rows=20 min_rows=2,
partition pa2 max_rows=30 min_rows=3,
partition pa3 max_rows=30 min_rows=4,
partition pa4 max_rows=40 min_rows=2);
show create table t1;
insert into t1 values (999999.9999), (-999999.9999), (123456.7899), (-123456.7899), (-1.5), (1), (0), (-1), (1.5), (1234.567), (-1234.567);
select * from t1;
select * from t1 where a=1234.567;
delete from t1 where a=1234.567;
select * from t1;
drop table t1;

eval create table t2 (a decimal(18,9) not null, primary key(a)) engine=$engine 
partition by key (a) partitions 10;
show create table t2;
insert into t2 values (999999999.999999999), (-999999999.999999999), (-1.5), (-1), (0), (1.5), (1234.567), (-1234.567);
select * from t2;
select * from t2 where a=1234.567;
delete from t2 where a=1234.567;
select * from t2;
delete from t2;
let $count=$maxrows;
--echo $count*3 inserts;
--disable_query_log
begin;
while ($count)
{
eval insert into t2 values ($count);
eval insert into t2 values ($count+0.333333333);
eval insert into t2 values ($count+0.755555555);
dec $count;
}
commit;
--enable_query_log
select count(*) from t2;
drop table t2;

# Bug 30577: FLOOR() and CEILING() not usable as partition functions
# Partition functions are required to return INT_RESULT; FLOOR() and
# CEILING() do not, unless they have an INT or DECIMAL argument.


eval create table t3 (a decimal(18,9) not null, primary key(a)) engine=$engine 
partition by range (floor(a)) subpartition by key (a) subpartitions 2 (
partition pa2 values less than (2),
partition pa4 values less than (4),
partition pa6 values less than (6),
partition pa8 values less than (8),
partition pa10 values less than (10)
);
show create table t3;
let $count=9;
--echo $count*3 inserts;
--disable_query_log
begin;
while ($count)
{
eval insert into t3 values ($count);
eval insert into t3 values ($count+0.333333333);
eval insert into t3 values ($count+0.755555555);
dec $count;
}
commit;
--enable_query_log
select count(*) from t3;
drop table t3;

eval create table t4 (a decimal(18,9) not null, primary key(a)) engine=$engine 
partition by list (ceiling(a)) subpartition by key (a) subpartitions 2 (
partition pa2 values in (1,2),
partition pa4 values in (3,4),
partition pa6 values in (5,6),
partition pa8 values in (7,8),
partition pa10 values in (9,10)
);
show create table t4;
let $count=9;
--echo $count*3 inserts;
--disable_query_log
begin;
while ($count)
{
eval insert into t4 values ($count);
eval insert into t4 values ($count+0.333333333);
eval insert into t4 values ($count+0.755555555);
dec $count;
}
commit;
--enable_query_log
select count(*) from t4;
drop table t4;
