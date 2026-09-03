# ==== Purpose ====
#
# Save, or assert unchanged, the relay-log files of the default channel of
# the current connection: the number of files in the relay-log index, the
# name and size of the last file, and Relay_Log_Space.
#
# Used by the in-memory relay log (IMR) tests that check an IMR channel does
# not create or write relay-log files (rpl_csa_imr_relay_log_no_growth,
# rpl_csa_imr_relay_log_encryption). Nothing run-dependent is printed.
#
# ==== Usage ====
#
# --let $rlf_mode= save
# --source suite/rpl_gtid/t/rpl_csa_imr_relay_log_files.inc
# ...
# --let $rlf_mode= assert
# --let $rlf_what= <text appended to the assert texts>
# --source suite/rpl_gtid/t/rpl_csa_imr_relay_log_files.inc
#
# Parameters:
#
#   $rlf_mode
#     save: store the current values in $rlf_saved_count, $rlf_saved_file,
#           $rlf_saved_size and $rlf_saved_space.
#     assert: assert that the current values equal the saved ones.
#     key: assert that the last relay-log file is encrypted with the key of
#          the replica's current binary log, which is the newest binary log
#          master key. Needs binlog_encryption = ON and a keyring.
#     verify: assert that mysqlbinlog --verify-binlog-checksum reads every
#          file in the relay-log index without error. Needs unencrypted
#          relay-log files.
#
#   $rlf_what
#     Describes the step in the assert texts (assert, key, verify modes).
#
# Output variables (all modes): $rlf_count (files in the index) and, from
# include/rpl/save_relay_log_file_position.inc, $relay_log_file and
# $relay_log_size.

--let _RLF_INDEX_FILE= `SELECT @@GLOBAL.relay_log_index`
--let _RLF_OUTPUT_FILE= `SELECT CONCAT('$MYSQLTEST_VARDIR/tmp/_rlf_count-', UUID(), '.txt')`
perl;
  my $index_file= $ENV{'_RLF_INDEX_FILE'};
  open FILE, "$index_file" or die "Error $? opening $index_file: $!";
  my $count= 0;
  while (<FILE>) {
    $count++ if /\S/;
  }
  close FILE or die "Error $? closing $index_file: $!";
  my $output_file= $ENV{'_RLF_OUTPUT_FILE'};
  open FILE, "> $output_file" or die "Error $? opening $output_file: $!";
  print FILE $count or die "Error $? writing to $output_file: $!";
  close FILE or die "Error $? writing to $output_file: $!";
EOF
--let $_rlf_count= `SELECT LOAD_FILE('$_RLF_OUTPUT_FILE')`
--let $rlf_count= $_rlf_count
--remove_file $_RLF_OUTPUT_FILE
--let _RLF_INDEX_FILE=
--let _RLF_OUTPUT_FILE=

--source include/rpl/save_relay_log_file_position.inc
--let $_rlf_space= query_get_value(SHOW REPLICA STATUS, Relay_Log_Space, 1)

if ($rlf_mode == save)
{
  --let $rlf_saved_count= $_rlf_count
  --let $rlf_saved_file= $relay_log_file
  --let $rlf_saved_size= $relay_log_size
  --let $rlf_saved_space= $_rlf_space
}

if ($rlf_mode == assert)
{
  --let $assert_text= The relay-log index has the same number of files $rlf_what
  --let $assert_cond= $_rlf_count = $rlf_saved_count
  --source include/assert.inc

  --let $assert_text= The last relay-log file is the same file $rlf_what
  --let $assert_cond= "$relay_log_file" = "$rlf_saved_file"
  --source include/assert.inc

  --let $assert_text= The last relay-log file has the same size $rlf_what
  --let $assert_cond= $relay_log_size = $rlf_saved_size
  --source include/assert.inc

  --let $assert_text= Relay_Log_Space is unchanged $rlf_what
  --let $assert_cond= $_rlf_space = $rlf_saved_space
  --source include/assert.inc
}

if ($rlf_mode == key)
{
  --let $_rlf_datadir= `SELECT @@GLOBAL.datadir`
  --let $rpl_log_file= $_rlf_datadir$relay_log_file
  --source include/rpl/get_log_encryption_key_id.inc
  --let $_rlf_relay_key= $rpl_encryption_key_id

  --let $_rlf_binlog_file= query_get_value(SHOW BINARY LOG STATUS, File, 1)
  --let $rpl_log_file= $_rlf_datadir$_rlf_binlog_file
  --source include/rpl/get_log_encryption_key_id.inc
  --let $_rlf_binlog_key= $rpl_encryption_key_id

  --let $assert_text= The replica's binary log is encrypted $rlf_what
  --let $assert_cond= "$_rlf_binlog_key" LIKE "MySQLReplicationKey_%"
  --source include/assert.inc

  --let $assert_text= The last relay-log file uses the newest master key $rlf_what
  --let $assert_cond= "$_rlf_relay_key" = "$_rlf_binlog_key"
  --source include/assert.inc
}

if ($rlf_mode == verify)
{
  --let _RLF_INDEX_FILE= `SELECT @@GLOBAL.relay_log_index`
  --let _RLF_DATADIR= `SELECT @@GLOBAL.datadir`
  --let _RLF_OUTPUT_FILE= `SELECT CONCAT('$MYSQLTEST_VARDIR/tmp/_rlf_verify-', UUID(), '.txt')`
  perl;
    my $index_file= $ENV{'_RLF_INDEX_FILE'};
    my $datadir= $ENV{'_RLF_DATADIR'};
    my $mysqlbinlog= $ENV{'MYSQL_BINLOG'};
    open FILE, "$index_file" or die "Error $? opening $index_file: $!";
    my @failed;
    while (<FILE>) {
      chomp;
      next unless /\S/;
      my $file= $_;
      $file =~ s{^\.[/\\]}{};
      my $rc= system("$mysqlbinlog --verify-binlog-checksum '$datadir/$file' > /dev/null 2>&1");
      push @failed, $file if $rc != 0;
    }
    close FILE or die "Error $? closing $index_file: $!";
    my $output_file= $ENV{'_RLF_OUTPUT_FILE'};
    open FILE, "> $output_file" or die "Error $? opening $output_file: $!";
    print FILE scalar(@failed) or die "Error $? writing to $output_file: $!";
    close FILE or die "Error $? writing to $output_file: $!";
    print "# rlf verify failed on: @failed\n" if @failed;
EOF
  --let $_rlf_failed= `SELECT LOAD_FILE('$_RLF_OUTPUT_FILE')`
  --remove_file $_RLF_OUTPUT_FILE
  --let _RLF_INDEX_FILE=
  --let _RLF_DATADIR=
  --let _RLF_OUTPUT_FILE=

  --let $assert_text= mysqlbinlog --verify-binlog-checksum reads every relay-log file $rlf_what
  --let $assert_cond= $_rlf_failed = 0
  --source include/assert.inc
}
