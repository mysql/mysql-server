#
# Bug#22653180:ASSERT DD::CACHE::SHARED_MULTI_MAP<T>::
# PUT(CONST K*, CONST T*, DD::CACHE::CACHE
#
CREATE TABLE t1(i int);
# Force rename_table to fail after update has been applied to dd cache
SET SESSION debug="+d,abort_rename_after_update";
# rename table will fail with dummy error
RENAME TABLE t1 to t2;
ERROR HY000: Error writing file 'error inject' (errno: 42 - simulated write error)
SET SESSION debug="-d,abort_rename_after_update";
SELECT * FROM t1;
i
DROP TABLE t1;
#
# Part of test coverage for WL#9826 "Allow RENAME TABLES under
# LOCK TABLES" which needs debug build and debug_sync facility.
#
# The main part of coverage for this WL resides in rename.test.
# This file only contains subtests which require debug/debug_sync
# facilities, hence their odd numbering.
SET @old_lock_wait_timeout= @@lock_wait_timeout;
connect  con1, localhost, root,,;
SET @old_lock_wait_timeout= @@lock_wait_timeout;
connection default;
#
# 1) Requirements on table locking for tables renamed and
#    target table names.
#
#
# 1.2) Locking of target table.
CREATE TABLE t1 (i INT);
CREATE TABLE t2 (j INT);
# RENAME TABLE under LOCK TABLES acquires X metadata
# lock on target table name.
connection con1;
# Ensure that table is cached in Table and Table Definition Caches.
SELECT * FROM t1;
i
SET DEBUG_SYNC='open_tables_after_open_and_process_table SIGNAL opened WAIT_FOR go';
SHOW CREATE TABLE t1;
connection default;
# Wait until SHOW CREATE TABLE acquires SH MDL on t1 and
# starts waiting.
SET DEBUG_SYNC='now WAIT_FOR opened';
LOCK TABLES t2 WRITE;
# RENAME TABLE fails due to lock timeout since it tries
# to acquire X metadata lock on t1, on which SH metadata
# lock is held in con1.
SET @@lock_wait_timeout= 1;
RENAME TABLE t2 TO t1;
ERROR HY000: Lock wait timeout exceeded; try restarting transaction
SET @@lock_wait_timeout= @old_lock_wait_timeout;
UNLOCK TABLES;
#
# 2) Failure to acquire/upgrade locks on tables involved.
#
# Failure to upgrade metadata lock on source table to X mode.
# (note that con1 still holds SH lock on it).
LOCK TABLE t1 WRITE;
SET @@lock_wait_timeout= 1;
RENAME TABLE t1 TO t3;
ERROR HY000: Lock wait timeout exceeded; try restarting transaction
SET @@lock_wait_timeout= @old_lock_wait_timeout;
UNLOCK TABLES;
#
# Failure to acquire X metadata lock on target table name.
LOCK TABLES t2 WRITE;
SET @@lock_wait_timeout= 1;
RENAME TABLE t2 TO t1;
ERROR HY000: Lock wait timeout exceeded; try restarting transaction
UNLOCK TABLES;
# Unblock and reap SHOW CREATE TABLE.
SET DEBUG_SYNC='now SIGNAL go';
connection con1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `i` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
connection default;
SET DEBUG_SYNC='RESET';
DROP TABLES t1, t2;
#
# 5) RENAME TABLES under LOCK TABLES and views.
#
# 5.1) Requirements on locking is similar to tables.
CREATE TABLE t1 (i INT);
CREATE TABLE t2 (j INT);
CREATE VIEW v1 AS SELECT * FROM t1;
CREATE VIEW v2 AS SELECT * FROM t2;
#
# RENAME TABLE on view under LOCK TABLES acquires X metadata
# lock on target name.
connection con1;
SET DEBUG_SYNC='open_tables_after_open_and_process_table SIGNAL opened WAIT_FOR go';
SHOW CREATE VIEW v2;
connection default;
# Wait until SHOW CREATE VIEW acquires SH MDL on v2 and
# starts waiting.
SET DEBUG_SYNC='now WAIT_FOR opened';
LOCK TABLES v1 WRITE;
# RENAME TABLE fails due to lock timeout since it tries
# to acquire X metadata lock on v2, on which SH metadata
# lock is held in con1.
SET @@lock_wait_timeout= 1;
RENAME TABLE v1 TO v2;
ERROR HY000: Lock wait timeout exceeded; try restarting transaction
SET @@lock_wait_timeout= @old_lock_wait_timeout;
UNLOCK TABLES;
# Unblock and reap SHOW CREATE VIEW.
SET DEBUG_SYNC='now SIGNAL go';
connection con1;
View	Create View	character_set_client	collation_connection
v2	CREATE ALGORITHM=UNDEFINED DEFINER=`root`@`localhost` SQL SECURITY DEFINER VIEW `v2` AS select `t2`.`j` AS `j` from `t2`	utf8mb4	utf8mb4_0900_ai_ci
connection default;
SET DEBUG_SYNC='RESET';
DROP VIEW v1;
DROP VIEW v2;
DROP TABLES t1, t2;
# Clean-up.
connection con1;
disconnect con1;
connection default;
