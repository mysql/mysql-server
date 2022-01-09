# Test passes --debug to mysqld:
--source include/have_debug.inc

--source log_pre_8_0_30_case_begin.inc

--echo ############################################################################################
--echo # Case 5: Attempt to start on redo files created by MEB.
--echo # 5A: InnoDB starts and performs upgrade on redo files created by MEB.
--echo ############################################################################################

perl;
    open my $fh, '+<:raw', $ENV{IB_LOGFILE0_PATH} or die "open failed: $!\n";
    seek $fh, 16, 0;
    syswrite($fh, 'MEB45678901234567890123456789012', 32) == 32 or die "write failed: $!\n";
    close $fh or die "close failed: $!\n";
EOF

let $i = 1;
while ($i <= 2) {
    --remove_file $SEARCH_FILE

    --echo # Case 5A: Start MySQL... (attempt $i)
    --error 1,42
    --exec $MYSQLD_CMD $MYSQLD_ARGS --debug="d,log_header_checksum_disabled" --$UNKNOWN_PARAM

    --echo # Verify...

    let SEARCH_PATTERN = log file was created by mysqlbackup --apply-log at MEB4567890123456789012345678901\.;
    # Expected: found iff $i = 1
    --source include/search_pattern.inc

    let SEARCH_PATTERN = Upgrading redo log: .*, LSN=;
    # Expected: found iff $i = 1
    --source include/search_pattern.inc

    let SEARCH_PATTERN = $PATTERN_UNKNOWN_PARAM;
    # Expected: found
    --source include/search_pattern.inc

    inc $i;
}

--echo ############################################################################################
--source log_pre_8_0_30_case_end.inc
