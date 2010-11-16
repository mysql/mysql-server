# inc/partition_auto_increment.inc
#
# auto_increment test
# used variables: $engine
#

-- disable_warnings
DROP TABLE IF EXISTS t1;
-- enable_warnings

-- echo # test without partitioning for reference
eval CREATE TABLE t1 (
  c1 INT NOT NULL AUTO_INCREMENT,
  PRIMARY KEY (c1))
ENGINE=$engine;
SHOW CREATE TABLE t1;
SELECT AUTO_INCREMENT FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA='test'
AND TABLE_NAME='t1';
INSERT INTO t1 VALUES (2);
INSERT INTO t1 VALUES (4);
INSERT INTO t1 VALUES (NULL);
SELECT AUTO_INCREMENT FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA='test'
AND TABLE_NAME='t1';
SELECT AUTO_INCREMENT FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA='test'
AND TABLE_NAME='t1';
INSERT INTO t1 VALUES (0);
-- error 0, ER_DUP_KEY, ER_DUP_ENTRY
INSERT INTO t1 VALUES (5), (16);
if (!$mysql_errno)
{
  echo # ERROR (only OK if Blackhole) should give ER_DUP_KEY or ER_DUP_ENTRY;
  echo # mysql_errno: $mysql_errno;
}
INSERT INTO t1 VALUES (17);
INSERT INTO t1 VALUES (19), (NULL);
-- error 0, ER_DUP_ENTRY, ER_DUP_KEY
INSERT INTO t1 VALUES (NULL), (10), (NULL);
if ($mysql_errno)
{
  echo # ERROR (only OK if Archive) mysql_errno: $mysql_errno;
}
INSERT INTO t1 VALUES (NULL);
SET INSERT_ID = 30;
INSERT INTO t1 VALUES (NULL);
SET INSERT_ID = 29;
-- error 0, ER_DUP_ENTRY, ER_DUP_KEY
INSERT INTO t1 VALUES (NULL), (NULL), (NULL);
if (!$mysql_errno)
{
  echo # ERROR (only OK if Blackhole) should give ER_DUP_KEY or ER_DUP_ENTRY;
  echo # mysql_errno: $mysql_errno;
}
INSERT INTO t1 VALUES (NULL);
if (!$skip_update)
{
# InnoDB Does not handle this correctly, see bug#14793, bug#21641
  UPDATE t1 SET c1 = 50 WHERE c1 = 17;
  UPDATE t1 SET c1 = 51 WHERE c1 = 19;
  FLUSH TABLES;
  UPDATE t1 SET c1 = 40 WHERE c1 = 50;
  UPDATE t1 SET c1 = -1 WHERE c1 = 40;
  SELECT AUTO_INCREMENT FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA='test'
  AND TABLE_NAME='t1';
  UPDATE t1 SET c1 = NULL WHERE c1 = 4;
  INSERT INTO t1 VALUES (NULL);
  INSERT INTO t1 VALUES (NULL);
}
SELECT * FROM t1 ORDER BY c1;
DROP TABLE t1;
eval CREATE TABLE t1 (
  c1 INT NOT NULL AUTO_INCREMENT,
  PRIMARY KEY (c1))
ENGINE=$engine;
SHOW CREATE TABLE t1;
FLUSH TABLE;
SHOW CREATE TABLE t1;
INSERT INTO t1 VALUES (4);
FLUSH TABLE;
SHOW CREATE TABLE t1;
INSERT INTO t1 VALUES (NULL);
FLUSH TABLE;
SHOW CREATE TABLE t1;
if (!$skip_delete)
{
DELETE FROM t1;
}
INSERT INTO t1 VALUES (NULL);
--error 0, ER_DUP_KEY
INSERT INTO t1 VALUES (-1);
if ($mysql_errno)
{
  echo # ERROR (only OK if Archive) mysql_errno: $mysql_errno;
}
INSERT INTO t1 VALUES (NULL);
INSERT INTO t1 VALUES (NULL);
SHOW CREATE TABLE t1;
SELECT * FROM t1 ORDER BY c1;
if (!$skip_truncate)
{
TRUNCATE TABLE t1;
}
INSERT INTO t1 VALUES (NULL);
SHOW CREATE TABLE t1;
SELECT * FROM t1 ORDER BY c1;
INSERT INTO t1 VALUES (100);
INSERT INTO t1 VALUES (NULL);
if (!$skip_delete)
{
DELETE FROM t1 WHERE c1 >= 100;
}
# InnoDB does reset auto_increment on OPTIMIZE, Bug#18274
# Archive does reset auto_increment on OPTIMIZE, Bug#40216
OPTIMIZE TABLE t1;
SHOW CREATE TABLE t1;
DROP TABLE t1;

if (!$skip_update)
{
eval CREATE TABLE t1
(a INT NULL AUTO_INCREMENT,
 UNIQUE KEY (a))
ENGINE=$engine;
SET LAST_INSERT_ID = 999;
SET INSERT_ID = 0;
INSERT INTO t1 SET a = 1 ON DUPLICATE KEY UPDATE a = NULL;
SELECT LAST_INSERT_ID();
SELECT * FROM t1;
INSERT INTO t1 SET a = 1 ON DUPLICATE KEY UPDATE a = NULL;
SELECT LAST_INSERT_ID();
SELECT * FROM t1;
UPDATE t1 SET a = 1 WHERE a IS NULL;
SELECT LAST_INSERT_ID();
SELECT * FROM t1;
UPDATE t1 SET a = NULL WHERE a = 1;
SELECT LAST_INSERT_ID();
SELECT * FROM t1;
DROP TABLE t1;
SET INSERT_ID = 1;
}

-- echo # Simple test with NULL
eval CREATE TABLE t1 (
  c1 INT NOT NULL AUTO_INCREMENT,
  PRIMARY KEY (c1))
ENGINE=$engine
PARTITION BY HASH(c1)
PARTITIONS 2;
INSERT INTO t1 VALUES (NULL);
SHOW CREATE TABLE t1;
SELECT * FROM t1;
DROP TABLE t1;

-- echo # Test with sql_mode and first insert as 0
eval CREATE TABLE t1 (
  c1 INT,
  c2 INT NOT NULL AUTO_INCREMENT,
  PRIMARY KEY (c2))
ENGINE=$engine
PARTITION BY HASH(c2)
PARTITIONS 2;
INSERT INTO t1 VALUES (1, NULL);
-- error 0, ER_DUP_KEY, ER_DUP_ENTRY
INSERT INTO t1 VALUES (1, 1), (99, 99);
if (!$mysql_errno)
{
  echo # ERROR (only OK if Blackhole) should give ER_DUP_KEY or ER_DUP_ENTRY;
  echo # mysql_errno: $mysql_errno;
}
INSERT INTO t1 VALUES (1, NULL);
let $old_sql_mode = `select @@session.sql_mode`;
SET @@session.sql_mode = 'NO_AUTO_VALUE_ON_ZERO';
-- error 0, ER_DUP_ENTRY, ER_DUP_KEY
INSERT INTO t1 VALUES (1, 0);
if ($mysql_errno)
{
  echo # ERROR (only OK if Archive) mysql_errno: $mysql_errno;
}
SELECT * FROM t1 ORDER BY c1, c2;
DROP TABLE t1;
eval CREATE TABLE t1 (
  c1 INT,
  c2 INT NOT NULL AUTO_INCREMENT,
  PRIMARY KEY (c2))
ENGINE=$engine
PARTITION BY HASH(c2)
PARTITIONS 2;
-- error 0, ER_DUP_ENTRY, ER_DUP_KEY
INSERT INTO t1 VALUES (1, 0);
if ($mysql_errno)
{
  echo # ERROR (only OK if Archive) mysql_errno: $mysql_errno;
}
INSERT INTO t1 VALUES (1, 1), (1, NULL);
INSERT INTO t1 VALUES (2, NULL), (4, 7);
INSERT INTO t1 VALUES (1, NULL);
SELECT * FROM t1 ORDER BY c1, c2;
eval SET @@session.sql_mode = '$old_sql_mode';
DROP TABLE t1;


-- echo # Simple test with NULL, 0 and explicit values both incr. and desc.
eval CREATE TABLE t1 (
  c1 INT NOT NULL AUTO_INCREMENT,
  PRIMARY KEY (c1))
ENGINE=$engine
PARTITION BY HASH(c1)
PARTITIONS 2;
INSERT INTO t1 VALUES (2), (4), (NULL);
INSERT INTO t1 VALUES (0);
-- error 0, ER_DUP_KEY, ER_DUP_ENTRY
INSERT INTO t1 VALUES (5), (16);
if (!$mysql_errno)
{
  echo # ERROR (only OK if Blackhole) should give ER_DUP_KEY or ER_DUP_ENTRY;
  echo # mysql_errno: $mysql_errno;
}
INSERT INTO t1 VALUES (17), (19), (NULL);
-- error 0, ER_DUP_ENTRY, ER_DUP_KEY
INSERT INTO t1 VALUES (NULL), (10), (NULL);
if ($mysql_errno)
{
  echo # ERROR (only OK if Archive) mysql_errno: $mysql_errno;
}
-- error 0, ER_DUP_ENTRY, ER_DUP_KEY
INSERT INTO t1 VALUES (NULL), (9);
if ($mysql_errno)
{
  echo # ERROR (only OK if Archive) mysql_errno: $mysql_errno;
}
-- error 0, ER_DUP_ENTRY, ER_DUP_KEY
INSERT INTO t1 VALUES (59), (55);
if ($mysql_errno)
{
  echo # ERROR (only OK if Archive) mysql_errno: $mysql_errno;
}
INSERT INTO t1 VALUES (NULL), (90);
INSERT INTO t1 VALUES (NULL);
if (!$skip_update)
{
  UPDATE t1 SET c1 = 150 WHERE c1 = 17;
  UPDATE t1 SET c1 = 151 WHERE c1 = 19;
  FLUSH TABLES;
  UPDATE t1 SET c1 = 140 WHERE c1 = 150;
  SELECT AUTO_INCREMENT FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA='test'
  AND TABLE_NAME='t1';
  UPDATE t1 SET c1 = NULL WHERE c1 = 4;
  INSERT INTO t1 VALUES (NULL);
  INSERT INTO t1 VALUES (NULL);
}
SELECT * FROM t1 ORDER BY c1;
DROP TABLE t1;

-- echo # Test with auto_increment_increment and auto_increment_offset.
eval CREATE TABLE t1 (
  c1 INT NOT NULL AUTO_INCREMENT,
  PRIMARY KEY (c1))
ENGINE=$engine
PARTITION BY HASH(c1)
PARTITIONS 2;
let $old_increment = `SELECT @@session.auto_increment_increment`;
let $old_offset = `SELECT @@session.auto_increment_offset`;
SET @@session.auto_increment_increment = 10;
SET @@session.auto_increment_offset = 5;
INSERT INTO t1 VALUES (1);
INSERT INTO t1 VALUES (NULL), (NULL), (NULL);
SET @@session.auto_increment_increment = 5;
SET @@session.auto_increment_offset = 3;
INSERT INTO t1 VALUES (NULL);
let $new_val = `SELECT LAST_INSERT_ID()`;
eval INSERT INTO t1 VALUES ($new_val + 1);
INSERT INTO t1 VALUES (NULL);
let $new_val = `SELECT LAST_INSERT_ID()`;
eval INSERT INTO t1 VALUES ($new_val + 2);
INSERT INTO t1 VALUES (NULL);
let $new_val = `SELECT LAST_INSERT_ID()`;
eval INSERT INTO t1 VALUES ($new_val + 3);
INSERT INTO t1 VALUES (NULL);
let $new_val = `SELECT LAST_INSERT_ID()`;
eval INSERT INTO t1 VALUES ($new_val + 4);
INSERT INTO t1 VALUES (NULL);
let $new_val = `SELECT LAST_INSERT_ID()`;
eval INSERT INTO t1 VALUES ($new_val + 5);
INSERT INTO t1 VALUES (NULL);
let $new_val = `SELECT LAST_INSERT_ID()`;
eval INSERT INTO t1 VALUES ($new_val + 6);
INSERT INTO t1 VALUES (NULL);
eval SET @@session.auto_increment_increment = $old_increment;
eval SET @@session.auto_increment_offset = $old_offset;
SELECT * FROM t1 ORDER BY c1;
DROP TABLE t1;


-- echo # Test reported auto_increment value
eval CREATE TABLE t1 (
  c1 INT NOT NULL AUTO_INCREMENT,
  PRIMARY KEY (c1))
ENGINE=$engine
PARTITION BY HASH (c1)
PARTITIONS 2;
SELECT AUTO_INCREMENT FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA='test'
AND TABLE_NAME='t1';
INSERT INTO t1 VALUES (2);
SELECT AUTO_INCREMENT FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA='test'
AND TABLE_NAME='t1';
INSERT INTO t1 VALUES (4);
INSERT INTO t1 VALUES (NULL);
SELECT AUTO_INCREMENT FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA='test'
AND TABLE_NAME='t1';
INSERT INTO t1 VALUES (NULL);
INSERT INTO t1 VALUES (17);
INSERT INTO t1 VALUES (19);
INSERT INTO t1 VALUES (NULL);
INSERT INTO t1 VALUES (NULL);
SELECT AUTO_INCREMENT FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA='test'
AND TABLE_NAME='t1';
SELECT AUTO_INCREMENT FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA='test'
AND TABLE_NAME='t1';
-- error 0, ER_DUP_ENTRY, ER_DUP_KEY
INSERT INTO t1 VALUES (10);
if ($mysql_errno)
{
  echo # ERROR (only OK if Archive) mysql_errno: $mysql_errno;
}
SELECT * FROM t1 ORDER BY c1;
INSERT INTO t1 VALUES (NULL);
SELECT AUTO_INCREMENT FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA='test'
AND TABLE_NAME='t1';
INSERT INTO t1 VALUES (NULL);
-- error 0, ER_DUP_ENTRY, ER_DUP_KEY
INSERT INTO t1 VALUES (15);
if ($mysql_errno)
{
  echo # ERROR (only OK if Archive) mysql_errno: $mysql_errno;
}
INSERT INTO t1 VALUES (NULL);
SELECT * FROM t1 ORDER BY c1;
INSERT INTO t1 VALUES (NULL);
if (!$skip_delete)
{
DELETE FROM t1;
}
INSERT INTO t1 VALUES (NULL);
SHOW CREATE TABLE t1;
SELECT * FROM t1 ORDER BY c1;
if (!$skip_truncate)
{
TRUNCATE TABLE t1;
}
INSERT INTO t1 VALUES (NULL);
SHOW CREATE TABLE t1;
SELECT * FROM t1 ORDER BY c1;
INSERT INTO t1 VALUES (100);
INSERT INTO t1 VALUES (NULL);
if (!$skip_delete)
{
DELETE FROM t1 WHERE c1 >= 100;
}
# InnoDB does reset auto_increment on OPTIMIZE, Bug#18274
OPTIMIZE TABLE t1;
SHOW CREATE TABLE t1;
DROP TABLE t1;

-- echo # Test with two threads
connection default;
-- echo # con default
eval CREATE TABLE t1 (c1 INT NOT NULL AUTO_INCREMENT, PRIMARY KEY (c1))
  ENGINE = $engine
  PARTITION BY HASH(c1)
  PARTITIONS 2;
INSERT INTO t1 (c1) VALUES (2);
INSERT INTO t1 (c1) VALUES (4);
connect(con1, localhost, root,,);
connection con1;
-- echo # con1
INSERT INTO t1 (c1) VALUES (NULL);
INSERT INTO t1 (c1) VALUES (10);
connection default;
-- echo # con default
INSERT INTO t1 (c1) VALUES (NULL);
INSERT INTO t1 (c1) VALUES (NULL);
INSERT INTO t1 (c1) VALUES (19);
INSERT INTO t1 (c1) VALUES (21);
-- echo # con1
connection con1;
INSERT INTO t1 (c1) VALUES (NULL);
connection default;
-- echo # con default
-- error 0, ER_DUP_ENTRY, ER_DUP_KEY
INSERT INTO t1 (c1) VALUES (16);
if ($mysql_errno)
{
  echo # ERROR (only OK if Archive) mysql_errno: $mysql_errno;
}
-- echo # con1
connection con1;
INSERT INTO t1 (c1) VALUES (NULL);
disconnect con1;
connection default;
-- echo # con default
INSERT INTO t1 (c1) VALUES (NULL);
SELECT * FROM t1 ORDER BY c1;
DROP TABLE t1;

-- echo # Test with two threads + start transaction NO PARTITIONING
connect(con1, localhost, root,,);
connection default;
-- echo # con default
eval CREATE TABLE t1 (c1 INT NOT NULL AUTO_INCREMENT, PRIMARY KEY (c1))
  ENGINE = $engine;
START TRANSACTION;
INSERT INTO t1 (c1) VALUES (2);
INSERT INTO t1 (c1) VALUES (4);
connection con1;
-- echo # con1
START TRANSACTION;
INSERT INTO t1 (c1) VALUES (NULL);
INSERT INTO t1 (c1) VALUES (10);
connection default;
-- echo # con default
INSERT INTO t1 (c1) VALUES (NULL);
INSERT INTO t1 (c1) VALUES (NULL);
INSERT INTO t1 (c1) VALUES (19);
INSERT INTO t1 (c1) VALUES (21);
-- echo # con1
connection con1;
INSERT INTO t1 (c1) VALUES (NULL);
connection default;
-- echo # con default
-- error 0, ER_DUP_KEY
INSERT INTO t1 (c1) VALUES (16);
if ($mysql_errno)
{
  echo # ERROR (only OK if Archive) mysql_errno: $mysql_errno;
}
-- echo # con1
connection con1;
INSERT INTO t1 (c1) VALUES (NULL);
SELECT * FROM t1 ORDER BY c1;
COMMIT;
SELECT * FROM t1 ORDER BY c1;
disconnect con1;
connection default;
-- echo # con default
INSERT INTO t1 (c1) VALUES (NULL);
SELECT * FROM t1 ORDER BY c1;
COMMIT;
SELECT * FROM t1 ORDER BY c1;
DROP TABLE t1;

-- echo # Test with two threads + start transaction
connect(con1, localhost, root,,);
connection default;
-- echo # con default
eval CREATE TABLE t1 (c1 INT NOT NULL AUTO_INCREMENT, PRIMARY KEY (c1))
  ENGINE = $engine
  PARTITION BY HASH(c1)
  PARTITIONS 2;
START TRANSACTION;
INSERT INTO t1 (c1) VALUES (2);
INSERT INTO t1 (c1) VALUES (4);
connection con1;
-- echo # con1
START TRANSACTION;
INSERT INTO t1 (c1) VALUES (NULL), (10);
connection default;
-- echo # con default
INSERT INTO t1 (c1) VALUES (NULL), (NULL), (19);
INSERT INTO t1 (c1) VALUES (21);
-- echo # con1
connection con1;
INSERT INTO t1 (c1) VALUES (NULL);
connection default;
-- echo # con default
-- error 0, ER_DUP_ENTRY, ER_DUP_KEY
INSERT INTO t1 (c1) VALUES (16);
if ($mysql_errno)
{
  echo # ERROR (only OK if Archive) mysql_errno: $mysql_errno;
}
-- echo # con1
connection con1;
INSERT INTO t1 (c1) VALUES (NULL);
SELECT * FROM t1 ORDER BY c1;
COMMIT;
SELECT * FROM t1 ORDER BY c1;
disconnect con1;
connection default;
-- echo # con default
INSERT INTO t1 (c1) VALUES (NULL);
SELECT * FROM t1 ORDER BY c1;
COMMIT;
SELECT * FROM t1 ORDER BY c1;
DROP TABLE t1;

if (!$only_ai_pk)
{
-- echo # Test with another column after
eval CREATE TABLE t1 (
c1 INT NOT NULL AUTO_INCREMENT,
c2 INT,
PRIMARY KEY (c1,c2))
ENGINE = $engine
PARTITION BY HASH(c2)
PARTITIONS 2;
INSERT INTO t1 VALUES (1, 0);
INSERT INTO t1 VALUES (1, 1);
INSERT INTO t1 VALUES (NULL, 1), (NULL, 2), (NULL, 3);
INSERT INTO t1 VALUES (NULL, 3);
INSERT INTO t1 VALUES (2, 0), (NULL, 2);
INSERT INTO t1 VALUES (2, 2);
INSERT INTO t1 VALUES (2, 22);
INSERT INTO t1 VALUES (NULL, 2);
SELECT * FROM t1 ORDER BY c1,c2;
DROP TABLE t1;
}

-- echo # Test with another column before
eval CREATE TABLE t1 (
c1 INT,
c2 INT NOT NULL AUTO_INCREMENT,
PRIMARY KEY (c2))
ENGINE = $engine
PARTITION BY HASH(c2)
PARTITIONS 2;
INSERT INTO t1 VALUES (1, 0);
-- error 0, ER_DUP_KEY, ER_DUP_ENTRY
INSERT INTO t1 VALUES (1, 1);
if (!$mysql_errno)
{
  echo # ERROR (only OK if Blackhole) should give ER_DUP_KEY or ER_DUP_ENTRY;
  echo # mysql_errno: $mysql_errno;
}
INSERT INTO t1 VALUES (1, NULL);
INSERT INTO t1 VALUES (2, NULL), (3, 11), (3, NULL), (2, 0);
INSERT INTO t1 VALUES (2, NULL);
-- error 0, ER_DUP_KEY, ER_DUP_ENTRY
INSERT INTO t1 VALUES (2, 2);
if (!$mysql_errno)
{
  echo # ERROR (only OK if Blackhole) should give ER_DUP_KEY or ER_DUP_ENTRY;
  echo # mysql_errno: $mysql_errno;
}
INSERT INTO t1 VALUES (2, 22);
INSERT INTO t1 VALUES (2, NULL);
SELECT * FROM t1 ORDER BY c1,c2;
DROP TABLE t1;

-- echo # Test with auto_increment on secondary column in multi-column-index
-- disable_abort_on_error
eval CREATE TABLE t1 (
c1 INT,
c2 INT NOT NULL AUTO_INCREMENT,
PRIMARY KEY (c1,c2))
ENGINE = $engine
PARTITION BY HASH(c2)
PARTITIONS 2;
-- enable_abort_on_error
-- disable_query_log
eval SET @my_errno= $mysql_errno ;
let $run = `SELECT @my_errno = 0`;
# ER_WRONG_AUTO_KEY is 1075
let $ER_WRONG_AUTO_KEY= 1075;
if (`SELECT @my_errno NOT IN (0,$ER_WRONG_AUTO_KEY)`)
{
  -- echo # Unknown error code, exits
  exit;
}
-- enable_query_log
if ($run)
{
INSERT INTO t1 VALUES (1, 0);
-- error 0, ER_DUP_KEY, ER_DUP_ENTRY
INSERT INTO t1 VALUES (1, 1);
if (!$mysql_errno)
{
  echo # ERROR (only OK if Blackhole) should give ER_DUP_KEY or ER_DUP_ENTRY;
  echo # mysql_errno: $mysql_errno;
}
INSERT INTO t1 VALUES (1, NULL);
INSERT INTO t1 VALUES (2, NULL);
INSERT INTO t1 VALUES (3, NULL);
INSERT INTO t1 VALUES (3, NULL), (2, 0), (2, NULL);
-- error 0, ER_DUP_ENTRY, ER_DUP_KEY
INSERT INTO t1 VALUES (2, 2);
if (!$mysql_errno)
{
  echo # ERROR (only OK if Blackhole/NDB) should give ER_DUP_KEY or ER_DUP_ENTRY;
  echo # mysql_errno: $mysql_errno;
}
INSERT INTO t1 VALUES (2, 22), (2, NULL);
SELECT * FROM t1 ORDER BY c1,c2;
DROP TABLE t1;
}

-- echo # Test AUTO_INCREMENT in CREATE
eval CREATE TABLE t1 (c1 INT NOT NULL AUTO_INCREMENT, PRIMARY KEY (c1))
  ENGINE = $engine
  AUTO_INCREMENT = 15
  PARTITION BY HASH(c1)
  PARTITIONS 2;
SHOW CREATE TABLE t1;
-- error 0, ER_DUP_ENTRY, ER_DUP_KEY
INSERT INTO t1 (c1) VALUES (4);
if ($mysql_errno)
{
  echo # ERROR (only OK if Archive) mysql_errno: $mysql_errno;
}
SHOW CREATE TABLE t1;
INSERT INTO t1 (c1) VALUES (0);
SHOW CREATE TABLE t1;
INSERT INTO t1 (c1) VALUES (NULL);
SHOW CREATE TABLE t1;
SELECT * FROM t1 ORDER BY c1;

-- echo # Test sql_mode 'NO_AUTO_VALUE_ON_ZERO'
let $old_sql_mode = `select @@session.sql_mode`;
SET @@session.sql_mode = 'NO_AUTO_VALUE_ON_ZERO';
INSERT INTO t1 (c1) VALUES (300);
SHOW CREATE TABLE t1;
-- error 0, ER_DUP_ENTRY, ER_DUP_KEY
INSERT INTO t1 (c1) VALUES (0);
if ($mysql_errno)
{
  echo # ERROR (only OK if Archive) mysql_errno: $mysql_errno;
}
SHOW CREATE TABLE t1;
INSERT INTO t1 (c1) VALUES (NULL);
SHOW CREATE TABLE t1;
SELECT * FROM t1 ORDER BY c1;
eval SET @@session.sql_mode = '$old_sql_mode';
DROP TABLE t1;

-- echo # Test SET INSERT_ID
eval CREATE TABLE t1 (c1 INT NOT NULL AUTO_INCREMENT, PRIMARY KEY (c1))
  ENGINE = $engine
  PARTITION BY HASH(c1)
  PARTITIONS 2;
SHOW CREATE TABLE t1;
INSERT INTO t1 (c1) VALUES (NULL);
SHOW CREATE TABLE t1;
SELECT * FROM t1;
SET INSERT_ID = 23;
SHOW CREATE TABLE t1;
INSERT INTO t1 (c1) VALUES (NULL);
SHOW CREATE TABLE t1;
SET INSERT_ID = 22;
-- error 0, ER_DUP_ENTRY, ER_DUP_KEY
INSERT INTO t1 VALUES (NULL), (NULL), (NULL);
if (!$mysql_errno)
{
  echo # ERROR (only OK if Blackhole) should give ER_DUP_KEY or ER_DUP_ENTRY;
  echo # mysql_errno: $mysql_errno;
}
INSERT INTO t1 VALUES (NULL);
SELECT * FROM t1 ORDER BY c1;
DROP TABLE t1;

-- echo # Testing with FLUSH TABLE
eval CREATE TABLE t1 (
  c1 INT NOT NULL AUTO_INCREMENT,
  PRIMARY KEY (c1))
  ENGINE=$engine
  PARTITION BY HASH(c1)
  PARTITIONS 2;
SHOW CREATE TABLE t1;
FLUSH TABLE;
SHOW CREATE TABLE t1;
INSERT INTO t1 VALUES (4);
FLUSH TABLE;
SHOW CREATE TABLE t1;
INSERT INTO t1 VALUES (NULL);
FLUSH TABLE;
SHOW CREATE TABLE t1;
SELECT * FROM t1 ORDER BY c1;
DROP TABLE t1;

if (!$skip_negative_auto_inc)
{
--echo #############################################################################
--echo # Bug #45823 - Assertion failure in file row/row0mysql.c line 1386
--echo # Bug #43988 - AUTO_INCREMENT errors with partitioned InnoDB tables in 5.1.31
--echo ##############################################################################

--echo # Inserting negative autoincrement values into a partition table (partitions >= 4)

eval CREATE TABLE t (c1 INT NOT NULL AUTO_INCREMENT, PRIMARY KEY(c1),
  c2 INT) ENGINE=$engine PARTITION BY HASH(c1) PARTITIONS 4;

INSERT INTO t(c2) VALUES (10);
INSERT INTO t(c2) VALUES (20);
--error 0, ER_DUP_KEY
INSERT INTO t VALUES (-1,-10);
if ($mysql_errno)
{
  echo # ERROR (only OK if Archive), mysql_errno: $mysql_errno;
}
INSERT INTO t(c2) VALUES (30);
INSERT INTO t(c2) VALUES (40);

SELECT * FROM t ORDER BY c1 ASC;

DROP TABLE t;

--echo # Reading from a partition table (partitions >= 2 ) after inserting a negative
--echo # value into the auto increment column


eval CREATE TABLE t (c1 INT NOT NULL AUTO_INCREMENT, PRIMARY KEY(c1),
  c2 INT) ENGINE=$engine PARTITION BY HASH(c1) PARTITIONS 2;

--error 0, ER_DUP_KEY
INSERT INTO t VALUES (-2,-20);
if ($mysql_errno)
{
  echo # ERROR (only OK if Archive), mysql_errno: $mysql_errno;
}
INSERT INTO t(c2) VALUES (30);

SELECT * FROM t ORDER BY c1 ASC;

DROP TABLE t;

--echo # Inserting negative auto increment value into a partition table (partitions >= 2)
--echo # auto increment value > 2.

eval CREATE TABLE t (c1 INT NOT NULL AUTO_INCREMENT, PRIMARY KEY(c1),
  c2 INT) ENGINE=$engine PARTITION BY HASH(c1) PARTITIONS 2;

--error 0, ER_DUP_KEY
INSERT INTO t VALUES (-4,-20);
if ($mysql_errno)
{
  echo # ERROR (only OK if Archive), mysql_errno: $mysql_errno;
}
INSERT INTO t(c2) VALUES (30);
INSERT INTO t(c2) VALUES (40);

SELECT * FROM t ORDER BY c1 ASC;

DROP TABLE t;

--echo # Inserting -1 into autoincrement column of a partition table (partition >= 4)

eval CREATE TABLE t (c1 INT NOT NULL AUTO_INCREMENT, PRIMARY KEY(c1),
  c2 INT) ENGINE=$engine PARTITION BY HASH(c1) PARTITIONS 4;

INSERT INTO t(c2) VALUES (10);
INSERT INTO t(c2) VALUES (20);
--error 0, ER_DUP_KEY
INSERT INTO t VALUES (-1,-10);
if ($mysql_errno)
{
  echo # ERROR (only OK if Archive), mysql_errno: $mysql_errno;
}

SELECT * FROM t ORDER BY c1 ASC;

INSERT INTO t(c2) VALUES (30);

SELECT * FROM t ORDER BY c1 ASC;

DROP TABLE t;

--echo # Deleting from an auto increment table after inserting negative values

eval CREATE TABLE t (c1 INT NOT NULL AUTO_INCREMENT, PRIMARY KEY(c1),
  c2 INT) ENGINE=$engine PARTITION BY HASH(c1) PARTITIONS 4; 

INSERT INTO t(c2) VALUES (10);
INSERT INTO t(c2) VALUES (20);
--error 0, ER_DUP_KEY
INSERT INTO t VALUES (-1,-10);
if ($mysql_errno)
{
  echo # ERROR (only OK if Archive), mysql_errno: $mysql_errno;
}
INSERT INTO t(c2) VALUES (30);
--error 0, ER_DUP_KEY
INSERT INTO t VALUES (-3,-20);
if ($mysql_errno)
{
  echo # ERROR (only OK if Archive), mysql_errno: $mysql_errno;
}
INSERT INTO t(c2) VALUES (40);

SELECT * FROM t ORDER BY c1 ASC;

if (!$skip_delete)
{ 
DELETE FROM t WHERE c1 > 1;
}

SELECT * FROM t ORDER BY c1 ASC;

DROP TABLE t;

--echo # Inserting a positive value that exceeds maximum allowed value for an
--echo # Auto Increment column (positive maximum)

eval CREATE TABLE t (c1 TINYINT NOT NULL AUTO_INCREMENT, PRIMARY KEY(c1),
  c2 INT) ENGINE=$engine PARTITION BY HASH(c1) PARTITIONS 4;

INSERT INTO t(c2) VALUES (10);
INSERT INTO t(c2) VALUES (20);
INSERT INTO t VALUES (126,30);
INSERT INTO t VALUES (127,40);

--error 0, ER_DUP_ENTRY, ER_DUP_KEY
INSERT INTO t VALUES (128,50);
if (!$mysql_errno)
{
  echo # ERROR (Only OK if Blackhole);
  echo # expecting ER_DUP_ENTRY or ER_DUP_KEY, mysql_errno: $mysql_errno;
}
--error 0, ER_DUP_ENTRY, ER_DUP_KEY
INSERT INTO t VALUES (129,60);
if (!$mysql_errno)
{
  echo # ERROR (only OK if Blackhole);
  echo # expecting ER_DUP_ENTRY or ER_DUP_KEY, mysql_errno: $mysql_errno;
}

SELECT * FROM t ORDER BY c1 ASC;

DROP TABLE t;

--echo # Inserting a negative value that goes below minimum allowed value for an
--echo # Auto Increment column (negative minimum)

eval CREATE TABLE t (c1 TINYINT NOT NULL AUTO_INCREMENT, PRIMARY KEY(c1),
  c2 INT) ENGINE=$engine PARTITION BY HASH(c1) PARTITIONS 4;

INSERT INTO t(c2) VALUES (10);
INSERT INTO t(c2) VALUES (20);
--error 0, ER_DUP_KEY
INSERT INTO t VALUES (-127,30);
if ($mysql_errno)
{
  echo # ERROR (only OK if Archive), mysql_errno: $mysql_errno;
}
--error 0, ER_DUP_KEY
INSERT INTO t VALUES (-128,40);
if ($mysql_errno)
{
  echo # ERROR (only OK if Archive), mysql_errno: $mysql_errno;
}

--error 0, ER_DUP_ENTRY, ER_DUP_KEY
INSERT INTO t VALUES (-129,50);
if (!$mysql_errno)
{
  echo # ERROR (only OK if Blackhole)
  echo # should give ER_DUP_KEY or ER_DUP_ENTRY mysql_errno: $mysql_errno;
}
--error 0, ER_DUP_ENTRY, ER_DUP_KEY
INSERT INTO t VALUES (-130,60);
if (!$mysql_errno)
{
  echo # ERROR (only OK if Blackhole)
  echo # should give ER_DUP_KEY or ER_DUP_ENTRY mysql_errno: $mysql_errno;
}

SELECT * FROM t ORDER BY c1 ASC;

DROP TABLE t;

--echo # Updating the partition table with a negative Auto Increment value

eval CREATE TABLE t (c1 INT NOT NULL AUTO_INCREMENT, PRIMARY KEY(c1),
  c2 INT) ENGINE=$engine PARTITION BY HASH(c1) PARTITIONS 4;

INSERT INTO t(c2) VALUES (10);
INSERT INTO t(c2) VALUES (20);
--error 0, ER_DUP_KEY
INSERT INTO t VALUES (-1,-10);
if ($mysql_errno)
{
  echo # ERROR (only OK if Archive), mysql_errno: $mysql_errno;
}
INSERT INTO t(c2) VALUES (30);

SELECT * FROM t ORDER BY c1 ASC;

if (!$skip_update)
{ 
UPDATE t SET c1 = -6 WHERE c1 = 2;
}

SELECT * FROM t ORDER BY c1 ASC;

INSERT INTO t(c2) VALUES (40);
INSERT INTO t(c2) VALUES (50);

if (!$skip_update)
{ 
UPDATE t SET c1 = -6 WHERE c1 = 2;
}

SELECT * FROM t ORDER BY c1 ASC;

DROP TABLE t;

--echo # Updating the partition table with a value that crosses the upper limits
--echo # on both the positive and the negative side.

eval CREATE TABLE t (c1 TINYINT NOT NULL AUTO_INCREMENT, PRIMARY KEY(c1),
  c2 INT) ENGINE=$engine PARTITION BY HASH(c1) PARTITIONS 4;

INSERT INTO t(c2) VALUES (10);
INSERT INTO t(c2) VALUES (20);
INSERT INTO t VALUES (126,30);
INSERT INTO t VALUES (127,40);

SELECT * FROM t ORDER BY c1 ASC;

if (!$skip_update)
{ 
UPDATE t SET c1 = 130 where c1 = 127;
}

SELECT * FROM t ORDER BY c1 ASC;

if (!$skip_update)
{ 
UPDATE t SET c1 = -140 where c1 = 126;
}

SELECT * FROM t ORDER BY c1 ASC;

DROP TABLE t;

if (!$skip_update)
{ 
eval CREATE TABLE t1
(a INT NULL AUTO_INCREMENT,
 UNIQUE KEY (a))
ENGINE=$engine
PARTITION BY KEY(a) PARTITIONS 2;
SET LAST_INSERT_ID = 999;
SET INSERT_ID = 0;
INSERT INTO t1 SET a = 1 ON DUPLICATE KEY UPDATE a = NULL;
SELECT LAST_INSERT_ID();
SELECT * FROM t1;
INSERT INTO t1 SET a = 1 ON DUPLICATE KEY UPDATE a = NULL;
SELECT LAST_INSERT_ID();
SELECT * FROM t1;
UPDATE t1 SET a = 1 WHERE a IS NULL;
SELECT LAST_INSERT_ID();
SELECT * FROM t1;
UPDATE t1 SET a = NULL WHERE a = 1;
SELECT LAST_INSERT_ID();
SELECT * FROM t1;
DROP TABLE t1;
}


--echo ##############################################################################
}
