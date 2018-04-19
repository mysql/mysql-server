SET DEBUG_SYNC='RESET';
DROP TABLE IF EXISTS t1;
#
# Bug#20667 - Truncate table fails for a write locked table
#
CREATE TABLE t1 (c1 INT);
INSERT INTO t1 VALUES (1);
#
# connection con1
HANDLER t1 OPEN;
#
# connection default
LOCK TABLE t1 WRITE;
SET DEBUG_SYNC='mdl_upgrade_lock SIGNAL waiting';
TRUNCATE TABLE t1;
#
# connection con2
SET DEBUG_SYNC='now WAIT_FOR waiting';
KILL QUERY @id;
#
# connection default
ERROR 70100: Query execution was interrupted
UNLOCK TABLES;
#
# connection con1
# Release shared metadata lock by closing HANDLER.
HANDLER t1 CLOSE;
#
# connection default
DROP TABLE t1;
SET DEBUG_SYNC='RESET';
CREATE TABLE t1 (c1 INT);
INSERT INTO t1 VALUES (1);
#
# connection con1
START TRANSACTION;
INSERT INTO t1 VALUES (2);
#
# connection default
SET DEBUG_SYNC='mdl_acquire_lock_wait SIGNAL waiting';
TRUNCATE TABLE t1;
#
# connection con1
SET DEBUG_SYNC='now WAIT_FOR waiting';
KILL QUERY @id;
#
# connection default
ERROR 70100: Query execution was interrupted
#
# connection con1
# Release SW lock by committing transaction.
COMMIT;
#
# connection default
UNLOCK TABLES;
DROP TABLE t1;
SET DEBUG_SYNC='RESET';
