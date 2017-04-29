#
# This script assumes that the caller has set the following variables:
#	$TABLESPACE1
#	$TABLESPACE2
#	$TABLESPACE3
#	$TABLESPACE4
#	$INNODB_FILE_PER_TABLE_FOR_CREATE
#	$INNODB_FILE_PER_TABLE_FOR_ALTER
#
--eval SET GLOBAL innodb_file_per_table = $INNODB_FILE_PER_TABLE_FOR_CREATE

eval CREATE TABLE t1 (a INT NOT NULL, b INT)
ENGINE=InnoDB
PARTITION BY RANGE(a)
  (PARTITION p1 VALUES LESS THAN (5) $TABLESPACE1,
   PARTITION p2 VALUES LESS THAN (10) $TABLESPACE2,
   PARTITION p3 VALUES LESS THAN (15) $TABLESPACE3);

SHOW CREATE TABLE t1;

--disable_query_log
--replace_result #P# #p#
--replace_regex /innodb_file_per_table.[0-9]+/innodb_file_per_table.##/
SELECT A.NAME as partition_name, A.SPACE_TYPE as space_type, B.NAME as
space_name
FROM information_schema.innodb_tables A
LEFT JOIN
information_schema.innodb_tablespaces B
ON A.SPACE = B.SPACE
WHERE A.NAME LIKE '%t1%' ORDER BY A.NAME;
--enable_query_log

--eval SET GLOBAL innodb_file_per_table = $INNODB_FILE_PER_TABLE_FOR_ALTER

eval ALTER TABLE t1 ADD PARTITION
  (PARTITION p4 VALUES LESS THAN (20) $TABLESPACE4);

SHOW CREATE TABLE t1;

--disable_query_log
--replace_result #P# #p#
--replace_regex /innodb_file_per_table.[0-9]+/innodb_file_per_table.##/
SELECT A.NAME as partition_name, A.SPACE_TYPE as space_type, B.NAME as
space_name
FROM information_schema.innodb_tables A
LEFT JOIN
information_schema.innodb_tablespaces B
ON A.SPACE = B.SPACE
WHERE A.NAME LIKE '%t1%' ORDER BY A.NAME;
--enable_query_log

DROP TABLE t1;

--eval SET GLOBAL innodb_file_per_table = $INNODB_FILE_PER_TABLE_FOR_CREATE

eval CREATE TABLE t2 (a INT NOT NULL, b INT)
ENGINE=InnoDB
PARTITION BY RANGE(a)
SUBPARTITION BY KEY(b)
  (PARTITION p1 VALUES LESS THAN (5)
	(SUBPARTITION sp1 $TABLESPACE1),
   PARTITION p2 VALUES LESS THAN (10)
	(SUBPARTITION sp2 $TABLESPACE2),
   PARTITION p3 VALUES LESS THAN (15)
	(SUBPARTITION sp3 $TABLESPACE3));

SHOW CREATE TABLE t2;

--disable_query_log
--replace_result #P# #p# #SP# #sp#
--replace_regex /innodb_file_per_table.[0-9]+/innodb_file_per_table.##/
SELECT A.NAME as partition_name, A.SPACE_TYPE as space_type, B.NAME as
space_name
FROM information_schema.innodb_tables A
LEFT JOIN
information_schema.innodb_tablespaces B
ON A.SPACE = B.SPACE
WHERE A.NAME LIKE '%t2%' ORDER BY A.NAME;
--enable_query_log

--eval SET GLOBAL innodb_file_per_table = $INNODB_FILE_PER_TABLE_FOR_ALTER

eval ALTER TABLE t2 ADD PARTITION
  (PARTITION p4 VALUES LESS THAN (20)
	(SUBPARTITION sp4 $TABLESPACE4));

SHOW CREATE TABLE t2;

--disable_query_log
--replace_result #P# #p# #SP# #sp#
--replace_regex /innodb_file_per_table.[0-9]+/innodb_file_per_table.##/
SELECT A.NAME as partition_name, A.SPACE_TYPE as space_type, B.NAME as
space_name
FROM information_schema.innodb_tables A
LEFT JOIN
information_schema.innodb_tablespaces B
ON A.SPACE = B.SPACE
WHERE A.NAME LIKE '%t2%' ORDER BY A.NAME;
--enable_query_log

DROP TABLE t2;
