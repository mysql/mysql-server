# Bug#32948
CREATE TABLE t1 (c1 INT, PRIMARY KEY (c1)) ENGINE=INNODB;
CREATE TABLE t2 (c1 INT, PRIMARY KEY (c1),
FOREIGN KEY (c1) REFERENCES t1 (c1)
ON DELETE CASCADE)
ENGINE=INNODB;
ALTER TABLE t1 PARTITION BY HASH(c1) PARTITIONS 5;
ERROR HY000: Foreign keys are not yet supported in conjunction with partitioning
ALTER TABLE t2 PARTITION BY HASH(c1) PARTITIONS 5;
ERROR HY000: Foreign keys are not yet supported in conjunction with partitioning
ALTER TABLE t1 ENGINE=MyISAM;
ERROR HY000: Cannot change table's storage engine because the table participates in a foreign key constraint.
DROP TABLE t2;
DROP TABLE t1;
create table t1 (int_column int, char_column char(5))
PARTITION BY RANGE (int_column) subpartition by key (char_column) subpartitions 2
(PARTITION p1 VALUES LESS THAN (5) ENGINE = InnoDB);
alter table t1
ENGINE = MyISAM
PARTITION BY RANGE (int_column)
subpartition by key (char_column) subpartitions 2
(PARTITION p1 VALUES LESS THAN (5));
ERROR 42000: The storage engine for the table doesn't support native partitioning
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `int_column` int DEFAULT NULL,
  `char_column` char(5) DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
/*!50100 PARTITION BY RANGE (`int_column`)
SUBPARTITION BY KEY (char_column)
SUBPARTITIONS 2
(PARTITION p1 VALUES LESS THAN (5) ENGINE = InnoDB) */
drop table t1;
