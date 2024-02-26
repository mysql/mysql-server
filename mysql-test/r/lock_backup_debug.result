#
# Bug26831155 -- CONCURRENT DDL OPERATION IN PROGRESS EVEN AFTER ACQUIRING BACKUP LOCK
#
CREATE TABLE t1 (a INT);
SET lock_wait_timeout=1;
SET DEBUG_SYNC='mysql_rm_table_after_lock_table_names SIGNAL run_backup_lock WAIT_FOR continue_dropping_table';
DROP TABLE t1;
SET DEBUG_SYNC='now WAIT_FOR run_backup_lock';
LOCK INSTANCE FOR BACKUP;
ERROR HY000: Lock wait timeout exceeded; try restarting transaction
SET DEBUG_SYNC='now SIGNAL continue_dropping_table';
UNLOCK INSTANCE;
