#
# Test for character set related things in combination
# with the partition storage engine
# 

--disable_warnings
drop table if exists t1;
--enable_warnings

set names utf8mb4;
-- error ER_SAME_NAME_PARTITION
create table t1 (s1 int)
  partition by list (s1)
    (partition c values in (1),
     partition Ç values in (3));
create table t1 (s1 int)
  partition by list (s1)
    (partition c values in (1),
     partition d values in (3));
insert into t1 values (1),(3);
select * from t1;
flush tables;
set names latin1;
select * from t1;
drop table t1;

-- error ER_PARTITION_FUNCTION_IS_NOT_ALLOWED
create table t1 (a varchar(1), primary key (a))
partition by list (ascii(a))
(partition p1 values in (65));
#insert into t1 values ('A');
#replace into t1 values ('A');
#drop table t1;
