--disable_query_log
# DATA DIRECTORY
eval SET @data_dir = 'DATA DIRECTORY =
''/tmp''';
let $data_directory = `select @data_dir`;

#INDEX DIRECTORY
eval SET @indx_dir = 'INDEX DIRECTORY =
''/tmp''';
let $index_directory = `select @indx_dir`;
--enable_query_log

eval create table t1 (a decimal(10,4) not null, primary key(a)) engine=$engine 
partition by key (a) (
partition pa1 $data_directory $index_directory max_rows=20 min_rows=2,
partition pa2 $data_directory $index_directory max_rows=30 min_rows=3,
partition pa3 $data_directory $index_directory max_rows=30 min_rows=4,
partition pa4 $data_directory $index_directory max_rows=40 min_rows=2);
show create table t1;
insert into t1 values (999999.9999), (-999999.9999), (123456.7899), (-123456.7899), (-1.5), (1), (0), (-1), (1.5), (1234.567), (-1234.567);
select * from t1;
select * from t1 where a=1234.567;
delete from t1 where a=1234.567;
select * from t1;
drop table t1;

eval create table t2 (a decimal(18,9) not null, primary key(a)) engine=$engine 
partition by key (a) partitions 10;
show create table t2;
insert into t2 values (999999999.999999999), (-999999999.999999999), (-1.5), (-1), (0), (1.5), (1234.567), (-1234.567);
select * from t2;
select * from t2 where a=1234.567;
delete from t2 where a=1234.567;
select * from t2;
delete from t2;
let $count=$maxrows;
--echo $count*3 inserts;
--disable_query_log
while ($count)
{
eval insert into t2 values ($count);
eval insert into t2 values ($count+0.333333333);
eval insert into t2 values ($count+0.755555555);
dec $count;
}
--enable_query_log
select count(*) from t2;
drop table t2;

eval create table t3 (a decimal(18,9) not null, primary key(a)) engine=$engine 
partition by range (cast(floor(a) as signed)) subpartition by key (a) subpartitions 2 (
partition pa2 values less than (2),
partition pa4 values less than (4),
partition pa6 values less than (6),
partition pa8 values less than (8),
partition pa10 values less than (10)
);
show create table t3;
let $count=9;
--echo $count*3 inserts;
while ($count)
{
eval insert into t3 values ($count);
eval insert into t3 values ($count+0.333333333);
eval insert into t3 values ($count+0.755555555);
dec $count;
}
--enable_query_log
select count(*) from t3;
drop table t3;

eval create table t4 (a decimal(18,9) not null, primary key(a)) engine=$engine 
partition by list (cast(floor(a) as signed)) subpartition by key (a) subpartitions 2 (
partition pa2 values in (1,2),
partition pa4 values in (3,4),
partition pa6 values in (5,6),
partition pa8 values in (7,8),
partition pa10 values in (9,10)
);
show create table t4;
let $count=9;
--echo $count*3 inserts;
while ($count)
{
eval insert into t4 values ($count);
eval insert into t4 values ($count+0.333333333);
eval insert into t4 values ($count+0.755555555);
dec $count;
}
--enable_query_log
select count(*) from t4;
drop table t4;
