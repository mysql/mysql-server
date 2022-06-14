# Partitioning test that require debug features

--source include/have_debug.inc

--echo #
--echo # Bug#13737949: CRASH IN HA_PARTITION::INDEX_INIT
--echo # Bug#18694052: SERVER CRASH IN HA_PARTITION::INIT_RECORD_PRIORITY_QUEUE
--echo #
CREATE TABLE t1 (a INT, b VARCHAR(64), KEY(b,a))
PARTITION BY HASH (a) PARTITIONS 3;
INSERT INTO t1 VALUES (1, "1"), (2, "2"), (3, "3"), (4, "Four"), (5, "Five"),
(6, "Six"), (7, "Seven"), (8, "Eight"), (9, "Nine");
SET SESSION debug="+d,partition_fail_index_init";
--error ER_NO_PARTITION_FOR_GIVEN_VALUE
SELECT * FROM t1 WHERE b = "Seven";
SET SESSION debug="-d,partition_fail_index_init";
SELECT * FROM t1 WHERE b = "Seven";
DROP TABLE t1;

--echo #
--echo # Bug 29706669 ALTER EXCHANGE CAUSES INDEX CORRUPTION
--echo #

CREATE TABLE t1_part_table ( f1 INT PRIMARY KEY,
			     f2 iNT , f3 INT,
			     KEY `IDX_F2` (f2),
			     KEY `IDX_F3` (f3)
                           ) ENGINE=INNODB
PARTITION BY RANGE (f1) (
PARTITION p0 VALUES LESS THAN (10),
PARTITION p1 VALUES LESS THAN (20));

CREATE TABLE t1_normal_table ( f1 INT PRIMARY KEY,
			     f2 iNT , f3 INT,
			     KEY `IDX_F3` (f3),
			     KEY `IDX_F2` (f2)
			   ) ENGINE=INNODB;

INSERT INTO t1_part_table VALUES (5,10,20),(6,30,40),(15,50,60),(17,70,80);
INSERT INTO t1_normal_table VALUES (19,90,100),(18,110,120);

SET SESSION debug='+d,skip_dd_table_access_check';

SELECT t.NAME AS TABLE_NAME , i.NAME AS INDEX_NAME , i.ORDINAL_POSITION
 FROM mysql.indexes i JOIN mysql.tables t ON i.TABLE_ID = t.ID
 WHERE t.NAME='t1_normal_table' ORDER BY i.ORDINAL_POSITION;

SHOW CREATE TABLE t1_normal_table;

SELECT t.NAME AS TABLE_NAME, p.NAME AS PARTITION_NAME , i.NAME AS INDEX_NAME, i.ORDINAL_POSITION
  FROM  mysql.tables t JOIN mysql.table_partitions p ON t.ID = p.TABLE_ID
        JOIN mysql.index_partitions ip ON p.ID = ip.PARTITION_ID
        JOIN mysql.indexes i ON i.ID = ip.INDEX_ID
  WHERE t.NAME= 't1_part_table' ORDER BY i.ORDINAL_POSITION,p.NAME;

SHOW CREATE TABLE t1_part_table;

ALTER TABLE t1_part_table EXCHANGE PARTITION p1 WITH TABLE t1_normal_table;

SELECT t.NAME AS TABLE_NAME , i.NAME AS INDEX_NAME , i.ORDINAL_POSITION
 FROM mysql.indexes i JOIN mysql.tables t ON i.TABLE_ID = t.ID
 WHERE t.NAME='t1_normal_table'  ORDER BY i.ORDINAL_POSITION;

SHOW CREATE TABLE t1_normal_table;

SELECT t.NAME AS TABLE_NAME, p.NAME AS PARTITION_NAME , i.NAME AS INDEX_NAME, i.ORDINAL_POSITION
  FROM  mysql.tables t JOIN mysql.table_partitions p ON t.ID = p.TABLE_ID
        JOIN mysql.index_partitions ip ON p.ID = ip.PARTITION_ID
        JOIN mysql.indexes i ON i.ID = ip.INDEX_ID
  WHERE t.NAME= 't1_part_table' ORDER BY i.ORDINAL_POSITION,p.NAME;

SHOW CREATE TABLE t1_part_table;

SET SESSION debug='-d,skip_dd_table_access_check';

CHECK TABLE t1_part_table;
CHECK TABLE t1_normal_table;
SELECT * FROM t1_part_table;
SELECT * FROM t1_normal_table;
DROP TABLE t1_part_table;
DROP TABLE t1_normal_table;

--echo #
--echo # Bug #30437407 ASSERTION ".NUM_ROWS == .~.HA_ROWS"
--echo #

CREATE TABLE t1(a int) PARTITION BY LIST (a) (PARTITION x1 VALUES IN
(2),PARTITION x2 VALUES IN (3));
ALTER TABLE t1 DISCARD TABLESPACE;
--skip_if_hypergraph  # Error message is worded differently.
--error ER_TABLESPACE_DISCARDED
SELECT COUNT(*)FROM t1;
DROP TABLE t1;

--echo #
--echo # Bug #31941543	EXCHANGE PARTITION ASSERT
--echo #

# Choosing a table with auto generated primary key
# and a secondary index whose name is less than  "PRIMARY".
# so when sorted "IX_LogTime" comes first  and "PRIMARY" comes
# after that. This condtion was not caught by the fix for
# Bug 29706669.
CREATE TABLE t1 (
    LogTime TIMESTAMP NOT NULL DEFAULT '2000-01-01 00:00:00',
    UserAgent VARCHAR(256) COLLATE utf8mb4_bin DEFAULT NULL,
    KEY IX_LogTime (LogTime)
) ENGINE=INNODB
PARTITION BY RANGE(UNIX_TIMESTAMP(LogTime))
  (
   PARTITION p201407 VALUES LESS THAN (UNIX_TIMESTAMP('2014-08-01')),
   PARTITION p201412 VALUES LESS THAN (UNIX_TIMESTAMP('2015-01-01')),
   PARTITION p201501 VALUES LESS THAN (UNIX_TIMESTAMP('2015-02-01')),
   PARTITION p201502 VALUES LESS THAN (UNIX_TIMESTAMP('2015-03-01')),
   PARTITION p201503 VALUES LESS THAN (UNIX_TIMESTAMP('2015-04-01')),
   PARTITION future VALUES LESS THAN MAXVALUE
  );
SHOW CREATE TABLE t1;

CREATE TABLE t2 (
    LogTime TIMESTAMP NOT NULL DEFAULT '2000-01-01 00:00:00',
    UserAgent VARCHAR(256) COLLATE utf8mb4_bin DEFAULT NULL,
    KEY IX_LogTime (LogTime)
) ENGINE=INNODB;
SHOW CREATE TABLE t2;

INSERT INTO t2 VALUES('2014-08-02 00:00:00','useragent');

SELECT * FROM t2;
SELECT * FROM t1;

ALTER TABLE t1 EXCHANGE PARTITION p201412 WITH TABLE t2;

SELECT * FROM t2;
SELECT * FROM t1;

DROP TABLE t1,t2;
