--disable_warnings
drop table if exists t1,t2,t3,t4;
--enable_warnings

create table t1 (a int not null,b int not null,c int not null, primary key(a,b))
partition by list (b*a)
(partition x1 values in (1),
 partition x2 values in (3, 11, 5, 7),
 partition x3 values in (16, 8, 5+19, 70-43));

--sorted_result
--replace_column 16 # 19 # 20 #
--sorted_result
select * from information_schema.partitions where table_schema="test"
and table_name="t1";

create table t2 (a int not null,b int not null,c int not null, primary key(a,b))
partition by range (a)
partitions 3
(partition x1 values less than (5),
 partition x2 values less than (10),
 partition x3 values less than maxvalue);
--sorted_result
--replace_column 16 # 19 # 20 #
--sorted_result
select * from information_schema.partitions where table_schema="test"
and table_name="t2";

create table t3 (f1 date)
partition by hash(month(f1))
partitions 3;
--sorted_result
--replace_column 16 # 19 # 20 #
select * from information_schema.partitions where table_schema="test"
and table_name="t3";

create table t4 (f1 date, f2 int)
partition by key(f1,f2)
partitions 3;
--sorted_result
--replace_column 16 # 19 # 20 #
select * from information_schema.partitions where table_schema="test"
and table_name="t4";

drop table t1,t2,t3,t4;

create table t1 (a int not null,b int not null,c int not null,primary key (a,b))
partition by range (a)
subpartition by hash (a+b)
( partition x1 values less than (1)
  ( subpartition x11,
    subpartition x12),
   partition x2 values less than (5)
  ( subpartition x21,
    subpartition x22)
);

create table t2 (a int not null,b int not null,c int not null,primary key (a,b))
partition by range (a)
subpartition by key (a)
( partition x1 values less than (1)
  ( subpartition x11,
    subpartition x12),
   partition x2 values less than (5)
  ( subpartition x21,
    subpartition x22)
);
--sorted_result
--replace_column 16 # 19 # 20 #
--sorted_result
select * from information_schema.partitions where table_schema="test"; 
drop table t1,t2;

create table t1 (
a int not null,
b int not null,
c int not null,
primary key (a,b))
partition by range (a)
subpartition by hash (a+b)
( partition x1 values less than (1)
 ( subpartition x11 nodegroup 0,
   subpartition x12 nodegroup 1),
  partition x2 values less than (5)
( subpartition x21 nodegroup 0,
  subpartition x22 nodegroup 1),
  partition x3 values less than (10)
( subpartition x31 max_rows=50,
  subpartition x32 nodegroup 1)
);
   
--sorted_result
--replace_column 16 # 19 # 20 #
--sorted_result
select * from information_schema.partitions where table_schema="test";
show tables;
drop table t1;

create table t1(f1 int, f2 int);
--sorted_result
--replace_column 16 # 19 # 20 #
select * from information_schema.partitions where table_schema="test";
drop table t1;

create table t1 (f1 date)
partition by linear hash(month(f1))
partitions 3;
--sorted_result
--replace_column 16 # 19 # 20 #
select * from information_schema.partitions where table_schema="test"
and table_name="t1";
drop table t1;

#
# Bug 20161 Partitions: SUBPARTITION METHOD does not show LINEAR keyword
#
create table t1 (a int)
PARTITION BY RANGE (a)
SUBPARTITION BY LINEAR HASH (a)
(PARTITION p0 VALUES LESS THAN (10));

SHOW CREATE TABLE t1;
select SUBPARTITION_METHOD FROM information_schema.partitions WHERE
table_schema="test" AND table_name="t1";
drop table t1;

create table t1 (a int)
PARTITION BY LIST (a)
(PARTITION p0 VALUES IN
(10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,
 32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53));
SHOW CREATE TABLE t1;
SELECT PARTITION_DESCRIPTION FROM information_schema.partitions WHERE
table_schema = "test" AND table_name = "t1";
drop table t1;

#
# Bug#38909 CREATE_OPTIONS in information_schema produces wrong results
#
--disable_warnings
drop table if exists t1;
--enable_warnings
create table t1 (f1 int key) partition by key(f1) partitions 2;
--sorted_result
select create_options from information_schema.tables where table_schema="test";
drop table t1;

--echo # Bug #29870919  INFORMATION SCHEMA STATS EXPIRY RESULTS IN BAD
--echo # STATS FOR PARTITIONED TABLES
--echo #
--echo # Without the fix the cardinality values by SHOW INDEXES is invalid.
--echo #

CREATE TABLE t1
(f1 INT NOT NULL AUTO_INCREMENT,
 f2 INT NOT NULL,
 PRIMARY KEY (f1,f2)) ENGINE=InnoDB
PARTITION BY RANGE (f2)
  (PARTITION p0 VALUES LESS THAN (100),
   PARTITION p1 VALUES LESS THAN (200),
   PARTITION p2 VALUES LESS THAN (300),
   PARTITION p3 VALUES LESS THAN (MAXVALUE));

INSERT INTO t1 VALUES (NULL, 1);
INSERT INTO t1 VALUES (NULL, 2);
INSERT INTO t1 SELECT NULL, 3
                FROM t1 a, t1 b, t1 c, t1 d, t1 e, t1 f, t1 g;

--echo # This SELECT caches the cardinality, as there is non cached yet.
--echo # We expect a non-zero cardinality value, as SE cannot provide
--echo # latest value, without the call to ANALYZE TABLE.
--sorted_result
SELECT TABLE_NAME, NON_UNIQUE, INDEX_SCHEMA INDEX_NAME, SEQ_IN_INDEX,
       COLUMN_NAME, COLLATION, IF(CARDINALITY > 0, "non-zero", "zero"),
       SUB_PART PACKED, NULLABLE, INDEX_TYPE, COMMENT, INDEX_COMMENT,
       IS_VISIBLE, EXPRESSION
  FROM INFORMATION_SCHEMA.STATISTICS
  WHERE TABLE_SCHEMA='test' AND TABLE_NAME='t1';


--echo # We do not account INSERTed rows into cardinality because
--echo # the cached value is not expired yet.
--echo # We expect a non-zero cardinality value, as SE cannot provide
--echo # latest value, without the call to ANALYZE TABLE.
INSERT INTO t1 SELECT NULL, 4 FROM t1;
--sorted_result
SELECT TABLE_NAME, NON_UNIQUE, INDEX_SCHEMA INDEX_NAME, SEQ_IN_INDEX,
       COLUMN_NAME, COLLATION, IF(CARDINALITY > 0, "non-zero", "zero"),
       SUB_PART PACKED, NULLABLE, INDEX_TYPE, COMMENT, INDEX_COMMENT,
       IS_VISIBLE, EXPRESSION
  FROM INFORMATION_SCHEMA.STATISTICS
  WHERE TABLE_SCHEMA='test' AND TABLE_NAME='t1';

--echo # We force update cache by calling ANALYZE TABLE.
ANALYZE TABLE t1;
SHOW INDEXES IN t1;

--echo # We do see INSERTed rows accounted into cardinality
--echo # because we ignore cache with expiry seconds set to 0.
SET session information_schema_stats_expiry = 0;
INSERT INTO t1 SELECT NULL, 4 FROM t1;
SHOW INDEXES IN t1;

DROP TABLE t1;
