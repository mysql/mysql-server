eval create table t1 (a timestamp not null, primary key(a)) engine=$engine 
partition by key (a) (
partition pa1 max_rows=20 min_rows=2,
partition pa2 max_rows=30 min_rows=3,
partition pa3 max_rows=30 min_rows=4,
partition pa4 max_rows=40 min_rows=2);
show create table t1;
insert into t1 values ('1975-01-01 21:21:21'), ('2020-12-31 12:10:30'), ('1980-10-14 03:03'), ('2000-06-15 23:59');
select * from t1;
select * from t1 where a=19801014030300;
delete from t1 where a=19801014030300;
select * from t1;
drop table t1;

eval create table t2 (a timestamp not null, primary key(a)) engine=$engine 
partition by key (a) partitions 12;
show create table t2;
insert into t2 values ('1975-01-01 0:1:1'), ('2020-12-31 10:11:12'), ('1980-10-14 13:14:15'), ('2000-06-15 14:15:16');
select * from t2;
select * from t2 where a='1980-10-14 13:14:15';
delete from t2 where a='1980-10-14 13:14:15';
select * from t2;
delete from t2;
let $count=59;
--echo $count inserts;
#--disable_query_log
while ($count)
{
eval insert into t2 values (19710101000000+$count);
dec $count;
}
select count(*) from t2;
select * from t2;
drop table t2;

eval create table t3 (a timestamp not null, primary key(a)) engine=$engine 
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
while ($count)
{
eval insert into t3 values (date_add('1970-01-01 00:00:00',interval $count-1 month));
dec $count;
}
select count(*) from t3;
select * from t3;
drop table t3;

eval create table t4 (a timestamp not null, primary key(a)) engine=$engine 
partition by list (month(a)) subpartition by key (a)
subpartitions 3 (
partition quarter1 values in (0,1,2,3),
partition quarter2 values in (4,5,6),
partition quarter3 values in (7,8,9), 
partition quarter4 values in (10,11,12)
);
show create table t4;
let $count=12;
--echo $count inserts;
while ($count)
{
eval insert into t4 values (date_add('1970-01-01 00:00:00',interval $count-1 month));
dec $count;
}
select count(*) from t4;
select * from t4;
drop table t4;
