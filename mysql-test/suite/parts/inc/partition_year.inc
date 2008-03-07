eval create table t1 (a year not null, primary key(a)) engine=$engine 
partition by key (a) (
partition pa1 max_rows=20 min_rows=2,
partition pa2 max_rows=30 min_rows=3,
partition pa3 max_rows=30 min_rows=4,
partition pa4 max_rows=40 min_rows=2);
show create table t1;
insert into t1 values ('1975'), (2020), ('1980'), ('2000');
select * from t1;
select * from t1 where a=1980;
delete from t1 where a=1980;
select * from t1;
drop table t1;

eval create table t2 (a year not null, primary key(a)) engine=$engine 
partition by key (a) partitions 12;
show create table t2;
insert into t2 values ('1975'), ('2020'), ('1980'), ('2000');
select * from t2;
select * from t2 where a='1980';
delete from t2 where a='1980';
select * from t2;
delete from t2;
let $count=255;
--echo $count inserts;
--disable_query_log
while ($count)
{
eval insert into t2 values (1901+$count);
dec $count;
}
--enable_query_log
select count(*) from t2;
select * from t2;
drop table t2;

