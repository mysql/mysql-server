drop table if exists t1, t2, t3, t4;
create table t1 (a int)
partition by list (a)
(partition p1 values in (0));
alter table t1 truncate partition p1,p1;
alter table t1 truncate partition p0;
ERROR HY000: Unknown partition 'p0' in table 't1'
drop table t1;
create table t1 (a int)
partition by list (a)
subpartition by hash (a)
subpartitions 1
(partition p1 values in (1)
(subpartition sp1));
alter table t1 truncate partition sp1;
drop table t1;
create table t1 (a int);
insert into t1 values (1), (3), (8);
alter table t1 truncate partition p0;
ERROR HY000: Partition management on a not partitioned table is not possible
select count(*) from t1;
count(*)
3
drop table t1;
# Additional coverage for WL#7743 "New data dictionary: changes
# to DDL-related parts of SE API".
#
# Test how failed call to Partition_handler::truncate_partition()
# is handled.
call mtr.add_suppression("\\[Warning\\] .*MY-\\d+.* Missing .ibd file for table `test`\.`t1` .* ");
create table t1 (id int)
partition by range (id)
(partition p0 values less than (1000),
partition p1 values less than (maxvalue));
# Discard partition's tablespace to force
# Partition_handler::truncate_partition() failure.
alter table t1 discard partition p0 tablespace;
alter table t1 truncate partition p0;
ERROR HY000: Tablespace has been discarded for table './test/t1'
drop table t1;
