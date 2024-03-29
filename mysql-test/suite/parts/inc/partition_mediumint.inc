eval create table t1 (a mediumint unsigned not null, primary key(a)) engine=$engine 
partition by key (a) (
partition pa1 max_rows=20 min_rows=2,
partition pa2 max_rows=30 min_rows=3,
partition pa3 max_rows=30 min_rows=4,
partition pa4 max_rows=40 min_rows=2);
show create table t1;
insert into t1 values (16777215), (16777214), (16777213), (16777212), (1), (2), (65535);
--sorted_result
select * from t1;
select * from t1 where a=16777213;
delete from t1 where a=16777213;
--sorted_result
select * from t1;
drop table t1;

eval create table t2 (a mediumint unsigned not null, primary key(a)) engine=$engine 
partition by key (a) partitions 8;
show create table t2;
insert into t2 values (16777215), (16777214), (16777213), (16777212);
--sorted_result
select * from t2;
select * from t2 where a=16777213;
delete from t2 where a=16777213;
--sorted_result
select * from t2;
delete from t2;
let $count=$maxrows;
--echo $maxrows inserts;
--disable_query_log
while ($count)
{
eval insert into t2 values ($count);
dec $count;
}
--enable_query_log
select count(*) from t2;
drop table t2;

eval create table t3 (a mediumint not null, primary key(a)) engine=$engine 
partition by key (a) partitions 7;
show create table t3;
insert into t3 values (8388607), (8388606), (8388605), (8388604), (-8388608), (-8388607), (1), (-1), (0);
--sorted_result
select * from t3;
select * from t3 where a=8388605;
delete from t3 where a=8388605;
--sorted_result
select * from t3;
drop table t3;
