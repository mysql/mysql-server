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

  --echo # Get rid of the explcit undo tablespaces.
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

let FILE_SIZE_11 = `SELECT FILE_SIZE FROM INFORMATION_SCHEMA.INNODB_TABLESPACES WHERE NAME = '$UNDO_NAME_1'`;
let FILE_SIZE_21 = `SELECT FILE_SIZE FROM INFORMATION_SCHEMA.INNODB_TABLESPACES WHERE NAME = '$UNDO_NAME_2'`;

let MYSQLD_DATADIR = `select @@datadir`;

--echo # Turn off purging so that undo logs build up in the undo tablespaces.
SET GLOBAL innodb_purge_stop_now=ON;

--echo # Perform enough DML action so that undo tablespace sizes grow beyond
--echo # the threshold set by --innodb-max-undo-log-size.
--connect(con1,localhost,root,,test)
--connection con1

create table t1(
  keyc int,
  c1 char(255),
  c2 char(255),
  c3 char(255),
  c4 char(255),
  c5 char(255),
  c6 char(255),
  primary key(keyc)) engine = innodb;

delimiter |;
CREATE PROCEDURE populate_t1()
BEGIN
  DECLARE i INT DEFAULT 1;
  while (i <= 16000) DO
    insert into t1 values (i, 'a', 'b', 'c', 'd', 'e', 'f' );
    SET i = i + 1;
  END WHILE;
END |
delimiter ;|

call populate_t1();
delete from t1 where keyc < 10000;
update t1 set c1 = 'mysql' where keyc > 10000;
update t1 set c2 = 'mysql' where keyc > 10000;
update t1 set c3 = 'mysql' where keyc > 10000;
update t1 set c4 = 'mysql' where keyc > 10000;
update t1 set c5 = 'mysql' where keyc > 10000;
update t1 set c6 = 'mysql' where keyc > 10000;

--echo # Start a new transaction for both tablespaces to get utilized
--connect(con2,localhost,root,,test)
--connection con2

create table t2 (
  keyc int,
  c1 char(255),
  c2 char(255),
  c3 char(255),
  c4 char(255),
  c5 char(255),
  c6 char(255),
  primary key(keyc)) engine = innodb;

delimiter |;
CREATE PROCEDURE populate_t2()
BEGIN
  DECLARE i INT DEFAULT 1;
  while (i <= 16000) DO
    insert into t2 values (i, 'a', 'b', 'c', 'd', 'e', 'f' );
    SET i = i + 1;
  END WHILE;
END |
delimiter ;|

call populate_t2();
delete from t2 where keyc < 10000;
update t2 set c1 = 'mysql' where keyc > 10000;
update t2 set c2 = 'mysql' where keyc > 10000;
update t2 set c3 = 'mysql' where keyc > 10000;
update t2 set c4 = 'mysql' where keyc > 10000;
update t2 set c5 = 'mysql' where keyc > 10000;
update t2 set c6 = 'mysql' where keyc > 10000;

--connection default

drop PROCEDURE populate_t1;
drop PROCEDURE populate_t2;
drop table t1, t2;
--disconnect con1
--disconnect con2

SELECT NAME, SPACE_TYPE, STATE, AUTOEXTEND_SIZE FROM INFORMATION_SCHEMA.INNODB_TABLESPACES
       WHERE SPACE_TYPE = 'Undo' ORDER BY NAME;

--echo # Compare the undo tablespace sizes before and after DML growth.
let FILE_SIZE_12 = `SELECT FILE_SIZE FROM INFORMATION_SCHEMA.INNODB_TABLESPACES WHERE NAME = '$UNDO_NAME_1'`;
let FILE_SIZE_22 = `SELECT FILE_SIZE FROM INFORMATION_SCHEMA.INNODB_TABLESPACES WHERE NAME = '$UNDO_NAME_2'`;
if($FILE_SIZE_11 > $FILE_SIZE_12)
{
  echo ERROR: Size increase did not happen for $UNDO_NAME_1: Initial size is $FILE_SIZE_11, Size after DML is $FILE_SIZE_12;
}
if($FILE_SIZE_21 > $FILE_SIZE_22)
{
  echo ERROR: Size increase did not happen for $UNDO_NAME_2: Initial size is $FILE_SIZE_21, Size after DML is $FILE_SIZE_22;
}

if ($CRASH_ON_SHUTDOWN == 1)
{
  --echo Setting crash point = '$CRASH_POINT'.
  eval SET GLOBAL DEBUG = "+d,$CRASH_POINT";
}

--echo # Do a slow shut down so the undo tablespaces can be truncated.
SET GLOBAL innodb_max_undo_log_size=20000000;
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
--let $restart_parameters="restart:"
--let $explicit_default_wait_counter = 15000
--source include/start_mysqld.inc

let FILE_SIZE_13 = `SELECT FILE_SIZE FROM INFORMATION_SCHEMA.INNODB_TABLESPACES WHERE NAME = '$UNDO_NAME_1'`;
let FILE_SIZE_23 = `SELECT FILE_SIZE FROM INFORMATION_SCHEMA.INNODB_TABLESPACES WHERE NAME = '$UNDO_NAME_2'`;

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
