eval create table t1 (a float not null, primary key(a)) engine=$engine 
partition by key (a) (
partition pa1 max_rows=20 min_rows=2,
partition pa2 max_rows=30 min_rows=3,
partition pa3 max_rows=30 min_rows=4,
partition pa4 max_rows=40 min_rows=2);
show create table t1;
insert into t1 values (-3.402823466E+38), (3.402823466E+38), (-1.5), (-1), (0), (1), (1.5);
select * from t1;
select * from t1 where a=1.5;
delete from t1 where a=1.5;
select * from t1;
drop table t1;

eval create table t2 (a float not null, primary key(a)) engine=$engine 
partition by key (a) partitions 10;
show create table t2;
insert into t2 values (-3.402823466E+38), (-3.402823466E+37), (-123.456), (0), (1234546.789), (123.456), (1.5);
select * from t2;
#result set is empty: Not a bug with float!!
select * from t2 where a=123.456;
delete from t2 where a=123.456;
select * from t2;
select * from t2 where a=1.5;
delete from t2 where a=1.5;
select * from t2;
delete from t2;
let $count=$maxrows;
--echo $maxrows*3 inserts;
--disable_query_log
begin;
while ($count)
{
eval insert into t2 values ($count);
eval insert into t2 values ($count+0.33);
eval insert into t2 values ($count+0.75);
dec $count;
}
commit;
--enable_query_log
select count(*) from t2;
drop table t2;
