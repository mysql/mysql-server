--source log_pre_8_0_30_case_begin.inc

--echo ############################################################################################
--echo # Case 3: Attempt to start with ib_logfile0 which has corrupted file header (checksum).
--echo # Pre-conditions: logically empty redo files from mysql-$TESTED_DATADIR_VERSION,
--echo #                 with corrupted checksum of the ib_logfile0's header
--echo # 3A: InnoDB refuses to start and emits error about corrupted file header
--echo # NOTE: Ensure there was no attempt to check if redo is empty.
--echo ############################################################################################

--remove_file $SEARCH_FILE

perl;
    open my $fh, '+<:raw', $ENV{IB_LOGFILE0_PATH} or die "open failed: $!\n";
    seek $fh, 508, 0;
    syswrite($fh, 'luck', 4) == 4 or die "write failed: $!\n";
    close $fh or die "close failed: $!\n";
EOF

--echo # Case 3A: Start MySQL...
--error 1,42
--exec $MYSQLD_CMD $MYSQLD_ARGS --$UNKNOWN_PARAM

--echo # Verify...

let SEARCH_PATTERN = Invalid redo log header checksum;
# Expected: found
--source include/search_pattern.inc

let SEARCH_PATTERN = Failed to read header of the redo log file .*ib_logfile0;
# Expected: found
--source include/search_pattern.inc

let SEARCH_PATTERN = Upgrading redo log: .*, LSN=;
# Expected: not found
--source include/search_pattern.inc

let SEARCH_PATTERN = $PATTERN_UNKNOWN_PARAM;
# Expected: not found
--source include/search_pattern.inc

--echo ############################################################################################
--source log_pre_8_0_30_case_end.inc
