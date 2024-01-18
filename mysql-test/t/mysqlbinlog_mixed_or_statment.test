--source include/force_myisam_default.inc
--source include/have_myisam.inc

--source include/rpl/force_binlog_format_statement.inc
# Save the initial number of concurrent sessions
--source include/count_sessions.inc

--echo #
--echo # Bug#18913551 - LOCK TABLES USES INCORRECT LOCK FOR IMPLICITLY USED
--echo #                TABLES.
--echo #

--enable_connect_log

SET @org_concurrent_insert= @@global.concurrent_insert;
SET @@global.concurrent_insert=1;

CREATE TABLE t1(a INT) ENGINE=MyISAM;
CREATE FUNCTION f1() RETURNS INT RETURN (SELECT MIN(a) FROM t1);
CREATE VIEW v1 AS (SELECT 1 FROM dual WHERE f1() = 1);

LOCK TABLE v1 READ;

connect (con1, localhost, root);
SET lock_wait_timeout=1;
--echo # With fix, con1 does not get lock on table "t1" so following insert
--echo # operation fails after waiting for "lock_wait_timeout" duration.
--error ER_LOCK_WAIT_TIMEOUT
INSERT INTO t1 VALUES (1);

connection default;
UNLOCK TABLES;

--echo # V1 should be empty here.
SELECT * FROM v1;

# Cleanup
disconnect con1;
SET @@global.concurrent_insert= @org_concurrent_insert;
DROP TABLE t1;
DROP VIEW v1;
DROP FUNCTION f1;
--disable_connect_log

#Wait till all disconnects are completed
--source include/wait_until_count_sessions.inc

--source include/rpl/restore_default_binlog_format.inc
