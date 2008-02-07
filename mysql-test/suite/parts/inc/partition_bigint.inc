eval create table t1 (a bigint unsigned not null, primary key(a)) engine=$engine 
partition by key (a) (
partition pa1 max_rows=20 min_rows=2,
partition pa2 max_rows=30 min_rows=3,
partition pa3 max_rows=30 min_rows=4,
partition pa4 max_rows=40 min_rows=2);
show create table t1;
insert into t1 values (18446744073709551615), (0xFFFFFFFFFFFFFFFE), (18446744073709551613), (18446744073709551612), (1), (2), (65535);
select * from t1;
select * from t1 where a=-2;
delete from t1 where a=-2;
select * from t1;
select * from t1 where a=18446744073709551615;
delete from t1 where a=18446744073709551615;
select * from t1;
drop table t1;

eval create table t2 (a bigint unsigned not null, primary key(a)) engine=$engine 
partition by key (a) partitions 10;
show create table t2;
insert into t2 values (18446744073709551615), (0xFFFFFFFFFFFFFFFE), (18446744073709551613), (18446744073709551612);
select * from t2;
select * from t2 where a=18446744073709551615;
delete from t2 where a=18446744073709551615;
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

eval create table t3 (a bigint not null, primary key(a)) engine=$engine 
partition by key (a) partitions 10;
show create table t3;
insert into t3 values (9223372036854775807), (9223372036854775806), (9223372036854775805), (9223372036854775804), (-9223372036854775808), (-9223372036854775807), (1), (-1), (0);
select * from t3;
select * from t3 where a=9223372036854775806;
delete from t3 where a=9223372036854775806;
select * from t3;
drop table t3;
