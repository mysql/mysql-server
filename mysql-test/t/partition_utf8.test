 #Get deafult engine value
--let $DEFAULT_ENGINE = `select @@global.default_storage_engine`

# Tests for Column list which requires utf8mb3 output
set names utf8mb3;
create table t1 (a varchar(2) character set cp1250)
partition by list columns (a)
( partition p0 values in (0x81));

#Replace default engine value with static engine string 
--replace_result $DEFAULT_ENGINE ENGINE
show create table t1;
drop table t1;
create table t1 (a varchar(2) character set cp1250)
partition by list columns (a)
( partition p0 values in (0x80));

#Replace default engine value with static engine string 
--replace_result $DEFAULT_ENGINE ENGINE
show create table t1;
drop table t1;

#
# BUG#48164, too long partition fields causes crash
#
--error ER_PARTITION_FIELDS_TOO_LONG
create table t1 (a varchar(1500), b varchar(1570))
partition by list columns(a,b)
( partition p0 values in (('a','b')));

create table t1 (a varchar(1023) character set utf8mb3 COLLATE utf8mb3_spanish2_ci)
partition by range columns(a)
( partition p0 values less than ('CZ'),
  partition p1 values less than ('CH'),
  partition p2 values less than ('D'));
insert into t1 values ('czz'),('chi'),('ci'),('cg');
select * from t1 where a between 'cg' AND 'ci';
drop table t1;

#
# BUG#48163, Dagger in UCS2 not working as partition value
#
create table t1 (a varchar(2) character set ucs2)
partition by list columns (a)
(partition p0 values in (0x2020),
 partition p1 values in (''));

#Replace default engine value with static engine string 
--replace_result $DEFAULT_ENGINE ENGINE
show create table t1;
insert into t1 values ('');
insert into t1 values (_ucs2 0x2020);
drop table t1;
