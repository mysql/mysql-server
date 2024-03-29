# According to perl's documentation, 'Q' in (un)pack() is supported only if perl
# has been compiled with support for 64bit integers.
--source include/have_perl_64bit_int.inc

--source log_pre_8_0_30_case_begin.inc

--echo ############################################################################################
--echo # Case 7: Attempt to start on non-empty redo files.
--echo # 7A: InnoDB refuses to start on logically non-empty redo log.
--echo # 7B: InnoDB refuses to start when the first log block is corrupted (invalid checksum).
--echo ############################################################################################

--echo # Update data_len field to 512 in the data block containing the checkpoint_lsn...
perl;
    # Find the oldest checkpoint lsn and related offset:
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
    $offset += 8;
    seek $fh, $offset, 0;
    read $fh, my $packed_checkpoint_offset, 8 or die "read failed: $!\n";
    my $checkpoint_offset = unpack('Q>', $packed_checkpoint_offset);
    close $fh or die "close failed: $!\n";

    # Compute file and block pointed by the checkpoint offset:
    my $file_size = -s $ENV{IB_LOGFILE0_PATH};
    my $nth_file = $checkpoint_offset / $file_size % 2;
    my $file_path = '';
    if ($nth_file == 0) {
        $file_path = $ENV{IB_LOGFILE0_PATH};
    } else {
        $file_path = $ENV{IB_LOGFILE1_PATH};
    }
    my $file_offset = $checkpoint_offset % $file_size;
    my $block_offset = int($file_offset / 512) * 512;

    # Open the pointed file and update the data_len field:
    open $fh, '+<:raw', $file_path or die "open failed: $!\n";
    seek $fh, $block_offset + 4, 0;
    syswrite($fh, pack('S>', 512), 2) == 2 or die "write failed: $!\n";
    close $fh or die "close failed: $!\n";
EOF

let $i = 1;
while ($i <= 2) {
    --remove_file $SEARCH_FILE

    --echo # Case 7A: Start MySQL... (attempt $i)
    --error 1,42
    --exec $MYSQLD_CMD $MYSQLD_ARGS --innodb-log-checksums=OFF --$UNKNOWN_PARAM

    --echo # Verify...

    let SEARCH_PATTERN = Upgrade is not supported after a crash or shutdown with innodb_fast_shutdown = 2.* appears logically non empty;
    # Expected: found
    --source include/search_pattern.inc

    let SEARCH_PATTERN = Upgrading redo log: .*, LSN=;
    # Expected: not found
    --source include/search_pattern.inc

    let SEARCH_PATTERN = $PATTERN_UNKNOWN_PARAM;
    # Expected: not found
    --source include/search_pattern.inc

    inc $i;
}

--echo ############################################################################################

let $i = 1;
while ($i <= 2) {
    --remove_file $SEARCH_FILE

    --echo # Case 7B: Start MySQL... (attempt $i)
    --error 1,42
    --exec $MYSQLD_CMD $MYSQLD_ARGS --$UNKNOWN_PARAM

    --echo # Verify...

    let SEARCH_PATTERN = Upgrade after a crash is not supported.* appears corrupted;
    # Expected: found
    --source include/search_pattern.inc

    let SEARCH_PATTERN = Upgrading redo log: .*, LSN=;
    # Expected: not found
    --source include/search_pattern.inc

    let SEARCH_PATTERN = $PATTERN_UNKNOWN_PARAM;
    # Expected: not found
    --source include/search_pattern.inc

    inc $i;
}

--echo ############################################################################################
--source log_pre_8_0_30_case_end.inc
