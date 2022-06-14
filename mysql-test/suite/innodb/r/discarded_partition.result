# Testing discarded partitions without restart
CREATE DATABASE partitions;
CREATE TABLE partitions.export1(c1 INT,c2 INT) PARTITION BY KEY(c1) PARTITIONS 4;
FLUSH TABLE partitions.export1 FOR EXPORT;
UNLOCK TABLES;
CREATE TABLE partitions.t1(c1 INT,c2 INT) PARTITION BY KEY(c1) PARTITIONS 4;
ALTER TABLE partitions.t1 DISCARD PARTITION p0 TABLESPACE;
CREATE TABLE partitions.t2(c1 INT,c2 INT) PARTITION BY KEY(c1) PARTITIONS 4;
ALTER TABLE partitions.t2 DISCARD PARTITION p0, p1 TABLESPACE;
ALTER TABLE partitions.t2 IMPORT PARTITION p0 TABLESPACE;
ALTER TABLE partitions.t2 IMPORT PARTITION p1 TABLESPACE;
CREATE TABLE partitions.t3(c1 INT,c2 INT) PARTITION BY KEY(c1) PARTITIONS 4;
ALTER TABLE partitions.t3 DISCARD PARTITION p0, p1 TABLESPACE;
ALTER TABLE partitions.t3 IMPORT PARTITION p1 TABLESPACE;
CREATE TABLE partitions.export2 (c1 INT, c2 INT)
PARTITION BY RANGE( c2)
SUBPARTITION BY HASH( c2 )
SUBPARTITIONS 2 (
PARTITION p0 VALUES LESS THAN (10),
PARTITION p1 VALUES LESS THAN (20),
PARTITION p2 VALUES LESS THAN (30),
PARTITION p3 VALUES LESS THAN MAXVALUE
);
FLUSH TABLE partitions.export2 FOR EXPORT;
UNLOCK TABLES;
CREATE TABLE partitions.t4 (c1 INT, c2 INT)
PARTITION BY RANGE( c2 )
SUBPARTITION BY HASH( c2 )
SUBPARTITIONS 2 (
PARTITION p0 VALUES LESS THAN (10),
PARTITION p1 VALUES LESS THAN (20),
PARTITION p2 VALUES LESS THAN (30),
PARTITION p3 VALUES LESS THAN MAXVALUE
);
ALTER TABLE partitions.t4 DISCARD PARTITION p0sp0 TABLESPACE;
ALTER TABLE partitions.t4 DISCARD PARTITION p0sp1 TABLESPACE;
ALTER TABLE partitions.t4 DISCARD PARTITION p1sp0 TABLESPACE;
CREATE TABLE partitions.t5 (c1 INT, c2 INT)
PARTITION BY RANGE( c2 )
SUBPARTITION BY HASH( c2 )
SUBPARTITIONS 2 (
PARTITION p0 VALUES LESS THAN (10),
PARTITION p1 VALUES LESS THAN (20),
PARTITION p2 VALUES LESS THAN (30),
PARTITION p3 VALUES LESS THAN MAXVALUE
);
ALTER TABLE partitions.t5 DISCARD PARTITION p0sp0 TABLESPACE;
ALTER TABLE partitions.t5 DISCARD PARTITION p0sp1 TABLESPACE;
ALTER TABLE partitions.t5 DISCARD PARTITION p1sp0 TABLESPACE;
ALTER TABLE partitions.t5 DISCARD PARTITION p1sp1 TABLESPACE;
ALTER TABLE partitions.t5 DISCARD PARTITION p2sp0 TABLESPACE;
ALTER TABLE partitions.t5 IMPORT PARTITION p0sp0 TABLESPACE;
ALTER TABLE partitions.t5 IMPORT PARTITION p0sp1 TABLESPACE;
ALTER TABLE partitions.t5 IMPORT PARTITION p1sp0 TABLESPACE;
ALTER TABLE partitions.t5 IMPORT PARTITION p2sp0 TABLESPACE;
CREATE TABLE partitions.t6 (c1 INT, c2 INT)
PARTITION BY RANGE( c2 )
SUBPARTITION BY HASH( c2 )
SUBPARTITIONS 2 (
PARTITION p0 VALUES LESS THAN (10),
PARTITION p1 VALUES LESS THAN (20),
PARTITION p2 VALUES LESS THAN (30),
PARTITION p3 VALUES LESS THAN MAXVALUE
);
ALTER TABLE partitions.t6 DISCARD PARTITION p0 TABLESPACE;
ALTER TABLE partitions.t6 DISCARD PARTITION p1sp1 TABLESPACE;
CREATE TABLE partitions.t7 (c1 INT, c2 INT)
PARTITION BY RANGE( c2 )
SUBPARTITION BY HASH( c2 )
SUBPARTITIONS 2 (
PARTITION p0 VALUES LESS THAN (10),
PARTITION p1 VALUES LESS THAN (20),
PARTITION p2 VALUES LESS THAN (30),
PARTITION p3 VALUES LESS THAN MAXVALUE
);
ALTER TABLE partitions.t7 DISCARD PARTITION p0 TABLESPACE;
ALTER TABLE partitions.t7 DISCARD PARTITION p1sp0 TABLESPACE;
ALTER TABLE partitions.t7 IMPORT PARTITION p0sp0 TABLESPACE;
ALTER TABLE partitions.t7 IMPORT PARTITION p0sp1 TABLESPACE;
ALTER TABLE partitions.t7 IMPORT PARTITION p1sp0 TABLESPACE;
SELECT * FROM partitions.t1;
ERROR HY000: Tablespace has been discarded for table 't1'
SELECT * FROM partitions.t1 PARTITION (p0);
ERROR HY000: Tablespace has been discarded for table 't1'
SELECT * FROM partitions.t1 PARTITION (p1);
c1	c2
SELECT * FROM partitions.t1 PARTITION (p2);
c1	c2
SELECT * FROM partitions.t1 PARTITION (p3);
c1	c2
SELECT * FROM partitions.t2;
c1	c2
SELECT * FROM partitions.t2 PARTITION (p0);
c1	c2
SELECT * FROM partitions.t2 PARTITION (p1);
c1	c2
SELECT * FROM partitions.t2 PARTITION (p2);
c1	c2
SELECT * FROM partitions.t2 PARTITION (p3);
c1	c2
SELECT * FROM partitions.t3;
ERROR HY000: Tablespace has been discarded for table 't3'
SELECT * FROM partitions.t3 PARTITION (p0);
ERROR HY000: Tablespace has been discarded for table 't3'
SELECT * FROM partitions.t3 PARTITION (p1);
c1	c2
SELECT * FROM partitions.t3 PARTITION (p2);
c1	c2
SELECT * FROM partitions.t3 PARTITION (p3);
c1	c2
SELECT * FROM partitions.t4;
ERROR HY000: Tablespace has been discarded for table 't4'
SELECT * FROM partitions.t4 PARTITION (p0);
ERROR HY000: Tablespace has been discarded for table 't4'
SELECT * FROM partitions.t4 PARTITION (p0sp0);
ERROR HY000: Tablespace has been discarded for table 't4'
SELECT * FROM partitions.t4 PARTITION (p0sp1);
ERROR HY000: Tablespace has been discarded for table 't4'
SELECT * FROM partitions.t4 PARTITION (p1);
ERROR HY000: Tablespace has been discarded for table 't4'
SELECT * FROM partitions.t4 PARTITION (p1sp0);
ERROR HY000: Tablespace has been discarded for table 't4'
SELECT * FROM partitions.t4 PARTITION (p1sp1);
c1	c2
SELECT * FROM partitions.t4 PARTITION (p2);
c1	c2
SELECT * FROM partitions.t4 PARTITION (p2sp0);
c1	c2
SELECT * FROM partitions.t4 PARTITION (p2sp1);
c1	c2
SELECT * FROM partitions.t5;
ERROR HY000: Tablespace has been discarded for table 't5'
SELECT * FROM partitions.t5 PARTITION (p0);
c1	c2
SELECT * FROM partitions.t5 PARTITION (p0sp0);
c1	c2
SELECT * FROM partitions.t5 PARTITION (p0sp1);
c1	c2
SELECT * FROM partitions.t5 PARTITION (p1);
ERROR HY000: Tablespace has been discarded for table 't5'
SELECT * FROM partitions.t5 PARTITION (p1sp0);
c1	c2
SELECT * FROM partitions.t5 PARTITION (p1sp1);
ERROR HY000: Tablespace has been discarded for table 't5'
SELECT * FROM partitions.t5 PARTITION (p2);
c1	c2
SELECT * FROM partitions.t5 PARTITION (p2sp0);
c1	c2
SELECT * FROM partitions.t5 PARTITION (p2sp1);
c1	c2
SELECT * FROM partitions.t6;
ERROR HY000: Tablespace has been discarded for table 't6'
SELECT * FROM partitions.t6 PARTITION (p0);
ERROR HY000: Tablespace has been discarded for table 't6'
SELECT * FROM partitions.t6 PARTITION (p0sp0);
ERROR HY000: Tablespace has been discarded for table 't6'
SELECT * FROM partitions.t6 PARTITION (p0sp1);
ERROR HY000: Tablespace has been discarded for table 't6'
SELECT * FROM partitions.t6 PARTITION (p1);
ERROR HY000: Tablespace has been discarded for table 't6'
SELECT * FROM partitions.t6 PARTITION (p1sp0);
c1	c2
SELECT * FROM partitions.t6 PARTITION (p1sp1);
ERROR HY000: Tablespace has been discarded for table 't6'
SELECT * FROM partitions.t6 PARTITION (p2);
c1	c2
SELECT * FROM partitions.t6 PARTITION (p2sp0);
c1	c2
SELECT * FROM partitions.t6 PARTITION (p2sp1);
c1	c2
SELECT * FROM partitions.t7;
c1	c2
SELECT * FROM partitions.t7 PARTITION (p0);
c1	c2
SELECT * FROM partitions.t7 PARTITION (p0sp0);
c1	c2
SELECT * FROM partitions.t7 PARTITION (p0sp1);
c1	c2
SELECT * FROM partitions.t7 PARTITION (p1);
c1	c2
SELECT * FROM partitions.t7 PARTITION (p1sp0);
c1	c2
SELECT * FROM partitions.t7 PARTITION (p1sp1);
c1	c2
SELECT * FROM partitions.t7 PARTITION (p2);
c1	c2
SELECT * FROM partitions.t7 PARTITION (p2sp0);
c1	c2
SELECT * FROM partitions.t7 PARTITION (p2sp1);
c1	c2
ALTER TABLE partitions.t1 REBUILD PARTITION p0;
ERROR HY000: Tablespace has been discarded for table 't1'
ALTER TABLE partitions.t1 REBUILD PARTITION p1;
Warnings:
Warning	1814	InnoDB: Tablespace has been discarded for table 't1'
Warning	1814	InnoDB: Tablespace has been discarded for table 't1'
ALTER TABLE partitions.t1 REBUILD PARTITION p2;
Warnings:
Warning	1814	InnoDB: Tablespace has been discarded for table 't1'
ALTER TABLE partitions.t1 REBUILD PARTITION p3;
Warnings:
Warning	1814	InnoDB: Tablespace has been discarded for table 't1'
ALTER TABLE partitions.t2 REBUILD PARTITION p0;
ALTER TABLE partitions.t2 REBUILD PARTITION p1;
ALTER TABLE partitions.t2 REBUILD PARTITION p2;
ALTER TABLE partitions.t2 REBUILD PARTITION p3;
ALTER TABLE partitions.t3 REBUILD PARTITION p0;
ERROR HY000: Tablespace has been discarded for table 't3'
ALTER TABLE partitions.t3 REBUILD PARTITION p1;
Warnings:
Warning	1814	InnoDB: Tablespace has been discarded for table 't3'
Warning	1814	InnoDB: Tablespace has been discarded for table 't3'
ALTER TABLE partitions.t3 REBUILD PARTITION p2;
Warnings:
Warning	1814	InnoDB: Tablespace has been discarded for table 't3'
ALTER TABLE partitions.t3 REBUILD PARTITION p3;
Warnings:
Warning	1814	InnoDB: Tablespace has been discarded for table 't3'
ALTER TABLE partitions.t4 REBUILD PARTITION p0;
ERROR HY000: Tablespace has been discarded for table 't4'
ALTER TABLE partitions.t4 REBUILD PARTITION p1;
ERROR HY000: Tablespace has been discarded for table 't4'
ALTER TABLE partitions.t4 REBUILD PARTITION p2;
Warnings:
Warning	1814	InnoDB: Tablespace has been discarded for table 't4'
Warning	1814	InnoDB: Tablespace has been discarded for table 't4'
Warning	1814	InnoDB: Tablespace has been discarded for table 't4'
Warning	1814	InnoDB: Tablespace has been discarded for table 't4'
Warning	1814	InnoDB: Tablespace has been discarded for table 't4'
Warning	1814	InnoDB: Tablespace has been discarded for table 't4'
ALTER TABLE partitions.t5 REBUILD PARTITION p0;
Warnings:
Warning	1814	InnoDB: Tablespace has been discarded for table 't5'
ALTER TABLE partitions.t5 REBUILD PARTITION p1;
ERROR HY000: Tablespace has been discarded for table 't5'
ALTER TABLE partitions.t5 REBUILD PARTITION p2;
Warnings:
Warning	1814	InnoDB: Tablespace has been discarded for table 't5'
Warning	1814	InnoDB: Tablespace has been discarded for table 't5'
ALTER TABLE partitions.t6 REBUILD PARTITION p0;
ERROR HY000: Tablespace has been discarded for table 't6'
ALTER TABLE partitions.t6 REBUILD PARTITION p1;
ERROR HY000: Tablespace has been discarded for table 't6'
ALTER TABLE partitions.t6 REBUILD PARTITION p2;
Warnings:
Warning	1814	InnoDB: Tablespace has been discarded for table 't6'
Warning	1814	InnoDB: Tablespace has been discarded for table 't6'
Warning	1814	InnoDB: Tablespace has been discarded for table 't6'
Warning	1814	InnoDB: Tablespace has been discarded for table 't6'
Warning	1814	InnoDB: Tablespace has been discarded for table 't6'
Warning	1814	InnoDB: Tablespace has been discarded for table 't6'
ALTER TABLE partitions.t7 REBUILD PARTITION p0;
ALTER TABLE partitions.t7 REBUILD PARTITION p1;
ALTER TABLE partitions.t7 REBUILD PARTITION p2;
DROP TABLE partitions.t1;
DROP TABLE partitions.t2;
DROP TABLE partitions.t3;
DROP TABLE partitions.t4;
DROP TABLE partitions.t5;
DROP TABLE partitions.t6;
DROP TABLE partitions.t7;
DROP DATABASE partitions;
# Testing discarded partitions with restart
CREATE DATABASE partitions;
CREATE TABLE partitions.export1(c1 INT,c2 INT) PARTITION BY KEY(c1) PARTITIONS 4;
FLUSH TABLE partitions.export1 FOR EXPORT;
UNLOCK TABLES;
CREATE TABLE partitions.t1(c1 INT,c2 INT) PARTITION BY KEY(c1) PARTITIONS 4;
ALTER TABLE partitions.t1 DISCARD PARTITION p0 TABLESPACE;
CREATE TABLE partitions.t2(c1 INT,c2 INT) PARTITION BY KEY(c1) PARTITIONS 4;
ALTER TABLE partitions.t2 DISCARD PARTITION p0, p1 TABLESPACE;
ALTER TABLE partitions.t2 IMPORT PARTITION p0 TABLESPACE;
ALTER TABLE partitions.t2 IMPORT PARTITION p1 TABLESPACE;
CREATE TABLE partitions.t3(c1 INT,c2 INT) PARTITION BY KEY(c1) PARTITIONS 4;
ALTER TABLE partitions.t3 DISCARD PARTITION p0, p1 TABLESPACE;
ALTER TABLE partitions.t3 IMPORT PARTITION p1 TABLESPACE;
CREATE TABLE partitions.export2 (c1 INT, c2 INT)
PARTITION BY RANGE( c2)
SUBPARTITION BY HASH( c2 )
SUBPARTITIONS 2 (
PARTITION p0 VALUES LESS THAN (10),
PARTITION p1 VALUES LESS THAN (20),
PARTITION p2 VALUES LESS THAN (30),
PARTITION p3 VALUES LESS THAN MAXVALUE
);
FLUSH TABLE partitions.export2 FOR EXPORT;
UNLOCK TABLES;
CREATE TABLE partitions.t4 (c1 INT, c2 INT)
PARTITION BY RANGE( c2 )
SUBPARTITION BY HASH( c2 )
SUBPARTITIONS 2 (
PARTITION p0 VALUES LESS THAN (10),
PARTITION p1 VALUES LESS THAN (20),
PARTITION p2 VALUES LESS THAN (30),
PARTITION p3 VALUES LESS THAN MAXVALUE
);
ALTER TABLE partitions.t4 DISCARD PARTITION p0sp0 TABLESPACE;
ALTER TABLE partitions.t4 DISCARD PARTITION p0sp1 TABLESPACE;
ALTER TABLE partitions.t4 DISCARD PARTITION p1sp0 TABLESPACE;
CREATE TABLE partitions.t5 (c1 INT, c2 INT)
PARTITION BY RANGE( c2 )
SUBPARTITION BY HASH( c2 )
SUBPARTITIONS 2 (
PARTITION p0 VALUES LESS THAN (10),
PARTITION p1 VALUES LESS THAN (20),
PARTITION p2 VALUES LESS THAN (30),
PARTITION p3 VALUES LESS THAN MAXVALUE
);
ALTER TABLE partitions.t5 DISCARD PARTITION p0sp0 TABLESPACE;
ALTER TABLE partitions.t5 DISCARD PARTITION p0sp1 TABLESPACE;
ALTER TABLE partitions.t5 DISCARD PARTITION p1sp0 TABLESPACE;
ALTER TABLE partitions.t5 DISCARD PARTITION p1sp1 TABLESPACE;
ALTER TABLE partitions.t5 DISCARD PARTITION p2sp0 TABLESPACE;
ALTER TABLE partitions.t5 IMPORT PARTITION p0sp0 TABLESPACE;
ALTER TABLE partitions.t5 IMPORT PARTITION p0sp1 TABLESPACE;
ALTER TABLE partitions.t5 IMPORT PARTITION p1sp0 TABLESPACE;
ALTER TABLE partitions.t5 IMPORT PARTITION p2sp0 TABLESPACE;
CREATE TABLE partitions.t6 (c1 INT, c2 INT)
PARTITION BY RANGE( c2 )
SUBPARTITION BY HASH( c2 )
SUBPARTITIONS 2 (
PARTITION p0 VALUES LESS THAN (10),
PARTITION p1 VALUES LESS THAN (20),
PARTITION p2 VALUES LESS THAN (30),
PARTITION p3 VALUES LESS THAN MAXVALUE
);
ALTER TABLE partitions.t6 DISCARD PARTITION p0 TABLESPACE;
ALTER TABLE partitions.t6 DISCARD PARTITION p1sp1 TABLESPACE;
CREATE TABLE partitions.t7 (c1 INT, c2 INT)
PARTITION BY RANGE( c2 )
SUBPARTITION BY HASH( c2 )
SUBPARTITIONS 2 (
PARTITION p0 VALUES LESS THAN (10),
PARTITION p1 VALUES LESS THAN (20),
PARTITION p2 VALUES LESS THAN (30),
PARTITION p3 VALUES LESS THAN MAXVALUE
);
ALTER TABLE partitions.t7 DISCARD PARTITION p0 TABLESPACE;
ALTER TABLE partitions.t7 DISCARD PARTITION p1sp0 TABLESPACE;
ALTER TABLE partitions.t7 IMPORT PARTITION p0sp0 TABLESPACE;
ALTER TABLE partitions.t7 IMPORT PARTITION p0sp1 TABLESPACE;
ALTER TABLE partitions.t7 IMPORT PARTITION p1sp0 TABLESPACE;
# restart
SELECT * FROM partitions.t1;
ERROR HY000: Tablespace has been discarded for table 't1'
SELECT * FROM partitions.t1 PARTITION (p0);
ERROR HY000: Tablespace has been discarded for table 't1'
SELECT * FROM partitions.t1 PARTITION (p1);
c1	c2
SELECT * FROM partitions.t1 PARTITION (p2);
c1	c2
SELECT * FROM partitions.t1 PARTITION (p3);
c1	c2
SELECT * FROM partitions.t2;
c1	c2
SELECT * FROM partitions.t2 PARTITION (p0);
c1	c2
SELECT * FROM partitions.t2 PARTITION (p1);
c1	c2
SELECT * FROM partitions.t2 PARTITION (p2);
c1	c2
SELECT * FROM partitions.t2 PARTITION (p3);
c1	c2
SELECT * FROM partitions.t3;
ERROR HY000: Tablespace has been discarded for table 't3'
SELECT * FROM partitions.t3 PARTITION (p0);
ERROR HY000: Tablespace has been discarded for table 't3'
SELECT * FROM partitions.t3 PARTITION (p1);
c1	c2
SELECT * FROM partitions.t3 PARTITION (p2);
c1	c2
SELECT * FROM partitions.t3 PARTITION (p3);
c1	c2
SELECT * FROM partitions.t4;
ERROR HY000: Tablespace has been discarded for table 't4'
SELECT * FROM partitions.t4 PARTITION (p0);
ERROR HY000: Tablespace has been discarded for table 't4'
SELECT * FROM partitions.t4 PARTITION (p0sp0);
ERROR HY000: Tablespace has been discarded for table 't4'
SELECT * FROM partitions.t4 PARTITION (p0sp1);
ERROR HY000: Tablespace has been discarded for table 't4'
SELECT * FROM partitions.t4 PARTITION (p1);
ERROR HY000: Tablespace has been discarded for table 't4'
SELECT * FROM partitions.t4 PARTITION (p1sp0);
ERROR HY000: Tablespace has been discarded for table 't4'
SELECT * FROM partitions.t4 PARTITION (p1sp1);
c1	c2
SELECT * FROM partitions.t4 PARTITION (p2);
c1	c2
SELECT * FROM partitions.t4 PARTITION (p2sp0);
c1	c2
SELECT * FROM partitions.t4 PARTITION (p2sp1);
c1	c2
SELECT * FROM partitions.t5;
ERROR HY000: Tablespace has been discarded for table 't5'
SELECT * FROM partitions.t5 PARTITION (p0);
c1	c2
SELECT * FROM partitions.t5 PARTITION (p0sp0);
c1	c2
SELECT * FROM partitions.t5 PARTITION (p0sp1);
c1	c2
SELECT * FROM partitions.t5 PARTITION (p1);
ERROR HY000: Tablespace has been discarded for table 't5'
SELECT * FROM partitions.t5 PARTITION (p1sp0);
c1	c2
SELECT * FROM partitions.t5 PARTITION (p1sp1);
ERROR HY000: Tablespace has been discarded for table 't5'
SELECT * FROM partitions.t5 PARTITION (p2);
c1	c2
SELECT * FROM partitions.t5 PARTITION (p2sp0);
c1	c2
SELECT * FROM partitions.t5 PARTITION (p2sp1);
c1	c2
SELECT * FROM partitions.t6;
ERROR HY000: Tablespace has been discarded for table 't6'
SELECT * FROM partitions.t6 PARTITION (p0);
ERROR HY000: Tablespace has been discarded for table 't6'
SELECT * FROM partitions.t6 PARTITION (p0sp0);
ERROR HY000: Tablespace has been discarded for table 't6'
SELECT * FROM partitions.t6 PARTITION (p0sp1);
ERROR HY000: Tablespace has been discarded for table 't6'
SELECT * FROM partitions.t6 PARTITION (p1);
ERROR HY000: Tablespace has been discarded for table 't6'
SELECT * FROM partitions.t6 PARTITION (p1sp0);
c1	c2
SELECT * FROM partitions.t6 PARTITION (p1sp1);
ERROR HY000: Tablespace has been discarded for table 't6'
SELECT * FROM partitions.t6 PARTITION (p2);
c1	c2
SELECT * FROM partitions.t6 PARTITION (p2sp0);
c1	c2
SELECT * FROM partitions.t6 PARTITION (p2sp1);
c1	c2
SELECT * FROM partitions.t7;
c1	c2
SELECT * FROM partitions.t7 PARTITION (p0);
c1	c2
SELECT * FROM partitions.t7 PARTITION (p0sp0);
c1	c2
SELECT * FROM partitions.t7 PARTITION (p0sp1);
c1	c2
SELECT * FROM partitions.t7 PARTITION (p1);
c1	c2
SELECT * FROM partitions.t7 PARTITION (p1sp0);
c1	c2
SELECT * FROM partitions.t7 PARTITION (p1sp1);
c1	c2
SELECT * FROM partitions.t7 PARTITION (p2);
c1	c2
SELECT * FROM partitions.t7 PARTITION (p2sp0);
c1	c2
SELECT * FROM partitions.t7 PARTITION (p2sp1);
c1	c2
ALTER TABLE partitions.t1 REBUILD PARTITION p0;
ERROR HY000: Tablespace has been discarded for table 't1'
ALTER TABLE partitions.t1 REBUILD PARTITION p1;
Warnings:
Warning	1814	InnoDB: Tablespace has been discarded for table 't1'
Warning	1814	InnoDB: Tablespace has been discarded for table 't1'
ALTER TABLE partitions.t1 REBUILD PARTITION p2;
Warnings:
Warning	1814	InnoDB: Tablespace has been discarded for table 't1'
ALTER TABLE partitions.t1 REBUILD PARTITION p3;
Warnings:
Warning	1814	InnoDB: Tablespace has been discarded for table 't1'
ALTER TABLE partitions.t2 REBUILD PARTITION p0;
ALTER TABLE partitions.t2 REBUILD PARTITION p1;
ALTER TABLE partitions.t2 REBUILD PARTITION p2;
ALTER TABLE partitions.t2 REBUILD PARTITION p3;
ALTER TABLE partitions.t3 REBUILD PARTITION p0;
ERROR HY000: Tablespace has been discarded for table 't3'
ALTER TABLE partitions.t3 REBUILD PARTITION p1;
Warnings:
Warning	1814	InnoDB: Tablespace has been discarded for table 't3'
Warning	1814	InnoDB: Tablespace has been discarded for table 't3'
ALTER TABLE partitions.t3 REBUILD PARTITION p2;
Warnings:
Warning	1814	InnoDB: Tablespace has been discarded for table 't3'
ALTER TABLE partitions.t3 REBUILD PARTITION p3;
Warnings:
Warning	1814	InnoDB: Tablespace has been discarded for table 't3'
ALTER TABLE partitions.t4 REBUILD PARTITION p0;
ERROR HY000: Tablespace has been discarded for table 't4'
ALTER TABLE partitions.t4 REBUILD PARTITION p1;
ERROR HY000: Tablespace has been discarded for table 't4'
ALTER TABLE partitions.t4 REBUILD PARTITION p2;
Warnings:
Warning	1814	InnoDB: Tablespace has been discarded for table 't4'
Warning	1814	InnoDB: Tablespace has been discarded for table 't4'
Warning	1814	InnoDB: Tablespace has been discarded for table 't4'
Warning	1814	InnoDB: Tablespace has been discarded for table 't4'
Warning	1814	InnoDB: Tablespace has been discarded for table 't4'
Warning	1814	InnoDB: Tablespace has been discarded for table 't4'
ALTER TABLE partitions.t5 REBUILD PARTITION p0;
Warnings:
Warning	1814	InnoDB: Tablespace has been discarded for table 't5'
ALTER TABLE partitions.t5 REBUILD PARTITION p1;
ERROR HY000: Tablespace has been discarded for table 't5'
ALTER TABLE partitions.t5 REBUILD PARTITION p2;
Warnings:
Warning	1814	InnoDB: Tablespace has been discarded for table 't5'
Warning	1814	InnoDB: Tablespace has been discarded for table 't5'
ALTER TABLE partitions.t6 REBUILD PARTITION p0;
ERROR HY000: Tablespace has been discarded for table 't6'
ALTER TABLE partitions.t6 REBUILD PARTITION p1;
ERROR HY000: Tablespace has been discarded for table 't6'
ALTER TABLE partitions.t6 REBUILD PARTITION p2;
Warnings:
Warning	1814	InnoDB: Tablespace has been discarded for table 't6'
Warning	1814	InnoDB: Tablespace has been discarded for table 't6'
Warning	1814	InnoDB: Tablespace has been discarded for table 't6'
Warning	1814	InnoDB: Tablespace has been discarded for table 't6'
Warning	1814	InnoDB: Tablespace has been discarded for table 't6'
Warning	1814	InnoDB: Tablespace has been discarded for table 't6'
ALTER TABLE partitions.t7 REBUILD PARTITION p0;
ALTER TABLE partitions.t7 REBUILD PARTITION p1;
ALTER TABLE partitions.t7 REBUILD PARTITION p2;
DROP TABLE partitions.t1;
DROP TABLE partitions.t2;
DROP TABLE partitions.t3;
DROP TABLE partitions.t4;
DROP TABLE partitions.t5;
DROP TABLE partitions.t6;
DROP TABLE partitions.t7;
DROP DATABASE partitions;
# Test shutdown just after rebuild partition
CREATE DATABASE partitions;
CREATE TABLE partitions.export1(c1 INT,c2 INT) PARTITION BY KEY(c1) PARTITIONS 4;
FLUSH TABLE partitions.export1 FOR EXPORT;
UNLOCK TABLES;
CREATE TABLE partitions.t1(c1 INT,c2 INT) PARTITION BY KEY(c1) PARTITIONS 4;
ALTER TABLE partitions.t1 DISCARD PARTITION p0 TABLESPACE;
CREATE TABLE partitions.t2(c1 INT,c2 INT) PARTITION BY KEY(c1) PARTITIONS 4;
ALTER TABLE partitions.t2 DISCARD PARTITION p0, p1 TABLESPACE;
ALTER TABLE partitions.t2 IMPORT PARTITION p0 TABLESPACE;
ALTER TABLE partitions.t2 IMPORT PARTITION p1 TABLESPACE;
CREATE TABLE partitions.t3(c1 INT,c2 INT) PARTITION BY KEY(c1) PARTITIONS 4;
ALTER TABLE partitions.t3 DISCARD PARTITION p0, p1 TABLESPACE;
ALTER TABLE partitions.t3 IMPORT PARTITION p1 TABLESPACE;
CREATE TABLE partitions.export2 (c1 INT, c2 INT)
PARTITION BY RANGE( c2)
SUBPARTITION BY HASH( c2 )
SUBPARTITIONS 2 (
PARTITION p0 VALUES LESS THAN (10),
PARTITION p1 VALUES LESS THAN (20),
PARTITION p2 VALUES LESS THAN (30),
PARTITION p3 VALUES LESS THAN MAXVALUE
);
FLUSH TABLE partitions.export2 FOR EXPORT;
UNLOCK TABLES;
CREATE TABLE partitions.t4 (c1 INT, c2 INT)
PARTITION BY RANGE( c2 )
SUBPARTITION BY HASH( c2 )
SUBPARTITIONS 2 (
PARTITION p0 VALUES LESS THAN (10),
PARTITION p1 VALUES LESS THAN (20),
PARTITION p2 VALUES LESS THAN (30),
PARTITION p3 VALUES LESS THAN MAXVALUE
);
ALTER TABLE partitions.t4 DISCARD PARTITION p0sp0 TABLESPACE;
ALTER TABLE partitions.t4 DISCARD PARTITION p0sp1 TABLESPACE;
ALTER TABLE partitions.t4 DISCARD PARTITION p1sp0 TABLESPACE;
CREATE TABLE partitions.t5 (c1 INT, c2 INT)
PARTITION BY RANGE( c2 )
SUBPARTITION BY HASH( c2 )
SUBPARTITIONS 2 (
PARTITION p0 VALUES LESS THAN (10),
PARTITION p1 VALUES LESS THAN (20),
PARTITION p2 VALUES LESS THAN (30),
PARTITION p3 VALUES LESS THAN MAXVALUE
);
ALTER TABLE partitions.t5 DISCARD PARTITION p0sp0 TABLESPACE;
ALTER TABLE partitions.t5 DISCARD PARTITION p0sp1 TABLESPACE;
ALTER TABLE partitions.t5 DISCARD PARTITION p1sp0 TABLESPACE;
ALTER TABLE partitions.t5 DISCARD PARTITION p1sp1 TABLESPACE;
ALTER TABLE partitions.t5 DISCARD PARTITION p2sp0 TABLESPACE;
ALTER TABLE partitions.t5 IMPORT PARTITION p0sp0 TABLESPACE;
ALTER TABLE partitions.t5 IMPORT PARTITION p0sp1 TABLESPACE;
ALTER TABLE partitions.t5 IMPORT PARTITION p1sp0 TABLESPACE;
ALTER TABLE partitions.t5 IMPORT PARTITION p2sp0 TABLESPACE;
CREATE TABLE partitions.t6 (c1 INT, c2 INT)
PARTITION BY RANGE( c2 )
SUBPARTITION BY HASH( c2 )
SUBPARTITIONS 2 (
PARTITION p0 VALUES LESS THAN (10),
PARTITION p1 VALUES LESS THAN (20),
PARTITION p2 VALUES LESS THAN (30),
PARTITION p3 VALUES LESS THAN MAXVALUE
);
ALTER TABLE partitions.t6 DISCARD PARTITION p0 TABLESPACE;
ALTER TABLE partitions.t6 DISCARD PARTITION p1sp1 TABLESPACE;
CREATE TABLE partitions.t7 (c1 INT, c2 INT)
PARTITION BY RANGE( c2 )
SUBPARTITION BY HASH( c2 )
SUBPARTITIONS 2 (
PARTITION p0 VALUES LESS THAN (10),
PARTITION p1 VALUES LESS THAN (20),
PARTITION p2 VALUES LESS THAN (30),
PARTITION p3 VALUES LESS THAN MAXVALUE
);
ALTER TABLE partitions.t7 DISCARD PARTITION p0 TABLESPACE;
ALTER TABLE partitions.t7 DISCARD PARTITION p1sp0 TABLESPACE;
ALTER TABLE partitions.t7 IMPORT PARTITION p0sp0 TABLESPACE;
ALTER TABLE partitions.t7 IMPORT PARTITION p0sp1 TABLESPACE;
ALTER TABLE partitions.t7 IMPORT PARTITION p1sp0 TABLESPACE;
ALTER TABLE partitions.t1 REBUILD PARTITION p0;
ERROR HY000: Tablespace has been discarded for table 't1'
# restart
DROP TABLE partitions.t1;
DROP TABLE partitions.t2;
DROP TABLE partitions.t3;
DROP TABLE partitions.t4;
DROP TABLE partitions.t5;
DROP TABLE partitions.t6;
DROP TABLE partitions.t7;
DROP DATABASE partitions;
