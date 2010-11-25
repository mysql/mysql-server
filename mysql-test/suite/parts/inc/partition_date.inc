eval create table t1 (a date not null, primary key(a)) engine=$engine 
partition by key (a) (
partition pa1 max_rows=20 min_rows=2,
partition pa2 max_rows=30 min_rows=3,
partition pa3 max_rows=30 min_rows=4,
partition pa4 max_rows=40 min_rows=2);
show create table t1;
insert into t1 values ('1975-01-01'), ('2020-12-31'), ('1980-10-14'), ('2000-06-15');
select * from t1;
select * from t1 where a=19801014;
delete from t1 where a=19801014;
select * from t1;
drop table t1;

eval create table t2 (a date not null, primary key(a)) engine=$engine 
partition by key (a) partitions 12;
show create table t2;
insert into t2 values ('1975-01-01'), ('2020-12-31'), ('1980-10-14'), ('2000-06-15');
select * from t2;
select * from t2 where a='1980-10-14';
delete from t2 where a='1980-10-14';
select * from t2;
delete from t2;
let $count=28;
--echo $count inserts;
--disable_query_log
begin;
while ($count)
{
eval insert into t2 values (19700101+$count-1);
eval insert into t2 values (19700201+$count-1);
eval insert into t2 values (19700301+$count-1);
dec $count;
}
commit;
--enable_query_log
select count(*) from t2;
select * from t2;
drop table t2;

eval create table t3 (a date not null, primary key(a)) engine=$engine 
partition by range (month(a)) subpartition by key (a)
subpartitions 3 (
partition quarter1 values less than (4),
partition quarter2 values less than (7),
partition quarter3 values less than (10), 
partition quarter4 values less than (13)
);
show create table t3;
let $count=12;
--echo $count inserts;
--disable_query_log
begin;
while ($count)
{
eval insert into t3 values (adddate(19700101,interval $count-1 month));
dec $count;
}
commit;
--enable_query_log
select count(*) from t3;
select * from t3;
drop table t3;

eval create table t4 (a date not null, primary key(a)) engine=$engine 
partition by list (month(a)) subpartition by key (a)
subpartitions 3 (
partition quarter1 values in (1,2,3),
partition quarter2 values in (4,5,6),
partition quarter3 values in (7,8,9), 
partition quarter4 values in (10,11,12)
);
show create table t4;
let $count=12;
--echo $count inserts;
--disable_query_log
begin;
while ($count)
{
eval insert into t4 values (adddate(19700101,interval $count-1 month));
dec $count;
}
commit;
--enable_query_log
select count(*) from t4;
select * from t4;
drop table t4;
