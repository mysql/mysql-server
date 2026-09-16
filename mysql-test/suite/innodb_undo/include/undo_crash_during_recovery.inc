###############################################################################
# Test the crash during recovery when UNDO truncation was handled
#
# Following to be set from the caller
#   $crash_point : A point where server should crash when UNDO is truncated.
#   $recovery_crash_point_1 : A point where recovery should crash.
###############################################################################

let $MYSQLD_DATADIR= `SELECT @@datadir`;
# Create an EXPLICIT UNDO tablespace
CREATE UNDO TABLESPACE undo_003 ADD DATAFILE 'undo_003.ibu';

# Set debug crash for truncate
--echo Setting server to crash at "$crash_point" during UNDO truncate
eval SET GLOBAL DEBUG = "+d,$crash_point";
--source include/expect_crash.inc

--echo # Make the 1st explicit undo tablespace inactive so it gets truncated.
SET GLOBAL innodb_purge_rseg_truncate_frequency=1;
ALTER UNDO TABLESPACE undo_003 SET INACTIVE;
--source include/wait_until_disconnected.inc

--echo # Restart server to crash during recovery at "$recovery_crash_point_1"
--let $restart_parameters = "--debug=+d,$recovery_crash_point_1"
--echo $restart_parameters
--source include/start_mysqld_expecting_crash.inc

--echo # Wait until failed bootstrap is seen
--let $grep_file = $MYSQLTEST_VARDIR/log/mysqld.1.err
--let $grep_pattern = $recovery_crash_point_1
--source include/wait_for_pattern_in_file.inc

# list files to check the presence of log file
--list_files $MYSQLD_DATADIR/ *.log
--echo # Restart server normally
--let $restart_parameters = restart:
--source include/start_mysqld.inc

--let $inactive_undo_space = undo_003
--source include/wait_until_undo_space_is_empty.inc

SELECT name, space_type, state, autoextend_size FROM
    INFORMATION_SCHEMA.INNODB_TABLESPACES WHERE space_type="Undo";

#
# Cleanup
#

DROP UNDO TABLESPACE undo_003;
