
# According to perl's documentation, 'Q' in (un)pack() is supported only if perl
# has been compiled with support for 64bit integers.
--source include/have_perl_64bit_int.inc

--source log_pre_8_0_30_case_begin.inc

--echo ############################################################################################
--echo # Case 6: Attempt to start on redo files with corrupted checkpoint header.
--echo # 6A: InnoDB refuses to start if both checkpoint headers are corrupted.
--echo # 6B: InnoDB successfully starts if one of checkpoint headers (older) is corrupted.
--echo ############################################################################################

--echo # Save the ib_logfile0...
--copy_file $IB_LOGFILE0_PATH $MYSQLD_DATADIR/tmp_ib_logfile0

perl;
    open my $fh, '+<:raw', $ENV{IB_LOGFILE0_PATH} or die "open failed: $!\n";
    seek $fh, 512 + 8, 0;
    syswrite($fh, 'xxxxxxxx', 8) > 0 or die "write failed: $!\n";
    seek $fh, 3*512 + 8, 0;
    syswrite($fh, 'xxxxxxxx', 8) > 0 or die "write failed: $!\n";
    close $fh or die "close failed: $!\n";
EOF

let $i = 1;
while ($i <= 2) {
    --remove_file $SEARCH_FILE

    --echo # Case 6A: Start MySQL... (attempt $i)
    --error 1,42
    --exec $MYSQLD_CMD $MYSQLD_ARGS --$UNKNOWN_PARAM

    --echo # Verify...

    let SEARCH_PATTERN = Upgrading redo log: .*, LSN=;
    # Expected: not found
    --source include/search_pattern.inc

    let SEARCH_PATTERN = $PATTERN_UNKNOWN_PARAM;
    # Expected: not found
    --source include/search_pattern.inc

    inc $i;
}

--echo # Restore the saved ib_logfile0...
--remove_file $IB_LOGFILE0_PATH
--move_file $MYSQLD_DATADIR/tmp_ib_logfile0 $IB_LOGFILE0_PATH

--echo ############################################################################################

--echo # Corrupt one of checkpoint headers (with older checkpoint_lsn)...
perl;
    open my $fh, '+<:raw', $ENV{IB_LOGFILE0_PATH} or die "open failed: $!\n";
    my $offset1 = 512 + 8;
    my $offset2 = 3*512 + 8;
    seek $fh, $offset1, 0;
    read $fh, my $packed_checkpoint_lsn1, 8 or die "read failed: $!\n";
    seek $fh, $offset2, 0;
    read $fh, my $packed_checkpoint_lsn2, 8 or die "read failed: $!\n";
    my $checkpoint_lsn1 = unpack('Q>', $packed_checkpoint_lsn1);
    my $checkpoint_lsn2 = unpack('Q>', $packed_checkpoint_lsn2);
    my $offset = 0;
    if ($checkpoint_lsn1 <= $checkpoint_lsn2) {
        $offset = $offset1;
    } else {
        $offset = $offset2;
    }
    seek $fh, $offset, 0;
    syswrite($fh, 'xxxxxxxx', 8) == 8 or die "write failed: $!\n";
    close $fh or die "close failed: $!\n";
EOF

let $i = 1;
while ($i <= 2) {
    --remove_file $SEARCH_FILE

    --echo # Case 6B: Start MySQL... (attempt $i)
    --error 1,42
    --exec $MYSQLD_CMD $MYSQLD_ARGS --$UNKNOWN_PARAM

    --echo # Verify...

    let SEARCH_PATTERN = Upgrading redo log: .*, LSN=;
    # Expected: found iff $i == 1
    --source include/search_pattern.inc

    let SEARCH_PATTERN = $PATTERN_UNKNOWN_PARAM;
    # Expected: found
    --source include/search_pattern.inc

    inc $i;
}

--echo ############################################################################################
--source log_pre_8_0_30_case_end.inc
