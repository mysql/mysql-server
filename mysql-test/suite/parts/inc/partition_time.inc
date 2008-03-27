eval create table t1 (a time not null, primary key(a)) engine=$engine 
partition by key (a) (
partition pa1 max_rows=20 min_rows=2,
partition pa2 max_rows=30 min_rows=3,
partition pa3 max_rows=30 min_rows=4,
partition pa4 max_rows=40 min_rows=2);
show create table t1;
insert into t1 values ('21:21:21'), ('12:10:30'), ('03:03:03'), ('23:59');
select * from t1;
select * from t1 where a=030303;
delete from t1 where a=030303;
select * from t1;
drop table t1;

eval create table t2 (a time not null, primary key(a)) engine=$engine 
partition by key (a) partitions 12;
show create table t2;
insert into t2 values ('0:1:1'), ('10:11:12'), ('13:14:15'), ('14:15:16');
select * from t2;
select * from t2 where a='13:14:15';
delete from t2 where a='13:14:15';
select * from t2;
delete from t2;
let $count=59;
--echo $count inserts;
#--disable_query_log
while ($count)
{
eval insert into t2 values (000100+$count);
dec $count;
}
select count(*) from t2;
select * from t2;
drop table t2;

eval create table t3 (a time not null, primary key(a)) engine=$engine 
partition by range (second(a)) subpartition by key (a)
subpartitions 3 (
partition quarter1 values less than (16),
partition quarter2 values less than (31),
partition quarter3 values less than (46), 
partition quarter4 values less than (61)
);
show create table t3;
let $count=59;
--echo $count inserts;
while ($count)
{
eval insert into t3 values (100000+$count);
dec $count;
}
select count(*) from t3;
select * from t3;
drop table t3;

eval create table t4 (a time not null, primary key(a)) engine=$engine 
partition by list (second(a)) subpartition by key (a)
subpartitions 3 (
partition quarter1 values in (1,2,3,4,5,6,7,8,9,10,11,12,13,14,15),
partition quarter2 values in (16,17,18,19,20,21,22,23,24,25,26,27,28,29,30),
partition quarter3 values in (31,32,33,34,35,36,37,38,39,40,41,42,43,44,45), 
partition quarter4 values in (46,47,48,49,50,51,52,53,54,55,56,57,58,59,60)
);
show create table t4;
let $count=59;
--echo $count inserts;
while ($count)
{
eval insert into t4 values (100000+$count);
dec $count;
}
select count(*) from t4;
select * from t4;
drop table t4;
