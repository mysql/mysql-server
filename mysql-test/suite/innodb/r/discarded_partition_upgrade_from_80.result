# Testing discarded partitions imported from 8.0.20
# restart: --datadir=BUGDATADIR
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
# restart
