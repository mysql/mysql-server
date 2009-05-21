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
if (!$skip_update)
{
# InnoDB Does not handle this correctly, see bug#14793, bug#21641
  UPDATE t1 SET c1 = 50 WHERE c1 = 17;
  UPDATE t1 SET c1 = 51 WHERE c1 = 19;
  FLUSH TABLES;
  UPDATE t1 SET c1 = 40 WHERE c1 = 50;
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

