--source log_pre_8_0_30_case_begin.inc

--echo ############################################################################################
--echo # Case 1: Attempt to start after clean shutdown on mysql-$TESTED_DATADIR_VERSION.
--echo # Pre-conditions: logically empty redo files from mysql-$TESTED_DATADIR_VERSION.
if ($LOG_ALLOW_UPGRADE == 0) {
    --echo # 1A: InnoDB refuses to start because of the too old redo log format.
}
if ($LOG_ALLOW_UPGRADE == 1) {
    --echo # 1A: InnoDB successfully upgrades and starts.
}
--echo # 1B: InnoDB refuses to start with innodb-force-recovery = 1 or 6.
--echo # 1C: InnoDB refuses to start with innodb-read-only = ON.
--echo ############################################################################################

let $i = 1;
while ($i <= 2) {
    --remove_file $SEARCH_FILE

    --echo # Case 1A: Start MySQL... (attempt $i)
    --error 1,42
    --exec $MYSQLD_CMD $MYSQLD_ARGS --$UNKNOWN_PARAM

    --echo # Verify...

    let SEARCH_PATTERN = $LOG_FORMAT_MESSAGE;
    # Expected: found unless upgraded
    --source include/search_pattern.inc

    let SEARCH_PATTERN = Redo log is from an earlier version;
    # Expected: found unless upgraded
    --source include/search_pattern.inc

    let SEARCH_PATTERN = The latest found checkpoint is at lsn =;
    # Expected: not found, unless $LOG_ALLOW_UPGRADE == 1
    --source include/search_pattern.inc

    let SEARCH_PATTERN = Upgrading redo log: .*, LSN=;
    # Expected: not found, unless $LOG_ALLOW_UPGRADE == 1
    --source include/search_pattern.inc

    let SEARCH_PATTERN = $PATTERN_UNKNOWN_PARAM;
    # Expected: not found, unless $LOG_ALLOW_UPGRADE == 1
    --source include/search_pattern.inc

    inc $i;
}

if ($LOG_ALLOW_UPGRADE == 1) {
    --echo # Redo log has become upgraded - replacing with older (from before upgrade)...
    --source log_pre_8_0_30_case_end.inc
    --source log_pre_8_0_30_case_begin.inc
}

--echo ############################################################################################

let $i = 1;
while ($i <= 2) {
    --remove_file $SEARCH_FILE

    if ($i == 1) {
        let $force_recovery = 1;
    }
    if ($i == 2) {
        let $force_recovery = 6;
    }

    --echo # Case 1B-$i: Start MySQL with --innodb-force-recovery=$force_recovery...
    --error 1,42
    --exec $MYSQLD_CMD $MYSQLD_ARGS --innodb-force-recovery=$force_recovery --$UNKNOWN_PARAM

    --echo # Verify...

    let SEARCH_PATTERN = Database upgrade cannot be accomplished with innodb_force_recovery > 0;
    # Expected: found
    --source include/search_pattern.inc

    let SEARCH_PATTERN = Cannot upgrade format .* of redo log files when innodb-force-recovery > 0;
    --source include/search_pattern.inc

    let SEARCH_PATTERN = The latest found checkpoint is at lsn =;
    # Expected: not found
    --source include/search_pattern.inc

    inc $i;
}

--echo ############################################################################################

--remove_file $SEARCH_FILE

--echo # Case 1C: Start MySQL with innodb-read-only=ON...
--error 1,42
--exec $MYSQLD_CMD $MYSQLD_ARGS --innodb-read-only=ON

--echo # Verify...

let SEARCH_PATTERN = Database upgrade cannot be accomplished in read-only mode;
# Expected: found
--source include/search_pattern.inc

let SEARCH_PATTERN = The latest found checkpoint is at lsn =;
# Expected: not found
--source include/search_pattern.inc

--echo ############################################################################################
--source log_pre_8_0_30_case_end.inc
