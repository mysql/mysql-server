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
while ($count)
{
eval insert into t2 values ($count);
eval insert into t2 values ($count+0.33);
eval insert into t2 values ($count+0.75);
dec $count;
}
--enable_query_log
select count(*) from t2;
drop table t2;

# Bug 30577: FLOOR() and CEILING() not usable as partition functions
# Partition functions are required to return INT_RESULT; FLOOR() and
# CEILING() do not, unless they have an INT argument.  Disable this
# portion of the test until bug 30577 is fixed.

--disable_parsing

eval create table t3 (a float not null, primary key(a)) engine=$engine 
partition by range (floor(a)) subpartition by key (a) subpartitions 3 (
partition pa1 values less than (3),
partition pa3 values less than (6),
partition pa10 values less than (10)
);
show create table t3;
let $count=9;
--echo $count*3 inserts;
while ($count)
{
eval insert into t3 values ($count);
eval insert into t3 values ($count+0.33);
eval insert into t3 values ($count+0.75);
dec $count;
}
select count(*) from t3;
select * from t3;
drop table t3;

eval create table t4 (a float not null, primary key(a)) engine=$engine 
partition by list (floor(a)) subpartition by key (a) subpartitions 3 (
partition pa1 values in (1,2,3),
partition pa3 values in (4,5,6),
partition pa10 values in (7,8,9,10)
);
show create table t4;
let $count=9;
--echo $count*3 inserts;
while ($count)
{
eval insert into t4 values ($count);
eval insert into t4 values ($count+0.33);
eval insert into t4 values ($count+0.75);
dec $count;
}
select count(*) from t4;
select * from t4;
drop table t4;

# Disabled due to Bug 30577
--enable_parsing
