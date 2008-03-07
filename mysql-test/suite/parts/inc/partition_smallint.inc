eval create table t1 (a smallint unsigned not null, primary key(a)) engine=$engine 
partition by key (a) (
partition pa1 max_rows=20 min_rows=2,
partition pa2 max_rows=30 min_rows=3,
partition pa3 max_rows=30 min_rows=4,
partition pa4 max_rows=40 min_rows=2);
show create table t1;
insert into t1 values (65535), (65534), (65533), (65532), (1), (2), (256);
select * from t1;
select * from t1 where a=65533;
delete from t1 where a=65533;
select * from t1;
drop table t1;

eval create table t2 (a smallint unsigned not null, primary key(a)) engine=$engine 
partition by key (a) partitions 8;
show create table t2;
insert into t2 values (65535), (65534), (65533), (65532);
select * from t2;
select * from t2 where a=65533;
delete from t2 where a=65533;
select * from t2;
delete from t2;
let $count=$maxrows;
--echo $count inserts;
--disable_query_log
while ($count)
{
eval insert into t2 values ($count);
dec $count;
}
--enable_query_log
select count(*) from t2;
drop table t2;

eval create table t3 (a smallint not null, primary key(a)) engine=$engine 
partition by key (a) partitions 10;
show create table t3;
insert into t3 values (32767), (32766), (32765), (32764), (-32768), (-32767), (1), (-1), (0);
select * from t3;
select * from t3 where a=32765;
delete from t3 where a=32765;
select * from t3;
drop table t3;
