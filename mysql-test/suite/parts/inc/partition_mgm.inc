################################################################################
# inc/partition_mgm.inc                                                        #
#                                                                              #
# Purpose:                                                                     #
#   Test of partition management functions including different Upper/Lower     #
#   case names of databases, tables and partitions                             #
#                                                                              #
#                                                                              #
# Uses following variables:                                                    #
#   engine        Use specified storage engine                                 #
#   can_only_key  Storage engine only able to use HASH/KEY (not range/list)    #
#                 (E.g. not ndbcluster)                                        #
#   part_optA-D   Extra partitioning options (E.g. INDEX/DATA DIR)             #
#                                                                              #
#   have_bug33158 NDB case insensitive create, but case sensitive rename       #
#   no_truncate   No support for truncate partition                            #
#------------------------------------------------------------------------------#
# Original Author: mattiasj                                                    #
# Original Date: 2008-06-27                                                    #
################################################################################
--enable_abort_on_error

let $MYSQLD_DATADIR = `SELECT @@datadir`;
let $old_db= `SELECT DATABASE()`;
--echo # Creating database MySQL_TEST_DB
CREATE DATABASE MySQL_Test_DB;
USE MySQL_Test_DB;
let $MYSQL_TEST_DB_NAME = `SELECT DATABASE()`;
--echo # 1.0 KEY partitioning mgm
--echo # Creating KEY partitioned table
eval CREATE TABLE TableA (a INT)
ENGINE = $engine
PARTITION BY KEY (a)
(PARTITION parta $part_optA,
 PARTITION partB $part_optB,
 PARTITION Partc $part_optC,
 PARTITION PartD $part_optD);
INSERT INTO TableA VALUES (1), (2), (7), (8), (9), (10);
INSERT INTO TableA VALUES (3), (4), (5), (6), (11), (12);
--sorted_result
SELECT * FROM TableA;

--echo # Test of ADD/COALESCE PARTITIONS
--echo # expecting duplicate partition name
--error ER_SAME_NAME_PARTITION
ALTER TABLE TableA ADD PARTITION
(PARTITION partA,
 PARTITION Parta,
 PARTITION PartA);
ALTER TABLE TableA ADD PARTITION
(PARTITION partE,
 PARTITION Partf,
 PARTITION PartG);
--sorted_result
SELECT * FROM TableA;
SHOW CREATE TABLE TableA;
ALTER TABLE TableA COALESCE PARTITION 4;
--sorted_result
SELECT * FROM TableA;
SHOW CREATE TABLE TableA;

-- disable_query_log
-- disable_result_log
ANALYZE TABLE TableA;
-- enable_result_log
-- enable_query_log

--echo # Test of EXCHANGE PARTITION WITH TABLE
if (!$native_partitioning)
{
SELECT PARTITION_NAME, TABLE_ROWS FROM INFORMATION_SCHEMA.PARTITIONS WHERE TABLE_SCHEMA ='MySQL_Test_DB' AND TABLE_NAME = 'TableA';
CREATE TABLE TableB LIKE TableA;
ALTER TABLE TableB REMOVE PARTITIONING;
ALTER TABLE TableA EXCHANGE PARTITION parta WITH TABLE TableB;
--sorted_result
SELECT * FROM TableA;
SHOW CREATE TABLE TableA;
--sorted_result
SELECT * FROM TableB;
SHOW CREATE TABLE TableB;
SELECT PARTITION_NAME, IF(TABLE_ROWS, 'YES', 'NO') AS HAVE_TABLE_ROWS FROM INFORMATION_SCHEMA.PARTITIONS WHERE TABLE_SCHEMA ='MySQL_Test_DB' AND TABLE_NAME = 'TableA';
ALTER TABLE TableA EXCHANGE PARTITION parta WITH TABLE TableB;
INSERT INTO TableB VALUES (11);
--error ER_ROW_DOES_NOT_MATCH_PARTITION
ALTER TABLE TableA EXCHANGE PARTITION Partc WITH TABLE TableB;
DROP TABLE TableB;
--sorted_result
SELECT * FROM TableA;
SHOW CREATE TABLE TableA;
}

--echo # Test of REORGANIZE PARTITIONS
--echo # Should not work on HASH/KEY
--error ER_REORG_HASH_ONLY_ON_SAME_NO
eval ALTER TABLE TableA REORGANIZE PARTITION parta,partB,Partc INTO
(PARTITION PARTA $part_optA,
 PARTITION partc $part_optC);
--error ER_CONSECUTIVE_REORG_PARTITIONS
eval ALTER TABLE TableA REORGANIZE PARTITION parta,Partc INTO
(PARTITION partB $part_optA,
 PARTITION parta $part_optC);
eval ALTER TABLE TableA REORGANIZE PARTITION parta,partB INTO
(PARTITION partB $part_optA COMMENT="Previusly named parta",
 PARTITION parta $part_optB COMMENT="Previusly named partB");
if ($fixed_bug20129)
{
ALTER TABLE TableA ANALYZE PARTITION parta, partB, Partc;
ALTER TABLE TableA CHECK PARTITION parta, partB, Partc;
ALTER TABLE TableA OPTIMIZE PARTITION parta, partB, Partc;
ALTER TABLE TableA REPAIR PARTITION parta, partB, Partc;
}
--sorted_result
SELECT * FROM TableA;
SHOW CREATE TABLE TableA;

--echo # Test of RENAME TABLE
RENAME TABLE TableA to TableB;
--sorted_result
SELECT * FROM TableB;
RENAME TABLE TableB to TableA;
--sorted_result
SELECT * FROM TableA;

--echo # Checking name comparision Upper vs Lower case
--echo # Error if lower_case_table_names != 0
let $lower_case_table_names= `SELECT VARIABLE_VALUE FROM INFORMATION_SCHEMA.GLOBAL_VARIABLES WHERE VARIABLE_NAME = 'lower_case_table_names'`;
--echo # lower_case_table_names: $lower_case_table_names
if ($lower_case_table_names)
{
--error ER_TABLE_EXISTS_ERROR
eval CREATE TABLE tablea (a INT)
ENGINE = $engine
PARTITION BY KEY (a)
(PARTITION parta $part_optA,
 PARTITION partB $part_optB,
 PARTITION Partc $part_optC,
 PARTITION PartD $part_optD);
SHOW TABLES;
--error ER_TABLE_EXISTS_ERROR
RENAME TABLE TableA to tablea;
--error ER_TABLE_EXISTS_ERROR
RENAME TABLE tablea to TableA;
--sorted_result
SELECT * FROM tablea;
SHOW CREATE TABLE tablea;
}
if (!$lower_case_table_names)
{
if (!$have_bug33158)
{
eval CREATE TABLE tablea (a INT)
ENGINE = $engine
PARTITION BY KEY (a)
(PARTITION parta $part_optA,
 PARTITION partB $part_optB,
 PARTITION Partc $part_optC,
 PARTITION PartD $part_optD);
INSERT INTO tablea VALUES (1), (2), (7), (8), (9), (10);
SHOW TABLES;
RENAME TABLE TableA to tableA;
--sorted_result
SELECT * FROM tablea;
--sorted_result
SELECT * FROM tableA;
RENAME TABLE tableA to TableA;
SHOW CREATE TABLE tablea;
DROP TABLE tablea;
}
}

--echo # Test of REMOVE PARTITIONING
ALTER TABLE TableA REMOVE PARTITIONING;
--sorted_result
SELECT * FROM TableA;
SHOW CREATE TABLE TableA;

--echo # Cleaning up after KEY PARTITIONING test
DROP TABLE TableA;

if (!$can_only_key)
{
--echo # 2.0 HASH partitioning mgm
--echo # expecting duplicate partition name
--error ER_SAME_NAME_PARTITION
eval CREATE TABLE TableA (a INT)
ENGINE = $engine
PARTITION BY HASH (a)
(PARTITION parta $part_optA,
 PARTITION partA $part_optB,
 PARTITION Parta $part_optC,
 PARTITION PartA $part_optD);

--echo # Creating Hash partitioned table
eval CREATE TABLE TableA (a INT)
ENGINE = $engine
PARTITION BY HASH (a)
(PARTITION parta $part_optA,
 PARTITION partB $part_optB,
 PARTITION Partc $part_optC,
 PARTITION PartD $part_optD);
INSERT INTO TableA VALUES (1), (2), (7), (8), (9), (10);
INSERT INTO TableA VALUES (3), (4), (5), (6), (11), (12);
--sorted_result
SELECT * FROM TableA;

--echo # Test of ADD/COALESCE PARTITIONS
--echo # expecting duplicate partition name
--error ER_SAME_NAME_PARTITION
ALTER TABLE TableA ADD PARTITION
(PARTITION partA,
 PARTITION Parta,
 PARTITION PartA);
ALTER TABLE TableA ADD PARTITION
(PARTITION partE,
 PARTITION Partf,
 PARTITION PartG);
--sorted_result
SELECT * FROM TableA;
SHOW CREATE TABLE TableA;
ALTER TABLE TableA COALESCE PARTITION 4;
--sorted_result
SELECT * FROM TableA;
SHOW CREATE TABLE TableA;

--echo # Test of REORGANIZE PARTITIONS
--echo # Should not work on HASH/KEY
--error ER_REORG_HASH_ONLY_ON_SAME_NO
eval ALTER TABLE TableA REORGANIZE PARTITION parta,partB,Partc INTO
(PARTITION PARTA $part_optA,
 PARTITION partc $part_optC);
--error ER_CONSECUTIVE_REORG_PARTITIONS
eval ALTER TABLE TableA REORGANIZE PARTITION parta,Partc INTO
(PARTITION partB $part_optA,
 PARTITION parta $part_optC);
eval ALTER TABLE TableA REORGANIZE PARTITION parta,partB INTO
(PARTITION partB $part_optA COMMENT="Previusly named parta",
 PARTITION parta $part_optB COMMENT="Previusly named partB");
if ($fixed_bug20129)
{
ALTER TABLE TableA ANALYZE PARTITION parta, partB, Partc;
ALTER TABLE TableA CHECK PARTITION parta, partB, Partc;
ALTER TABLE TableA OPTIMIZE PARTITION parta, partB, Partc;
ALTER TABLE TableA REPAIR PARTITION parta, partB, Partc;
}
--sorted_result
SELECT * FROM TableA;
SHOW CREATE TABLE TableA;

--echo # Test of RENAME TABLE
RENAME TABLE TableA to TableB;
--sorted_result
SELECT * FROM TableB;
RENAME TABLE TableB to TableA;
--sorted_result
SELECT * FROM TableA;

--echo # Checking name comparision Upper vs Lower case
--echo # Error if lower_case_table_names != 0
let $lower_case_table_names= `SELECT VARIABLE_VALUE FROM INFORMATION_SCHEMA.GLOBAL_VARIABLES WHERE VARIABLE_NAME = 'lower_case_table_names'`;
--echo # lower_case_table_names: $lower_case_table_names
if ($lower_case_table_names)
{
--error ER_TABLE_EXISTS_ERROR
eval CREATE TABLE tablea (a INT)
ENGINE = $engine
PARTITION BY HASH (a)
(PARTITION parta $part_optA,
 PARTITION partB $part_optB,
 PARTITION Partc $part_optC,
 PARTITION PartD $part_optD);
SHOW TABLES;
--error ER_TABLE_EXISTS_ERROR
RENAME TABLE TableA to tablea;
--error ER_TABLE_EXISTS_ERROR
RENAME TABLE tablea to TableA;
--sorted_result
SELECT * FROM tablea;
SHOW CREATE TABLE tablea;
}
if (!$lower_case_table_names)
{
eval CREATE TABLE tablea (a INT)
ENGINE = $engine
PARTITION BY HASH (a)
(PARTITION parta $part_optA,
 PARTITION partB $part_optB,
 PARTITION Partc $part_optC,
 PARTITION PartD $part_optD);
INSERT INTO tablea VALUES (1), (2), (7), (8), (9), (10);
SHOW TABLES;
RENAME TABLE TableA to tableA;
--sorted_result
SELECT * FROM tablea;
--sorted_result
SELECT * FROM tableA;
RENAME TABLE tableA to TableA;
SHOW CREATE TABLE tablea;
DROP TABLE tablea;
}

--echo # Test of REMOVE PARTITIONING
ALTER TABLE TableA REMOVE PARTITIONING;
--sorted_result
SELECT * FROM TableA;
SHOW CREATE TABLE TableA;

--echo # Cleaning up after HASH PARTITIONING test
DROP TABLE TableA;


--echo # 3.0 RANGE partitioning mgm
--echo # Creating RANGE partitioned table
eval CREATE TABLE TableA (a INT)
ENGINE = $engine
PARTITION BY RANGE (a)
(PARTITION parta VALUES LESS THAN (4) $part_optA,
 PARTITION partB VALUES LESS THAN (7) $part_optB,
 PARTITION Partc VALUES LESS THAN (10) $part_optC,
 PARTITION PartD VALUES LESS THAN (13) $part_optD);
INSERT INTO TableA VALUES (1), (2), (7), (8), (9), (10);
INSERT INTO TableA VALUES (3), (4), (5), (6), (11), (12);
--sorted_result
SELECT * FROM TableA;

--echo # Test of ADD/DROP PARTITIONS
--echo # expecting duplicate partition name
--error ER_SAME_NAME_PARTITION
ALTER TABLE TableA ADD PARTITION
(PARTITION partA VALUES LESS THAN (MAXVALUE));
ALTER TABLE TableA ADD PARTITION
(PARTITION partE VALUES LESS THAN (16),
 PARTITION Partf VALUES LESS THAN (19),
 PARTITION PartG VALUES LESS THAN (22));
--sorted_result
SELECT * FROM TableA;
SHOW CREATE TABLE TableA;
ALTER TABLE TableA DROP PARTITION partE, PartG;
ALTER TABLE TableA DROP PARTITION Partf;
ALTER TABLE TableA ADD PARTITION
(PARTITION PartE VALUES LESS THAN (MAXVALUE));
--sorted_result
SELECT * FROM TableA;
SHOW CREATE TABLE TableA;

--echo # Test of REORGANIZE PARTITIONS
--echo # Error since it must reorganize a consecutive range
--error ER_CONSECUTIVE_REORG_PARTITIONS
eval ALTER TABLE TableA REORGANIZE PARTITION parta,Partc INTO
(PARTITION partB VALUES LESS THAN (3) $part_optA,
 PARTITION parta VALUES LESS THAN (11) $part_optC);
eval ALTER TABLE TableA REORGANIZE PARTITION partB,Partc,PartD,PartE INTO
(PARTITION partD VALUES LESS THAN (8) $part_optB
 COMMENT="Previously partB and partly Partc",
 PARTITION partB VALUES LESS THAN (11) $part_optC
 COMMENT="Previously partly Partc and partly PartD",
 PARTITION partC VALUES LESS THAN (MAXVALUE) $part_optD
 COMMENT="Previously partly PartD");
if ($fixed_bug20129)
{
ALTER TABLE TableA ANALYZE PARTITION parta, partB, Partc;
ALTER TABLE TableA CHECK PARTITION parta, partB, Partc;
ALTER TABLE TableA OPTIMIZE PARTITION parta, partB, Partc;
ALTER TABLE TableA REPAIR PARTITION parta, partB, Partc;
}
--sorted_result
SELECT * FROM TableA;
SHOW CREATE TABLE TableA;

--echo # Test of RENAME TABLE
RENAME TABLE TableA to TableB;
--sorted_result
SELECT * FROM TableB;
RENAME TABLE TableB to TableA;
--sorted_result
SELECT * FROM TableA;

--echo # Checking name comparision Upper vs Lower case
--echo # Error if lower_case_table_names != 0
let $lower_case_table_names= `SELECT VARIABLE_VALUE FROM INFORMATION_SCHEMA.GLOBAL_VARIABLES WHERE VARIABLE_NAME = 'lower_case_table_names'`;
--echo # lower_case_table_names: $lower_case_table_names
if ($lower_case_table_names)
{
--error ER_TABLE_EXISTS_ERROR
eval CREATE TABLE tablea (a INT)
ENGINE = $engine
PARTITION BY RANGE (a)
(PARTITION parta VALUES LESS THAN (4) $part_optA,
 PARTITION partB VALUES LESS THAN (7) $part_optB,
 PARTITION Partc VALUES LESS THAN (10) $part_optC,
 PARTITION PartD VALUES LESS THAN (13) $part_optD);
SHOW TABLES;
--error ER_TABLE_EXISTS_ERROR
RENAME TABLE TableA to tablea;
--error ER_TABLE_EXISTS_ERROR
RENAME TABLE tablea to TableA;
--sorted_result
SELECT * FROM tablea;
SHOW CREATE TABLE tablea;
}
if (!$lower_case_table_names)
{
eval CREATE TABLE tablea (a INT)
ENGINE = $engine
PARTITION BY RANGE (a)
(PARTITION parta VALUES LESS THAN (4) $part_optA,
 PARTITION partB VALUES LESS THAN (7) $part_optB,
 PARTITION Partc VALUES LESS THAN (10) $part_optC,
 PARTITION PartD VALUES LESS THAN (13) $part_optD);
INSERT INTO tablea VALUES (1), (2), (7), (8), (9), (10);
SHOW TABLES;
RENAME TABLE TableA to tableA;
--sorted_result
SELECT * FROM tablea;
--sorted_result
SELECT * FROM tableA;
RENAME TABLE tableA to TableA;
SHOW CREATE TABLE tablea;
DROP TABLE tablea;
}

--echo # Test of REMOVE PARTITIONING
ALTER TABLE TableA REMOVE PARTITIONING;
--sorted_result
SELECT * FROM TableA;
SHOW CREATE TABLE TableA;

--echo # Cleaning up after RANGE PARTITIONING test
DROP TABLE TableA;

--echo # 4.0 LIST partitioning mgm
--echo # Creating LIST partitioned table
eval CREATE TABLE TableA (a INT)
ENGINE = $engine
PARTITION BY LIST (a)
(PARTITION parta VALUES IN (1,8,9) $part_optA,
 PARTITION partB VALUES IN (2,10,11) $part_optB,
 PARTITION Partc VALUES IN (3,4,7) $part_optC,
 PARTITION PartD VALUES IN (5,6,12) $part_optD);
INSERT INTO TableA VALUES (1), (2), (7), (8), (9), (10);
INSERT INTO TableA VALUES (3), (4), (5), (6), (11), (12);
--sorted_result
SELECT * FROM TableA;

--echo # Test of ADD/DROP PARTITIONS
--echo # expecting duplicate partition name
--error ER_SAME_NAME_PARTITION
ALTER TABLE TableA ADD PARTITION
(PARTITION partA VALUES IN (0));
ALTER TABLE TableA ADD PARTITION
(PARTITION partE VALUES IN (16),
 PARTITION Partf VALUES IN (19),
 PARTITION PartG VALUES IN (22));
--sorted_result
SELECT * FROM TableA;
SHOW CREATE TABLE TableA;
ALTER TABLE TableA DROP PARTITION partE, PartG;
ALTER TABLE TableA DROP PARTITION Partf;
ALTER TABLE TableA ADD PARTITION
(PARTITION PartE VALUES IN (13));
--sorted_result
SELECT * FROM TableA;
SHOW CREATE TABLE TableA;

--echo # Test of REORGANIZE PARTITIONS
--error ER_CONSECUTIVE_REORG_PARTITIONS
eval ALTER TABLE TableA REORGANIZE PARTITION parta,Partc INTO
(PARTITION Partc VALUES IN (1,7) $part_optA
 COMMENT = "Mix 1 of old parta and Partc",
 PARTITION partF VALUES IN (3,9) $part_optC
 COMMENT = "Mix 2 of old parta and Partc",
 PARTITION parta VALUES IN (4,8) $part_optC
 COMMENT = "Mix 3 of old parta and Partc");
eval ALTER TABLE TableA REORGANIZE PARTITION parta,partB,Partc INTO
(PARTITION Partc VALUES IN (1,7) $part_optA
 COMMENT = "Mix 1 of old parta and Partc",
 PARTITION parta VALUES IN (3,9) $part_optC
 COMMENT = "Mix 2 of old parta and Partc",
 PARTITION partB VALUES IN (4,8) $part_optC
 COMMENT = "Mix 3 of old parta and Partc");
if ($fixed_bug20129)
{
ALTER TABLE TableA ANALYZE PARTITION parta, partB, Partc;
ALTER TABLE TableA CHECK PARTITION parta, partB, Partc;
ALTER TABLE TableA OPTIMIZE PARTITION parta, partB, Partc;
ALTER TABLE TableA REPAIR PARTITION parta, partB, Partc;
}
--sorted_result
SELECT * FROM TableA;
SHOW CREATE TABLE TableA;

--echo # Test of RENAME TABLE
RENAME TABLE TableA to TableB;
--sorted_result
SELECT * FROM TableB;
RENAME TABLE TableB to TableA;
--sorted_result
SELECT * FROM TableA;

--echo # Checking name comparision Upper vs Lower case
--echo # Error if lower_case_table_names != 0
let $lower_case_table_names= `SELECT VARIABLE_VALUE FROM INFORMATION_SCHEMA.GLOBAL_VARIABLES WHERE VARIABLE_NAME = 'lower_case_table_names'`;
--echo # lower_case_table_names: $lower_case_table_names
if ($lower_case_table_names)
{
--error ER_TABLE_EXISTS_ERROR
eval CREATE TABLE tablea (a INT)
ENGINE = $engine
PARTITION BY LIST (a)
(PARTITION parta VALUES IN (1,8,9) $part_optA,
 PARTITION partB VALUES IN (2,10,11) $part_optB,
 PARTITION Partc VALUES IN (3,4,7) $part_optC,
 PARTITION PartD VALUES IN (5,6,12) $part_optD);
SHOW TABLES;
--error ER_TABLE_EXISTS_ERROR
RENAME TABLE TableA to tablea;
--error ER_TABLE_EXISTS_ERROR
RENAME TABLE tablea to TableA;
--sorted_result
SELECT * FROM tablea;
SHOW CREATE TABLE tablea;
}
if (!$lower_case_table_names)
{
eval CREATE TABLE tablea (a INT)
ENGINE = $engine
PARTITION BY LIST (a)
(PARTITION parta VALUES IN (1,8,9) $part_optA,
 PARTITION partB VALUES IN (2,10,11) $part_optB,
 PARTITION Partc VALUES IN (3,4,7) $part_optC,
 PARTITION PartD VALUES IN (5,6,12) $part_optD);
INSERT INTO tablea VALUES (1), (2), (7), (8), (9), (10);
SHOW TABLES;
RENAME TABLE TableA to tableA;
--sorted_result
SELECT * FROM tablea;
--sorted_result
SELECT * FROM tableA;
RENAME TABLE tableA to TableA;
SHOW CREATE TABLE tablea;
DROP TABLE tablea;
}

--echo # Test of REMOVE PARTITIONING
ALTER TABLE TableA REMOVE PARTITIONING;
--sorted_result
SELECT * FROM TableA;
SHOW CREATE TABLE TableA;

--echo # Cleaning up after LIST PARTITIONING test
DROP TABLE TableA;
}
# End of $can_only_key

if ($no_truncate)
{
--echo # Verify that TRUNCATE PARTITION gives error
eval CREATE TABLE t1
(a BIGINT AUTO_INCREMENT PRIMARY KEY,
 b VARCHAR(255))
ENGINE = $engine
PARTITION BY KEY (a)
(PARTITION LT1000,
 PARTITION LT2000,
 PARTITION MAX);
INSERT INTO t1 VALUES (NULL, "First"), (NULL, "Second"), (999, "Last in LT1000"), (NULL, "First in LT2000"), (NULL, "Second in LT2000"), (1999, "Last in LT2000"), (NULL, "First in MAX"), (NULL, "Second in MAX");
--error ER_PARTITION_MGMT_ON_NONPARTITIONED, ER_ILLEGAL_HA
ALTER TABLE t1 TRUNCATE PARTITION MAX;
--sorted_result
SELECT * FROM t1;
}
if (!$no_truncate)
{
--echo # Testing TRUNCATE PARTITION
eval CREATE TABLE t1
(a BIGINT AUTO_INCREMENT PRIMARY KEY,
 b VARCHAR(255))
ENGINE = $engine
PARTITION BY RANGE (a)
(PARTITION LT1000 VALUES LESS THAN (1000),
 PARTITION LT2000 VALUES LESS THAN (2000),
 PARTITION MAX VALUES LESS THAN MAXVALUE);
INSERT INTO t1 VALUES (NULL, "First"), (NULL, "Second"), (999, "Last in LT1000"), (NULL, "First in LT2000"), (NULL, "Second in LT2000"), (1999, "Last in LT2000"), (NULL, "First in MAX"), (NULL, "Second in MAX");
SHOW CREATE TABLE t1;
SELECT * FROM t1 ORDER BY a;
ALTER TABLE t1 ANALYZE PARTITION MAX;
--echo # Truncate without FLUSH
ALTER TABLE t1 TRUNCATE PARTITION MAX;
INSERT INTO t1 VALUES (NULL, "First after TRUNCATE MAX (1)");
SELECT * FROM t1 WHERE a >= 2000;
--echo # Truncate with FLUSH after
ALTER TABLE t1 TRUNCATE PARTITION MAX;
FLUSH TABLES;
INSERT INTO t1 VALUES (NULL, "First after TRUNCATE MAX (2)");
SELECT * FROM t1 WHERE a >= 2000;
--echo # Truncate with FLUSH before
FLUSH TABLES;
ALTER TABLE t1 TRUNCATE PARTITION MAX;
INSERT INTO t1 VALUES (NULL, "First after TRUNCATE MAX (3)");
SELECT * FROM t1 WHERE a >= 2000;
--echo # Truncate with FLUSH after INSERT
FLUSH TABLES;
ALTER TABLE t1 TRUNCATE PARTITION MAX;
INSERT INTO t1 VALUES (NULL, "First after TRUNCATE MAX (4)");
SELECT * FROM t1 WHERE a >= 2000;
--echo # Truncate without FLUSH
ALTER TABLE t1 TRUNCATE PARTITION LT1000;
INSERT INTO t1 VALUES (NULL, "First after TRUNCATE LT1000 (1)");
SELECT * FROM t1 ORDER BY a;
--echo # Truncate with FLUSH after
ALTER TABLE t1 TRUNCATE PARTITION LT1000;
FLUSH TABLES;
INSERT INTO t1 VALUES (NULL, "First after TRUNCATE LT1000 (2)");
SELECT * FROM t1 ORDER BY a;
--echo # Truncate with FLUSH before
FLUSH TABLES;
ALTER TABLE t1 TRUNCATE PARTITION LT1000;
INSERT INTO t1 VALUES (NULL, "First after TRUNCATE LT1000 (3)");
SELECT * FROM t1 ORDER BY a;
--echo # Truncate with FLUSH after INSERT
FLUSH TABLES;
ALTER TABLE t1 TRUNCATE PARTITION LT1000;
INSERT INTO t1 VALUES (NULL, "First after TRUNCATE LT1000 (4)");
SELECT * FROM t1 ORDER BY a;
--echo # Truncate without FLUSH
ALTER TABLE t1 TRUNCATE PARTITION LT2000;
INSERT INTO t1 VALUES (NULL, "First after TRUNCATE LT2000 (1)");
SELECT * FROM t1 ORDER BY a;
--echo # Truncate with FLUSH after
ALTER TABLE t1 TRUNCATE PARTITION LT2000;
FLUSH TABLES;
INSERT INTO t1 VALUES (NULL, "First after TRUNCATE LT2000 (2)");
SELECT * FROM t1 ORDER BY a;
--echo # Truncate with FLUSH before
FLUSH TABLES;
ALTER TABLE t1 TRUNCATE PARTITION LT2000;
INSERT INTO t1 VALUES (NULL, "First after TRUNCATE LT2000 (3)");
SELECT * FROM t1 ORDER BY a;
--echo # Truncate with FLUSH after INSERT
FLUSH TABLES;
ALTER TABLE t1 TRUNCATE PARTITION LT2000;
INSERT INTO t1 VALUES (NULL, "First after TRUNCATE LT2000 (4)");
SELECT * FROM t1 ORDER BY a;
DROP TABLE t1;
}
--echo # Cleaning up before exit
eval USE $old_db;
DROP DATABASE MySQL_Test_DB;
