--disable_warnings
drop table if exists t1;
--enable_warnings

--error ER_TOO_BIG_DISPLAYWIDTH
eval create table t1 (a bit(65), primary key (a)) engine=$engine partition by key (a);

eval create table t1 (a bit(0), primary key (a)) engine=$engine partition by key (a);
show create table t1;
drop table t1;

eval create table t1 (a bit(0), primary key (a)) engine=$engine 
partition by key (a) (
partition pa1,
partition pa2);
show create table t1;
drop table t1;

eval create table t1 (a bit(64), primary key (a)) engine=$engine 
partition by key (a) partitions 2;
show create table t1;
insert into t1 values 
(b'1111111111111111111111111111111111111111111111111111111111111111'),
(b'1000000000000000000000000000000000000000000000000000000000000000'),
(b'0000000000000000000000000000000000000000000000000000000000000001'),
(b'1010101010101010101010101010101010101010101010101010101010101010'),
(b'0101010101010101010101010101010101010101010101010101010101010101');
--sorted_result
select hex(a) from t1;
drop table t1;

eval create table t1 (a bit(64), primary key (a)) engine=$engine 
partition by key (a) (
partition pa1 max_rows=20 min_rows=2,
partition pa2 max_rows=30 min_rows=3,
partition pa3 max_rows=30 min_rows=4,
partition pa4 max_rows=40 min_rows=2);
show create table t1;
insert into t1 values 
(b'1111111111111111111111111111111111111111111111111111111111111111'),
(b'1000000000000000000000000000000000000000000000000000000000000000'),
(b'0000000000000000000000000000000000000000000000000000000000000001'),
(b'1010101010101010101010101010101010101010101010101010101010101010'),
(b'0101010101010101010101010101010101010101010101010101010101010101');
select hex(a) from t1 where a=b'0101010101010101010101010101010101010101010101010101010101010101';
delete from t1 where a=b'0101010101010101010101010101010101010101010101010101010101010101';
--sorted_result
select hex(a) from t1;
drop table t1;

eval create table t2 (a bit, primary key (a)) engine=$engine 
partition by key (a) partitions 4;
show create table t2;
insert into t2 values (b'0'), (b'1');
--sorted_result
select hex(a) from t2;
alter table t2 drop primary key;
show create table t2;
--sorted_result
select hex(a) from t2;
alter table t2 add primary key (a);
show create table t2;
--sorted_result
select hex(a) from t2;
drop table t2;

eval create table t3 (a bit(8), primary key (a)) engine=$engine 
partition by range (a) subpartition by key (a) subpartitions 2 (
partition pa1 values less than (3),
partition pa2 values less than (16),
partition pa3 values less than (64),
partition pa4 values less than (256));
show create table t3;
let $count=255;
--echo $count inserts;
--disable_query_log
begin;
while ($count)
{
eval insert into t3 values ($count);
dec $count;
}
commit;
--enable_query_log
select hex(a) from t3 where a=b'01010101';
delete from t3 where a=b'01010101';
select count(*) from t3;
--sorted_result
select hex(a) from t3;
drop table t3;

eval create table t4 (a bit(8), primary key (a)) engine=$engine 
partition by list (a) subpartition by key (a) subpartitions 2 (
partition pa1 values in (0,1,2,3),
partition pa2 values in (4,5,6,7,8,9,10,11,12,13,14,15,16),
partition pa3 values in (17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32));
show create table t4;
let $count=32;
--echo $count inserts;
--disable_query_log
begin;
while ($count)
{
eval insert into t4 values ($count);
dec $count;
}
commit;
--enable_query_log
select hex(a) from t4 where a=b'00000001';
delete from t4 where a=b'00000001';
select count(*) from t4;
--sorted_result
select hex(a) from t4;
drop table t4;
