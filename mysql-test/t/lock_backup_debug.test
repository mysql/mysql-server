--source include/have_debug.inc

--echo #
--echo # Bug26831155 -- CONCURRENT DDL OPERATION IN PROGRESS EVEN AFTER ACQUIRING BACKUP LOCK
--echo #

CREATE TABLE t1 (a INT);

--connect (con1, localhost, root,,)

SET lock_wait_timeout=1;

--connection default
SET DEBUG_SYNC='mysql_rm_table_after_lock_table_names SIGNAL run_backup_lock WAIT_FOR continue_dropping_table';

--send DROP TABLE t1

--connection con1

SET DEBUG_SYNC='now WAIT_FOR run_backup_lock';

--error ER_LOCK_WAIT_TIMEOUT
LOCK INSTANCE FOR BACKUP;

SET DEBUG_SYNC='now SIGNAL continue_dropping_table';

UNLOCK INSTANCE;

--connection default
--reap # Reaping a result of DROP TABLE t1

--connection con1
--disconnect con1

--source include/wait_until_disconnected.inc
--connection default
