--source include/force_myisam_default.inc
--source include/have_myisam.inc

--echo # 
--echo # WL#8971: Deprecate and remove partitioning storage engine
--echo # 
--echo # Tests to verify that partition related SQL statements
--echo # for engines not supporting native partitioning return
--echo # an error.
--echo # 


--echo # 
--echo # Test recursively for various storage engines.
--echo # 

if ($ENGINE == "")
{
  --let $ENGINE=MyISAM
  --source partition_not_supported.test
  exit;
}

--echo # 
--echo # 1. Create partitioned table.
--echo # 

--error ER_CHECK_NOT_IMPLEMENTED, ER_PARTITION_MERGE_ERROR
eval CREATE TABLE t1 (i INTEGER)
  ENGINE $ENGINE
  PARTITION BY HASH (i) PARTITIONS 2;

--echo # 
--echo # 2. Create subpartitioned table.
--echo # 

--error ER_CHECK_NOT_IMPLEMENTED, ER_PARTITION_MERGE_ERROR
eval CREATE TABLE t1 (i INTEGER)
  ENGINE $ENGINE
  PARTITION BY LIST (i) SUBPARTITION BY HASH (i)
  (PARTITION p0 VALUES IN (0)
   (SUBPARTITION s0,
    SUBPARTITION s1,
    SUBPARTITION s2),
  PARTITION p1 VALUES IN (1)
   (SUBPARTITION s3,
    SUBPARTITION s4,
    SUBPARTITION s5));

--echo # 
--echo # 3. Alter a table to have partitions.
--echo # 

eval CREATE TABLE t1 (i INTEGER NOT NULL)
  ENGINE $ENGINE;

--error ER_CHECK_NOT_IMPLEMENTED, ER_PARTITION_MERGE_ERROR
ALTER TABLE t1
  PARTITION BY HASH (i) PARTITIONS 2;

DROP TABLE t1;

--echo # 
--echo # 4. Alter a table to have subpartitions.
--echo # 

eval CREATE TABLE t1 (i INTEGER NOT NULL)
  ENGINE $ENGINE;

--error ER_CHECK_NOT_IMPLEMENTED, ER_PARTITION_MERGE_ERROR
ALTER TABLE t1
  PARTITION BY RANGE (i) SUBPARTITION BY HASH (i)
  (PARTITION p0 VALUES LESS THAN (50)
   (SUBPARTITION s0,
    SUBPARTITION s1),
  PARTITION p1 VALUES LESS THAN (100)
   (SUBPARTITION s2,
    SUBPARTITION s3));

DROP TABLE t1;

--echo # 
--echo # 5. Alter engine from an SE supporting native partitioning.
--echo # 

CREATE TABLE t1 (i INTEGER NOT NULL)
  ENGINE InnoDB;

--error ER_CHECK_NOT_IMPLEMENTED, ER_PARTITION_MERGE_ERROR
eval ALTER TABLE t1 ENGINE $ENGINE
  PARTITION BY HASH (i) PARTITIONS 2;

ALTER TABLE t1
  PARTITION BY HASH (i) PARTITIONS 2;

SHOW CREATE TABLE t1;

--error ER_CHECK_NOT_IMPLEMENTED, ER_PARTITION_MERGE_ERROR
eval ALTER TABLE t1 ENGINE $ENGINE;

DROP TABLE t1;
