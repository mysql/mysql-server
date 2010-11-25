eval create table t1 (a int unsigned not null, primary key(a)) engine=$engine 
partition by key (a) (
partition pa1 max_rows=20 min_rows=2,
partition pa2 max_rows=30 min_rows=3,
partition pa3 max_rows=30 min_rows=4,
partition pa4 max_rows=40 min_rows=2);
show create table t1;
insert into t1 values (4294967295), (4294967294), (4294967293), (4294967292), (1), (2), (65535);
--sorted_result
select * from t1;
select * from t1 where a=4294967293;
delete from t1 where a=4294967293;
--sorted_result
select * from t1;
drop table t1;

eval create table t2 (a int unsigned not null, primary key(a)) engine=$engine 
partition by key (a) partitions 8;
show create table t2;
insert into t2 values (4294967295), (4294967294), (4294967293), (4294967292);
--sorted_result
select * from t2;
select * from t2 where a=4294967293;
delete from t2 where a=4294967293;
--sorted_result
select * from t2;
delete from t2;
let $count=$maxrows;
--echo $count inserts;
--disable_query_log
begin;
while ($count)
{
eval insert into t2 values ($count);
dec $count;
}
commit;
--enable_query_log
select count(*) from t2;
drop table t2;

eval create table t3 (a int not null, primary key(a)) engine=$engine 
partition by key (a) partitions 7;
show create table t3;
insert into t3 values (2147483647), (2147483646), (2147483645), (2147483644), (-2147483648), (-2147483647), (1), (-1), (0);
--sorted_result
select * from t3;
select * from t3 where a=2147483645;
delete from t3 where a=2147483645;
--sorted_result
select * from t3;
drop table t3;
