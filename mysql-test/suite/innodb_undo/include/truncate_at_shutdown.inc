# As we forcefully delete the undo logs and restart, these errors
# are expected while the server looks for the removed undo files
--disable_query_log
call mtr.add_suppression("\\[ERROR\\] .* Operating system error number 2 in a file operation.");
call mtr.add_suppression("\\[ERROR\\] .* The error means the system cannot find the path specified.");
call mtr.add_suppression("\\[ERROR\\] .* If you are installing InnoDB, remember that you must create directories yourself, InnoDB does not create them.");
call mtr.add_suppression("\\[ERROR\\].* File .* 'open' returned OS error .*");
call mtr.add_suppression("\\[ERROR\\].* File .* 'delete' returned OS error .*");
call mtr.add_suppression("\\[ERROR\\] .* Cannot find undo tablespace innodb_undo_001 with filename '.*' as indicated by the Data Dictionary. Did you move or delete this tablespace. Any undo logs in it cannot be used");
--enable_query_log

--echo # While the purge thread is turned off, perform enough DML to cause
--echo # undo tablespace sizes grow beyond the truncation threshold.

if ($CRASH_ON_SHUTDOWN == 0)
{
  --echo # Then check if truncation is accomplished by restarting mysqld
  --echo # causing each undo tablespace to be truncated.
}

if ($CRASH_ON_SHUTDOWN == 1)
{
  --echo # Then inject a crash at $CRASH_POINT and check that recovery occurs
  --echo # and that the truncation is completed if it was started.
}

# This test uses debug settings like innodb_purge_stop_now.
--source include/have_debug.inc

# Valgrind would complain about memory leaks when we crash on purpose.
--source include/not_valgrind.inc

# Avoid CrashReporter popup on Mac
--source include/not_crashrep.inc

# The test must start with only 2 undo tablespaces
--source include/have_innodb_default_undo_tablespaces.inc

--echo # Create 2 explicit undo tablespaces.
CREATE UNDO TABLESPACE undo_003 ADD DATAFILE 'undo_003.ibu';
CREATE UNDO TABLESPACE undo_004 ADD DATAFILE 'undo_004.ibu';

--echo # Make the 2 implicit undo tablespaces inactive so they get truncated.
SET GLOBAL innodb_purge_rseg_truncate_frequency=1;
ALTER UNDO TABLESPACE innodb_undo_001 SET INACTIVE;
ALTER UNDO TABLESPACE innodb_undo_002 SET INACTIVE;
let $inactive_undo_space = innodb_undo_001;
source include/wait_until_undo_space_is_empty.inc;
let $inactive_undo_space = innodb_undo_002;
source include/wait_until_undo_space_is_empty.inc;

if ($USE_EXPLICIT == 0)
{
  --echo # Make the 2 implicit undo tablespaces active.
  ALTER UNDO TABLESPACE innodb_undo_001 SET ACTIVE;
  ALTER UNDO TABLESPACE innodb_undo_002 SET ACTIVE;

  --echo # Get rid of the explicit undo tablespaces.
  ALTER UNDO TABLESPACE undo_003 SET INACTIVE;
  ALTER UNDO TABLESPACE undo_004 SET INACTIVE;
  let $inactive_undo_space = undo_003;
  source include/wait_until_undo_space_is_empty.inc;
  let $inactive_undo_space = undo_004;
  source include/wait_until_undo_space_is_empty.inc;
  DROP UNDO TABLESPACE undo_003;
  DROP UNDO TABLESPACE undo_004;

  --let UNDO_NAME_1 = innodb_undo_001
  --let UNDO_NAME_2 = innodb_undo_002
}

if ($USE_EXPLICIT == 1) {
  --let UNDO_NAME_1 = undo_003
  --let UNDO_NAME_2 = undo_004
}

SET GLOBAL innodb_purge_rseg_truncate_frequency=default;

--echo # The two undo tablespaces to truncate at shutdown will be $UNDO_NAME_1 and $UNDO_NAME_2

SELECT NAME, SPACE_TYPE, STATE, AUTOEXTEND_SIZE FROM INFORMATION_SCHEMA.INNODB_TABLESPACES
       WHERE SPACE_TYPE = 'Undo' ORDER BY NAME;

let $FILE_SIZE_11 = `SELECT FILE_SIZE FROM INFORMATION_SCHEMA.INNODB_TABLESPACES WHERE NAME = '$UNDO_NAME_1'`;
let $FILE_SIZE_21 = `SELECT FILE_SIZE FROM INFORMATION_SCHEMA.INNODB_TABLESPACES WHERE NAME = '$UNDO_NAME_2'`;
let $TRUNCATE_THRESHOLD = 20000000;

let MYSQLD_DATADIR = `select @@datadir`;

--echo # Turn off purging so that UPDATE undo logs build up in the undo tablespaces.
SET GLOBAL innodb_purge_stop_now=ON;

--echo # Perform enough DML action so that both undo tablespaces grow beyond
--echo # the threshold set by --innodb-max-undo-log-size.

CREATE TABLE t1(
  id INT PRIMARY KEY,
  c1 CHAR(255) NOT NULL,
  c2 CHAR(255) NOT NULL,
  c3 CHAR(255) NOT NULL
);
INSERT INTO t1 VALUES
  (1, 'ab', 'ab', 'ab'),
  (2, 'ab', 'ab', 'ab'),
  (3, 'ab', 'ab', 'ab'),
  (4, 'ab', 'ab', 'ab');

# Note: this is not one big transaction, but many single-statement transactions.
# We use many small transactions instead of one big, so that the Undo Logs get
# assigned evenly to the Rollback Segments and thus to both Undo Spaces, even
# if there is some stray background transaction like dict stats update.
# For each of them, InnoDB will have to at minimum allocate Undo Log header, and
# some bytes to describe the before-image of c1,c2,c3 an UPDATE Undo Log.
# The number of iterations was chosen empirically, so that the Undo Logs
# generated much larger than 20MB, but also comes from the following reasoning.
# Assume conservative estimate of 1100 bytes per Update Undo Log (we are
# modifying 3 fields of fixed 255-byte width + Undo Log Header itself has
# 140 XID + 2 * 64 for GTID + several fixed-length fields).
# We want to fill two Undo Spaces with 20MB of data. Assuming 10% margin for
# imbalance, we aim for 2x22MB = 44MB. This is just 1100 * 4 * 10000.
# We run it from 4 clients in parallel just to make it faster - it
# has nothing to do with spreading the Undo Logs between files.
# In practice, the observed growth is approximately 1786 Undo Page pages in
# each of the two spaces, and both end up rounded up to 48MB.
delimiter |;
CREATE PROCEDURE generate_many_update_undo_logs(tid INT)
BEGIN
  DECLARE i INT DEFAULT 1;
  while (i <= 10000) DO
    UPDATE t1 SET c1=REVERSE(c1),c2=REVERSE(c2),c3=REVERSE(c3) WHERE id=tid;
    SET i = i + 1;
  END WHILE;
END |
delimiter ;|
# To make undo generation faster, we temporarily sacrifice ACID,
# by not waiting for persistence after each of the many transactions
SET @saved_innodb_flush_log_at_trx_commit = @@innodb_flush_log_at_trx_commit;
SET GLOBAL innodb_flush_log_at_trx_commit = 0;
SET @saved_sync_binlog = @@sync_binlog;
SET GLOBAL sync_binlog = 0;
--let $tid=1
while($tid <= 4){
  --connect (con$tid,localhost,root,,)
  --send_eval CALL generate_many_update_undo_logs($tid)
  --inc $tid
}
--let $tid=1
while($tid <= 4){
  --connection con$tid
  --reap
  --connection default
  --disconnect con$tid
  --inc $tid
}
SET GLOBAL innodb_flush_log_at_trx_commit = @saved_innodb_flush_log_at_trx_commit;
SET GLOBAL sync_binlog = @saved_sync_binlog;

DROP PROCEDURE generate_many_update_undo_logs;
DROP TABLE t1;

SELECT NAME, SPACE_TYPE, STATE, AUTOEXTEND_SIZE FROM INFORMATION_SCHEMA.INNODB_TABLESPACES
       WHERE SPACE_TYPE = 'Undo' ORDER BY NAME;

--echo # Compare the undo tablespace sizes before and after DML growth.
let $FILE_SIZE_12 = `SELECT FILE_SIZE FROM INFORMATION_SCHEMA.INNODB_TABLESPACES WHERE NAME = '$UNDO_NAME_1'`;
let $FILE_SIZE_22 = `SELECT FILE_SIZE FROM INFORMATION_SCHEMA.INNODB_TABLESPACES WHERE NAME = '$UNDO_NAME_2'`;

if($FILE_SIZE_12 <= $TRUNCATE_THRESHOLD)
{
  echo ERROR: Undo tablespace did not grow beyond the truncation threshold for $UNDO_NAME_1: Initial size is $FILE_SIZE_11, Size after DML is $FILE_SIZE_12, Threshold is $TRUNCATE_THRESHOLD;
}
if($FILE_SIZE_22 <= $TRUNCATE_THRESHOLD)
{
  echo ERROR: Undo tablespace did not grow beyond the truncation threshold for $UNDO_NAME_2: Initial size is $FILE_SIZE_21, Size after DML is $FILE_SIZE_22, Threshold is $TRUNCATE_THRESHOLD;
}

if ($CRASH_ON_SHUTDOWN == 1)
{
  --echo Setting crash point = '$CRASH_POINT'.
  eval SET GLOBAL DEBUG = "+d,$CRASH_POINT";
}

--echo # Do a slow shut down so the undo tablespaces can be truncated.
--eval SET GLOBAL innodb_max_undo_log_size=$TRUNCATE_THRESHOLD
SET GLOBAL innodb_fast_shutdown=0;
SET GLOBAL innodb_purge_rseg_truncate_frequency=1;
SET GLOBAL innodb_purge_run_now=ON;
--let $shutdown_server_timeout= 300
--source include/shutdown_mysqld.inc

if ($CRASH_ON_SHUTDOWN == 1)
{
  --echo Recovery from crash point = '$CRASH_POINT'.
  let SEARCH_FILE = $MYSQLTEST_VARDIR/log/mysqld.1.err;
  let SEARCH_PATTERN = $CRASH_POINT;
  --source include/search_pattern.inc
  --remove_files_wildcard $MYSQLD_DATADIR cht_logbin.*
  --remove_files_wildcard $MYSQLD_DATADIR binlog.*
}

# The truncate will get started during shutdown using the settings above.
# The restarted engine does not need these settings to complete the truncate
# if the injected crash was far enough along.
--let $restart_parameters="restart: $PLUGIN_DIR_OPT"
--let $explicit_default_wait_counter = 15000
--source include/start_mysqld_no_echo.inc

let $FILE_SIZE_13 = `SELECT FILE_SIZE FROM INFORMATION_SCHEMA.INNODB_TABLESPACES WHERE NAME = '$UNDO_NAME_1'`;
let $FILE_SIZE_23 = `SELECT FILE_SIZE FROM INFORMATION_SCHEMA.INNODB_TABLESPACES WHERE NAME = '$UNDO_NAME_2'`;

if ($CRASH_ON_SHUTDOWN == 0)
{
  --echo # Compare the undo tablespace sizes before and after truncation.
  if ($FILE_SIZE_12 <= $FILE_SIZE_13)
  {
    --echo ERROR: Truncated size is wrong for $UNDO_NAME_1: Size before truncation is $FILE_SIZE_12, Size after truncation  is $FILE_SIZE_13;
  }
  if ($FILE_SIZE_22 <= $FILE_SIZE_23)
  {
    --echo ERROR: Truncated size is wrong for $UNDO_NAME_2: Size before truncation is $FILE_SIZE_22, Size after truncation is $FILE_SIZE_23;
  }
}

SELECT NAME, SPACE_TYPE, STATE, AUTOEXTEND_SIZE FROM INFORMATION_SCHEMA.INNODB_TABLESPACES
       WHERE SPACE_TYPE = 'Undo' ORDER BY NAME;

--echo #
--echo # Cleanup
--echo #

if ($USE_EXPLICIT == 1)
{
  ALTER UNDO TABLESPACE innodb_undo_001 SET ACTIVE;
  ALTER UNDO TABLESPACE innodb_undo_002 SET ACTIVE;
  ALTER UNDO TABLESPACE undo_003 SET INACTIVE;
  ALTER UNDO TABLESPACE undo_004 SET INACTIVE;

  SET GLOBAL innodb_purge_rseg_truncate_frequency=1;
  let $inactive_undo_space = undo_003;
  source include/wait_until_undo_space_is_empty.inc;
  let $inactive_undo_space = undo_004;
  source include/wait_until_undo_space_is_empty.inc;
  SET GLOBAL innodb_purge_rseg_truncate_frequency=default;

  DROP UNDO TABLESPACE undo_003;
  DROP UNDO TABLESPACE undo_004;
}

SELECT NAME, SPACE_TYPE, STATE, AUTOEXTEND_SIZE FROM INFORMATION_SCHEMA.INNODB_TABLESPACES
       WHERE SPACE_TYPE = 'Undo' ORDER BY NAME;
