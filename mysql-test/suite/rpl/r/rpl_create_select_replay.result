RESET MASTER;
#
# Case 1: Replay DDL_CTAS without rows, crash before commit;
#
CREATE TABLE t0 (f1 INT);
CREATE TABLE t1 AS SELECT * FROM t0;
# Dump the binlog events into a file.
include/mysqlbinlog.inc
# Replay without crash.
DROP TABLE t0,t1;
RESET MASTER;
include/assert.inc [Verify two tables (t0 & t1) are present in test database]
include/assert.inc [Verify test.t1 is synced]
DROP TABLE t0,t1;
RESET MASTER;
# Replay the binlog events and cause crash before commit;
SET GLOBAL DEBUG='+d, crash_on_transactional_ddl_commit';
# Recover the server.
include/assert.inc [Verify that only table t0 is present in test database]
DROP TABLE t0;
RESET MASTER;
SET global DEBUG='-d, crash_on_transactional_ddl_commit';
RESET MASTER;
#
# Case 2: Replay DDL_CTAS with rows, crash before commit;
#
CREATE TABLE t0 (f1 INT);
INSERT INTO t0 VALUES (1);
INSERT INTO t0 VALUES (2);
INSERT INTO t0 VALUES (3);
INSERT INTO t0 VALUES (4);
CREATE TABLE t1 AS SELECT * FROM t0;
# Dump the binlog events into a file.
include/mysqlbinlog.inc
# Replay without crash.
DROP TABLE t0,t1;
RESET MASTER;
include/assert.inc [Verify two tables (t0 & t1) are present in test database]
include/assert.inc [Verify test.t1 is synced]
DROP TABLE t0,t1;
RESET MASTER;
# Replay the binlog events, cause crash before commit and restart;
SET GLOBAL DEBUG='+d, crash_on_transactional_ddl_commit';
# Recover the server.
SET global DEBUG='-d, crash_on_transactional_ddl_commit';
include/assert.inc [Verify that only table t0 is present in test database]
DROP TABLE t0;
#
# Case 3: Replay DDL_CTAS with rows and then force rollback.
#
RESET MASTER;
SET GLOBAL DEBUG='+d, force_rollback_in_replica_on_transactional_ddl_commit';
SET global DEBUG='-d, force_rollback_in_replica_on_transactional_ddl_commit';
include/assert.inc [Verify that only table t0 is present in test database]
DROP TABLE t0;
RESET MASTER;
#
# Case 4: Replay DDL_CTAS in a SP without rows, crash before commit;
#
CREATE TABLE t0 (f1 INT);
CREATE PROCEDURE proc1()
BEGIN
CREATE TABLE t1 AS SELECT * FROM t0;
END|
CALL proc1();
# Dump the binlog events into a file.
include/mysqlbinlog.inc
# Replay without crash.
DROP TABLE t0,t1;
DROP PROCEDURE proc1;
RESET MASTER;
include/assert.inc [Verify two tables (t0 & t1) are present in test database]
# Drop existing objects.
DROP TABLE t0,t1;
DROP PROCEDURE proc1;
RESET MASTER;
# Replay the binlog events and cause crash.
SET GLOBAL DEBUG='+d, crash_on_transactional_ddl_commit';
# Recover the server.
include/assert.inc [Verify that only table t0 is present in test database]
DROP TABLE t0;
DROP PROCEDURE proc1;
RESET MASTER;
SET global DEBUG='-d, crash_on_transactional_ddl_commit';
RESET MASTER;
#
# Case 5: Replay DDL_CTAS in a SP with rows, crash before commit;
#
CREATE TABLE t0 (f1 INT);
INSERT INTO t0 VALUES (1);
INSERT INTO t0 VALUES (2);
INSERT INTO t0 VALUES (3);
INSERT INTO t0 VALUES (4);
CREATE PROCEDURE proc1()
BEGIN
CREATE TABLE t1 AS SELECT * FROM t0;
END|
CALL proc1();
# Dump the binlog events into a file.
include/mysqlbinlog.inc
# Replay without crash.
DROP TABLE t0,t1;
DROP PROCEDURE proc1;
RESET MASTER;
include/assert.inc [Verify two tables (t0 & t1) are present in test database]
include/assert.inc [Verify test.t1 is synced]
# Drop existing objects.
DROP TABLE t0,t1;
DROP PROCEDURE proc1;
RESET MASTER;
# Replay the binlog events and cause crash.
SET GLOBAL DEBUG='+d, crash_on_transactional_ddl_commit';
# Recover the server.
include/assert.inc [Verify that only table t0 is present in test database]
DROP TABLE t0;
DROP PROCEDURE proc1;
RESET MASTER;
SET global DEBUG='-d, crash_on_transactional_ddl_commit';
RESET MASTER;
#
# Case 6: Replay DDL_CTAS using PS with rows, crash before commit;
#
CREATE TABLE t0 (f1 INT);
INSERT INTO t0 VALUES (1);
INSERT INTO t0 VALUES (2);
INSERT INTO t0 VALUES (3);
INSERT INTO t0 VALUES (4);
PREPARE stmt FROM "CREATE TABLE t1 AS SELECT * FROM t0";
EXECUTE stmt;
DEALLOCATE PREPARE stmt;
# Dump the binlog events into a file.
include/mysqlbinlog.inc
# Replay without crash.
DROP TABLE t0,t1;
RESET MASTER;
include/assert.inc [Verify two tables (t0 & t1) are present in test database]
include/assert.inc [Verify test.t1 is synced]
# Drop existing objects.
DROP TABLE t0,t1;
RESET MASTER;
SET GLOBAL DEBUG='+d, crash_on_transactional_ddl_commit';
# Replay the binlog events and cause crash.
# Recover the server.
include/assert.inc [Verify that only table t0 is present in test database]
DROP TABLE t0;
RESET MASTER;
