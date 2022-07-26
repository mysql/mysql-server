# Test passes --debug to mysqld:
--source include/have_debug.inc

--source log_pre_8_0_30_case_begin.inc

--echo ############################################################################################
--echo # Case 4: Attempt to start on redo files created by foreign creators.
--echo # 4A: InnoDB starts and performs upgrade on redo files created by unknown creator,
--echo #     but warning is emitted.
--echo ############################################################################################

--remove_file $SEARCH_FILE

perl;
    open my $fh, '+<:raw', $ENV{IB_LOGFILE0_PATH} or die "open failed: $!\n";
    seek $fh, 16, 0;
    syswrite($fh, 'ping', 4) > 0 or die "write failed: $!\n";
    close $fh or die "close failed: $!\n";
EOF

--echo # Case 4A: Start MySQL...
--error 1,42
--exec $MYSQLD_CMD $MYSQLD_ARGS --debug="d,log_header_checksum_disabled" --$UNKNOWN_PARAM

--echo # Verify...

let SEARCH_PATTERN = Redo log files created by unknown creator ping;
# Expected: found
--source include/search_pattern.inc

let SEARCH_PATTERN = Upgrading redo log: .*, LSN=;
# Expected: found
--source include/search_pattern.inc

let SEARCH_PATTERN = $PATTERN_UNKNOWN_PARAM;
# Expected: found
--source include/search_pattern.inc

--echo ############################################################################################
--source log_pre_8_0_30_case_end.inc
