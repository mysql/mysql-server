# 
# WL#8971: Deprecate and remove partitioning storage engine
# 
# Tests to verify that partition related SQL statements
# for engines not supporting native partitioning return
# an error.
# 
# 
# Test recursively for various storage engines.
# 
# 
# WL#8971: Deprecate and remove partitioning storage engine
# 
# Tests to verify that partition related SQL statements
# for engines not supporting native partitioning return
# an error.
# 
# 
# Test recursively for various storage engines.
# 
# 
# 1. Create partitioned table.
# 
CREATE TABLE t1 (i INTEGER)
ENGINE MyISAM
PARTITION BY HASH (i) PARTITIONS 2;
Got one of the listed errors
# 
# 2. Create subpartitioned table.
# 
CREATE TABLE t1 (i INTEGER)
ENGINE MyISAM
PARTITION BY LIST (i) SUBPARTITION BY HASH (i)
(PARTITION p0 VALUES IN (0)
(SUBPARTITION s0,
SUBPARTITION s1,
SUBPARTITION s2),
PARTITION p1 VALUES IN (1)
(SUBPARTITION s3,
SUBPARTITION s4,
SUBPARTITION s5));
Got one of the listed errors
# 
# 3. Alter a table to have partitions.
# 
CREATE TABLE t1 (i INTEGER NOT NULL)
ENGINE MyISAM;
ALTER TABLE t1
PARTITION BY HASH (i) PARTITIONS 2;
Got one of the listed errors
DROP TABLE t1;
# 
# 4. Alter a table to have subpartitions.
# 
CREATE TABLE t1 (i INTEGER NOT NULL)
ENGINE MyISAM;
ALTER TABLE t1
PARTITION BY RANGE (i) SUBPARTITION BY HASH (i)
(PARTITION p0 VALUES LESS THAN (50)
(SUBPARTITION s0,
SUBPARTITION s1),
PARTITION p1 VALUES LESS THAN (100)
(SUBPARTITION s2,
SUBPARTITION s3));
Got one of the listed errors
DROP TABLE t1;
# 
# 5. Alter engine from an SE supporting native partitioning.
# 
CREATE TABLE t1 (i INTEGER NOT NULL)
ENGINE InnoDB;
ALTER TABLE t1 ENGINE MyISAM
PARTITION BY HASH (i) PARTITIONS 2;
Got one of the listed errors
ALTER TABLE t1
PARTITION BY HASH (i) PARTITIONS 2;
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `i` int NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
/*!50100 PARTITION BY HASH (`i`)
PARTITIONS 2 */
ALTER TABLE t1 ENGINE MyISAM;
Got one of the listed errors
DROP TABLE t1;
