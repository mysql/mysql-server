#
# Bug#18913551 - LOCK TABLES USES INCORRECT LOCK FOR IMPLICITLY USED
#                TABLES.
#
SET @org_concurrent_insert= @@global.concurrent_insert;
SET @@global.concurrent_insert=1;
CREATE TABLE t1(a INT) ENGINE=MyISAM;
CREATE FUNCTION f1() RETURNS INT RETURN (SELECT MIN(a) FROM t1);
CREATE VIEW v1 AS (SELECT 1 FROM dual WHERE f1() = 1);
LOCK TABLE v1 READ;
connect  con1, localhost, root;
SET lock_wait_timeout=1;
# With fix, con1 does not get lock on table "t1" so following insert
# operation fails after waiting for "lock_wait_timeout" duration.
INSERT INTO t1 VALUES (1);
ERROR HY000: Lock wait timeout exceeded; try restarting transaction
connection default;
UNLOCK TABLES;
# V1 should be empty here.
SELECT * FROM v1;
1
disconnect con1;
SET @@global.concurrent_insert= @org_concurrent_insert;
DROP TABLE t1;
DROP VIEW v1;
DROP FUNCTION f1;
