--source log_pre_8_0_30_case_begin.inc

--echo ############################################################################################
--echo # Case 2: Attempt to start with invalid set of redo log files.
--echo # 2A: InnoDB refuses to start if ib_logfile1 was removed
--echo # 2B: InnoDB refuses to start if ib_logfile0 was removed
--echo # 2C: InnoDB refuses to start if both redo files were removed
--echo # NOTE: Ensure there was no attempt to check if redo is empty.
--echo ############################################################################################

--remove_file $SEARCH_FILE

--echo # Hide the ib_logfile1...
--move_file $MYSQLD_DATADIR/ib_logfile1 $MYSQLD_DATADIR/tmp_ib_logfile1

--echo # Case 2A: Start MySQL...
--error 1,42
--exec $MYSQLD_CMD $MYSQLD_ARGS --$UNKNOWN_PARAM

--echo # Verify...

let SEARCH_PATTERN = Only one redo log file found;
# Expected: found
--source include/search_pattern.inc

let SEARCH_PATTERN = $PATTERN_UNKNOWN_PARAM;
# Expected: not found
--source include/search_pattern.inc

--echo # Restore the ib_logfile1...
--move_file $MYSQLD_DATADIR/tmp_ib_logfile1 $MYSQLD_DATADIR/ib_logfile1

--echo ############################################################################################

--remove_file $SEARCH_FILE

--echo # Hide the ib_logfile0...
--move_file $MYSQLD_DATADIR/ib_logfile0 $MYSQLD_DATADIR/tmp_ib_logfile0

--echo # Case 2B: Start MySQL...
--error 1,42
--exec $MYSQLD_CMD $MYSQLD_ARGS --$UNKNOWN_PARAM

--echo # Verify...

let SEARCH_PATTERN = Missing ib_logfile0 in the directory;
# Expected: found
--source include/search_pattern.inc

let SEARCH_PATTERN = $PATTERN_UNKNOWN_PARAM;
# Expected: not found
--source include/search_pattern.inc

--echo # Restore the ib_logfile0...
--move_file $MYSQLD_DATADIR/tmp_ib_logfile0 $MYSQLD_DATADIR/ib_logfile0

--echo ############################################################################################

--remove_file $SEARCH_FILE

--echo # Hide both redo log files (ib_logfile0 and ib_logfile1)...

--move_file $MYSQLD_DATADIR/ib_logfile0 $MYSQLD_DATADIR/tmp_ib_logfile0
--move_file $MYSQLD_DATADIR/ib_logfile1 $MYSQLD_DATADIR/tmp_ib_logfile1

--echo # Case 2C: Start MySQL...
--error 1,42
--exec $MYSQLD_CMD $MYSQLD_ARGS --$UNKNOWN_PARAM

--echo # Verify...

let SEARCH_PATTERN = Neither found .* subdirectory, nor .* files;
# Expected: found
--source include/search_pattern.inc

# Expected: not found
let SEARCH_PATTERN = $PATTERN_UNKNOWN_PARAM;
--source include/search_pattern.inc

--echo # Restore both redo log files (ib_logfile0 and ib_logfile1)...

--move_file $MYSQLD_DATADIR/tmp_ib_logfile1 $MYSQLD_DATADIR/ib_logfile1
--move_file $MYSQLD_DATADIR/tmp_ib_logfile0 $MYSQLD_DATADIR/ib_logfile0

--echo ############################################################################################
--source log_pre_8_0_30_case_end.inc
