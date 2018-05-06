# Include file to test PARTITION EXCHANGE usable with different engines
# Written by Mattias.Jonsson@Sun.Com

--echo # Test with AUTO_INCREMENT
eval CREATE TABLE tp
(a INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
 b varchar(24))
ENGINE = $engine
PARTITION BY HASH (a) PARTITIONS 4;
CREATE TABLE t LIKE tp;
ALTER TABLE t REMOVE PARTITIONING;
SHOW CREATE TABLE tp;
SHOW CREATE TABLE t;
INSERT INTO tp (b) VALUES ("One"), ("Two"), ("Three"), ("Four"), ("Five"),
("Six"), ("Seven"), ("Eight"), ("Nine"), ("Ten"), ("Eleven"), ("Twelwe");
if ($no_update)
{
# Archive handles auto inc by only allowing increasing values,
# so this must be inserted before a higher value is inserted.
INSERT INTO tp VALUES (41, "One hundred one");
}
INSERT INTO tp VALUES (97, "Ninety seven");
if (!$no_delete)
{
INSERT INTO tp VALUES (111, "One hundred eleven");
}
if (!$no_update)
{
INSERT INTO tp VALUES (101, "One hundred one");
}
SET INSERT_ID = 13;
INSERT INTO t (b) VALUES ("Thirteen");
if ($no_update)
{
SET INSERT_ID = 17;
INSERT INTO t (b) VALUES ("Twenty five");
}
SET INSERT_ID = 21;
INSERT INTO t (b) VALUES ("Twenty one");
if (!$no_update)
{
SET INSERT_ID = 25;
INSERT INTO t (b) VALUES ("Twenty five");
}
if (!$no_delete)
{
SET INSERT_ID = 55;
INSERT INTO t (b) VALUES ("Fifty five");
DELETE FROM tp WHERE a = 111;
DELETE FROM t WHERE a = 55;
}
if (!$no_update)
{
UPDATE tp SET a = 41 WHERE a = 101;
UPDATE t SET a = 17 WHERE a = 25;
}
-- disable_query_log
-- disable_result_log
ANALYZE TABLE tp;
ANALYZE TABLE t;
-- enable_result_log
-- enable_query_log
SELECT PARTITION_NAME, IF(TABLE_ROWS, 'YES', 'NO') AS HAVE_ROWS FROM INFORMATION_SCHEMA.PARTITIONS WHERE TABLE_SCHEMA ='test' AND TABLE_NAME = 'tp';
SELECT IF(TABLE_ROWS, 'YES', 'NO') AS HAVE_ROWS FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA ='test' AND TABLE_NAME = 't';
ALTER TABLE tp EXCHANGE PARTITION p1 WITH TABLE t;
-- disable_query_log
-- disable_result_log
ANALYZE TABLE tp;
-- enable_result_log
-- enable_query_log
SELECT PARTITION_NAME, IF(TABLE_ROWS, 'YES', 'NO') AS HAVE_ROWS FROM INFORMATION_SCHEMA.PARTITIONS WHERE TABLE_SCHEMA ='test' AND TABLE_NAME = 'tp';
SELECT IF(TABLE_ROWS, 'YES', 'NO') AS HAVE_ROWS FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA ='test' AND TABLE_NAME = 't';
SELECT * FROM tp ORDER BY a;
SELECT * FROM t ORDER BY a;
SHOW CREATE TABLE tp;
SHOW CREATE TABLE t;
DROP TABLE tp, t;

# set a DEBUG_SYNC after open table and before verifying is done
# Test that one can read from the table but not write, test that one can both
# read and write to the partition
# Verify that one must close the other instances of the table before the rename
# takes place (i.e. no need for LOCK_open)
# Test that it is not possible to drop, create, truncate either the tables or
# partition
# Test that it waits for ongoing transactions

if ($no_keys)
{
eval CREATE TABLE t
(a INT,
 b VARCHAR(55))
ENGINE =  $engine;
eval CREATE TABLE tp
(a INT,
 b VARCHAR(55))
ENGINE =  $engine
PARTITION BY RANGE (a)
(PARTITION p0 VALUES LESS THAN (100),
 PARTITION p1 VALUES LESS THAN MAXVALUE);
eval CREATE TABLE tsp
(a INT,
 b VARCHAR(55))
ENGINE =  $engine
PARTITION BY RANGE (a)
SUBPARTITION BY HASH(a)
(PARTITION p0 VALUES LESS THAN (100)
 (SUBPARTITION sp0,
  SUBPARTITION sp1),
 PARTITION p1 VALUES LESS THAN MAXVALUE
 (SUBPARTITION sp2,
  SUBPARTITION sp3));
}
if (!$no_keys)
{
eval CREATE TABLE t
(a INT,
 b VARCHAR(55),
 PRIMARY KEY (a))
ENGINE =  $engine;
eval CREATE TABLE tp
(a INT,
 b VARCHAR(55),
 PRIMARY KEY (a))
ENGINE =  $engine
PARTITION BY RANGE (a)
(PARTITION p0 VALUES LESS THAN (100),
 PARTITION p1 VALUES LESS THAN MAXVALUE);
eval CREATE TABLE tsp
(a INT,
 b VARCHAR(55),
 PRIMARY KEY (a))
ENGINE =  $engine
PARTITION BY RANGE (a)
SUBPARTITION BY HASH(a)
(PARTITION p0 VALUES LESS THAN (100)
 (SUBPARTITION sp0,
  SUBPARTITION sp1),
 PARTITION p1 VALUES LESS THAN MAXVALUE
 (SUBPARTITION sp2,
  SUBPARTITION sp3));
}

INSERT INTO t VALUES (1, "First value"), (3, "Three"), (5, "Five"), (99, "End of values");
INSERT INTO tp VALUES (2, "First value"), (10, "Ten"), (50, "Fifty"), (200, "Two hundred, end of values"), (61, "Sixty one"), (62, "Sixty two"), (63, "Sixty three"), (64, "Sixty four"), (161, "161"), (162, "162"), (163, "163"), (164, "164");
INSERT INTO tsp VALUES (2, "First value"), (10, "Ten"), (50, "Fifty"), (200, "Two hundred, end of values"), (61, "Sixty one"), (62, "Sixty two"), (63, "Sixty three"), (64, "Sixty four"), (161, "161"), (162, "162"), (163, "163"), (164, "164");
--sorted_result
SELECT * FROM t;
--sorted_result
SELECT * FROM tp;

--echo # Start by testing read/write locking
SET AUTOCOMMIT = 1;

connect(con1, localhost, root,,);
--echo # con1
SET DEBUG_SYNC= 'swap_partition_after_compare_tables SIGNAL swap_in_progress WAIT_FOR goto_verification';
SET DEBUG_SYNC= 'swap_partition_first_row_read SIGNAL swap_in_progress WAIT_FOR goto_wait';
SET DEBUG_SYNC= 'swap_partition_after_wait SIGNAL swap_in_progress WAIT_FOR goto_exchange';
SET DEBUG_SYNC= 'swap_partition_before_exchange SIGNAL swap_in_progress WAIT_FOR test_done';
send ALTER TABLE tp EXCHANGE PARTITION p0 WITH TABLE t;

connection default;
--echo # con default
SET DEBUG_SYNC= 'now WAIT_FOR swap_in_progress';
--echo # select from t and select/update/delete/insert from tp should work
SELECT * FROM t WHERE a = 99;
SELECT * FROM tp WHERE a = 61;
--echo # any write (update/delete/insert) into t or tp should fail
SET SESSION lock_wait_timeout=1;
if (!$no_update)
{
--error ER_LOCK_WAIT_TIMEOUT
UPDATE tp SET a = 53, b = concat("Fifty three, was ", b) WHERE a = 63;
}
--error ER_LOCK_WAIT_TIMEOUT
INSERT INTO tp VALUES (63, "Sixty three, new"), (59, "To be deleted");
if (!$no_delete)
{
--error ER_LOCK_WAIT_TIMEOUT
DELETE FROM tp WHERE a = 59;
}
if (!$no_update)
{
--error ER_LOCK_WAIT_TIMEOUT
UPDATE t SET a = 53, b = "Fifty three, was three" WHERE a = 3;
}
--error ER_LOCK_WAIT_TIMEOUT
INSERT INTO t VALUES (63, "Sixty three, new"), (59, "To be deleted");
if (!$no_delete)
{
--error ER_LOCK_WAIT_TIMEOUT
DELETE FROM t WHERE a = 3;
}
--error ER_LOCK_WAIT_TIMEOUT
eval ALTER TABLE t ENGINE = $engine;
--error ER_LOCK_WAIT_TIMEOUT
eval ALTER TABLE tp ENGINE = $engine;
SHOW CREATE TABLE t;
SHOW CREATE TABLE tp;
SET DEBUG_SYNC= 'now SIGNAL goto_verification';
SET DEBUG_SYNC= 'now WAIT_FOR swap_in_progress';
--echo # select from t and select/update/delete/insert from tp should work
SELECT * FROM t WHERE a = 99;
SELECT * FROM tp WHERE a = 61;
if (!$no_update)
{
--error ER_LOCK_WAIT_TIMEOUT
UPDATE tp SET a = 43, b = concat("Fifty three, was ", b) WHERE a = 63;
}
--error ER_LOCK_WAIT_TIMEOUT
INSERT INTO tp VALUES (63, "Sixty three, new 2"), (59, "To be deleted");
if (!$no_delete)
{
--error ER_LOCK_WAIT_TIMEOUT
DELETE FROM tp WHERE a = 59;
}
--echo # any write (update/delete/insert) into t should fail
if (!$no_update)
{
--error ER_LOCK_WAIT_TIMEOUT
UPDATE t SET a = 53, b = "Fifty three, was three" WHERE a = 3;
}
--error ER_LOCK_WAIT_TIMEOUT
INSERT INTO t VALUES (63, "Sixty three, new"), (59, "To be deleted");
if (!$no_delete)
{
--error ER_LOCK_WAIT_TIMEOUT
DELETE FROM t WHERE a = 3;
}
--error ER_LOCK_WAIT_TIMEOUT
eval ALTER TABLE t ENGINE = $engine;
--error ER_LOCK_WAIT_TIMEOUT
eval ALTER TABLE tp ENGINE = $engine;
SHOW CREATE TABLE t;
SHOW CREATE TABLE tp;
SET DEBUG_SYNC= 'now SIGNAL goto_wait';
SET DEBUG_SYNC= 'now WAIT_FOR swap_in_progress';
--echo # Both tables should now be under exclusive lock, even SHOW should fail
--error ER_LOCK_WAIT_TIMEOUT
SELECT * FROM t WHERE a = 99;
--error ER_LOCK_WAIT_TIMEOUT
SELECT * FROM tp WHERE a = 61;
if (!$no_update)
{
--error ER_LOCK_WAIT_TIMEOUT
UPDATE tp SET a = 53, b = concat("Fifty three, was ", b) WHERE a = 63;
}
--error ER_LOCK_WAIT_TIMEOUT
INSERT INTO tp VALUES (63, "Sixty three, new 2"), (59, "To be deleted");
if (!$no_delete)
{
--error ER_LOCK_WAIT_TIMEOUT
DELETE FROM tp WHERE a = 59;
}
if (!$no_update)
{
--error ER_LOCK_WAIT_TIMEOUT
UPDATE t SET a = 53, b = "Fifty three, was three" WHERE a = 3;
}
--error ER_LOCK_WAIT_TIMEOUT
INSERT INTO t VALUES (63, "Sixty three, new"), (59, "To be deleted");
if (!$no_delete)
{
--error ER_LOCK_WAIT_TIMEOUT
DELETE FROM t WHERE a = 3;
}
--error ER_LOCK_WAIT_TIMEOUT
SHOW CREATE TABLE t;
--error ER_LOCK_WAIT_TIMEOUT
SHOW CREATE TABLE tp;
--error ER_LOCK_WAIT_TIMEOUT
eval ALTER TABLE t ENGINE = $engine;
--error ER_LOCK_WAIT_TIMEOUT
eval ALTER TABLE tp ENGINE = $engine;
SET DEBUG_SYNC= 'now SIGNAL goto_exchange';
SET DEBUG_SYNC= 'now WAIT_FOR swap_in_progress';
--echo # Both tables should now be under exclusive lock
--error ER_LOCK_WAIT_TIMEOUT
SELECT * FROM t WHERE a = 99;
--error ER_LOCK_WAIT_TIMEOUT
SELECT * FROM tp WHERE a = 61;
if (!$no_update)
{
--error ER_LOCK_WAIT_TIMEOUT
UPDATE tp SET a = 53, b = concat("Fifty three, was ", b) WHERE a = 63;
}
--error ER_LOCK_WAIT_TIMEOUT
INSERT INTO tp VALUES (63, "Sixty three, new 2"), (59, "To be deleted");
if (!$no_delete)
{
--error ER_LOCK_WAIT_TIMEOUT
DELETE FROM tp WHERE a = 59;
}
if (!$no_update)
{
--error ER_LOCK_WAIT_TIMEOUT
UPDATE t SET a = 53, b = "Fifty three, was three" WHERE a = 3;
}
--error ER_LOCK_WAIT_TIMEOUT
INSERT INTO t VALUES (63, "Sixty three, new"), (59, "To be deleted");
if (!$no_delete)
{
--error ER_LOCK_WAIT_TIMEOUT
DELETE FROM t WHERE a = 3;
}
--error ER_LOCK_WAIT_TIMEOUT
eval ALTER TABLE t ENGINE = $engine;
--error ER_LOCK_WAIT_TIMEOUT
eval ALTER TABLE tp ENGINE = $engine;
--error ER_LOCK_WAIT_TIMEOUT
SHOW CREATE TABLE t;
--error ER_LOCK_WAIT_TIMEOUT
SHOW CREATE TABLE tp;

SET DEBUG_SYNC= 'now SIGNAL test_done';

connection con1;
--echo # con1
--reap
connection default;
--echo # con default
--echo # Tables should now be as normal
SHOW CREATE TABLE t;
SHOW CREATE TABLE tp;
SELECT * FROM tp WHERE a = 99;
SELECT * FROM t WHERE a = 61;
if (!$no_update)
{
UPDATE t SET a = 53, b = "Fifty three, was sixty three" WHERE a = 63;
}
INSERT INTO t VALUES (63, "Sixty three, new"), (59, "To be deleted");
if (!$no_delete)
{
DELETE FROM t WHERE a = 59;
}
if (!$no_update)
{
UPDATE tp SET a = 53, b = "Fifty three, was three" WHERE a = 3;
}
INSERT INTO tp VALUES (63, "Sixty three, new"), (59, "To be deleted");
if (!$no_delete)
{
DELETE FROM tp WHERE a = 3;
}
eval ALTER TABLE t ENGINE = $engine;
eval ALTER TABLE tp ENGINE = $engine;

disconnect con1;
connection default;
--echo # con default
SET DEBUG_SYNC= 'RESET';
SHOW CREATE TABLE t;
SHOW CREATE TABLE tp;
--sorted_result
SELECT * FROM t;
--sorted_result
SELECT * FROM tp;

DROP TABLE t, tp, tsp;


