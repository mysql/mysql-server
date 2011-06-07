
--source include/have_innodb.inc

--disable_warnings
drop table if exists t1, t2, t3;
--enable_warnings

--error ER_TOO_BIG_PRECISION
eval create table t1 (a $type(7));

eval create table t1 (a $type(3), key(a));
insert t1 values ('2010-12-11 00:20:03.1234');
insert t1 values ('2010-12-11 15:47:11.1234');
insert t1 values (20101211010203.45678);
insert t1 values (20101211030405.789e0);
insert t1 values (99991231235959e1);
select * from t1;
--replace_regex /121000/121094/ /457000/457031/ /789000/789062/
select truncate(a, 6) from t1; # Field::val_real()
select a DIV 1 from t1; # Field::val_int()
select group_concat(distinct a) from t1; # Field::cmp()
alter table t1 engine=innodb;
select * from t1 order by a;
select * from t1 order by a+0;
drop table t1;
eval create table t1 (a $type(4)) engine=innodb;
insert t1 values ('2010-12-11 01:02:03.456789');
select * from t1;
select extract(microsecond from a + interval 100 microsecond) from t1 where a>'2010-11-12 01:02:03.456';
select a from t1 where a>'2010-11-12 01:02:03.456' group by a;

#
# metadata
#
show create table t1;
show columns from t1;
--query_vertical select table_name, column_name, column_default, is_nullable, data_type, character_maximum_length, character_octet_length, numeric_precision, numeric_scale, datetime_precision, character_set_name, collation_name, column_type, column_key, extra from information_schema.columns where table_name='t1'

#
# update/delete
#
select a, a+interval 9876543 microsecond from t1;
update t1 set a=a+interval 9876543 microsecond;
select * from t1;
select a, a + interval 2 year from t1;
insert t1 select a + interval 2 year from t1;
select * from t1;
delete from t1 where a < 20110101;
select * from t1;

#
# create ... select
#
create table t2 select * from t1;
create table t3 like t1;

show create table t2;
show create table t3;
drop table t2, t3;

# math, aggregation
insert t1 values ('2010-12-13 14:15:16.222222');
select a, a+0, a-1, a*1, a/2 from t1;
select max(a), min(a), sum(a), avg(a) from t1;
create table t2 select a, a+0, a-1, a*1, a/2 from t1;
create table t3 select max(a), min(a), sum(a), avg(a) from t1;
show create table t2;
show create table t3;

drop table t1, t2, t3;

# insert, alter with conversion
--vertical_results
eval create table t1 (f0_$type $type(0), f1_$type $type(1), f2_$type $type(2), f3_$type $type(3), f4_$type $type(4), f5_$type $type(5), f6_$type $type(6));
insert t1 values ( '2010-11-12 11:14:17.765432', '2010-11-12 11:14:17.765432', '2010-11-12 11:14:17.765432', '2010-11-12 11:14:17.765432', '2010-11-12 11:14:17.765432', '2010-11-12 11:14:17.765432', '2010-11-12 11:14:17.765432');
select * from t1;
eval select cast(f0_$type as time(4)) time4_f0_$type, cast(f1_$type as datetime(3)) datetime3_f1_$type, cast(f2_$type as date) date_f2_$type, cast(f4_$type as double) double_f3_$type, cast(f4_$type as decimal(40,5)) decimal5_f4_$type, cast(f5_$type as signed) bigint_f5_$type, cast(f6_$type as char(255)) varchar_f6_$type from t1;
eval create table t2 (time4_f0_$type time(4), datetime3_f1_$type datetime(3), date_f2_$type date, double_f3_$type double, decimal5_f4_$type decimal(40,5), bigint_f5_$type bigint, varchar_f6_$type varchar(255));
insert t2 select * from t1;
select * from t2;
eval alter table t1 change f0_$type time4_f0_$type time(4), change f1_$type datetime3_f1_$type datetime(3), change f2_$type date_f2_$type date, change f3_$type double_f3_$type double, change f4_$type decimal5_f4_$type decimal(40,5), change f5_$type bigint_f5_$type bigint, change f6_$type varchar_f6_$type varchar(255);
select * from t1;
eval alter table t1 modify time4_f0_$type $type(0), modify datetime3_f1_$type $type(1), modify date_f2_$type $type(2), modify double_f3_$type $type(3), modify decimal5_f4_$type $type(4), modify bigint_f5_$type $type(5), modify varchar_f6_$type $type(6);
select * from t1;
delete from t1;
insert t1 select * from t2;
select * from t1;
drop table t1, t2;
--horizontal_results

#
# SP
#
eval create table t1 (a $type(6), b $type(6));
eval create procedure foo(x $type, y $type(4)) insert into t1 values (x, y);
call foo('2010-02-03 4:5:6.789123', '2010-02-03 4:5:6.789123');
select * from t1;
delimiter |;
eval create procedure bar(a int, c $type(5))
begin
  declare b $type(4);
  set b = c + interval a microsecond;
  insert t1 values (b, c + interval a microsecond);
end|
delimiter ;|
call bar(1111111, '2011-01-02 3:4:5.123456');
select * from t1;
drop procedure foo;
drop procedure bar;
eval create function xyz(s char(20)) returns $type(4)
       return addtime('2010-10-10 10:10:10.101010', s);
select xyz('1:1:1.010101');
drop function xyz;

#
# Views
#

create view v1 as select * from t1 group by a,b;
select * from v1;
show columns from v1;
create table t2 select * from v1;
show create table t2;
select * from t2;

drop view v1;
drop table t1, t2;

