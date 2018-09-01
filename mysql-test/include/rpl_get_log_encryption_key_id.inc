# ==== Purpose ====
#
# Retrieve a given replication log file encryption key id.
#
# ==== Usage ====
#
# --let $rpl_log_file= <BINARY OR RELAY LOG FILE>
# [--let $rpl_debug= 0]
# --source include/rpl_get_log_encryption_key_id.inc
#
# Parameters:
#
#   $rpl_log_file
#     The file to be inspected.
#
#   $rpl_debug=1
#     Print extra debugging information.
#
# Output variable:
#
# $rpl_encryption_key_id will be set with:
#
#   - Error: Error fetching the info.
#
#   - None: The file is not encrypted.
#
#   - MySQLReplicationKey_<UUID>_<SEQNO>: a replication encryption key.
#

--let $_rgeki_suffix= `SELECT UUID()`
--let _RPL_RESULT_FILE= $MYSQLTEST_VARDIR/tmp/_rgeki_$_rgeki_suffix
--let _RPL_LOG_FILE= $rpl_log_file
--let _RPL_DEBUG= $rpl_debug
--let $rpl_encryption_key_id=Error

# Write file to make mysql-test-run.pl start up the server again
# Because mysqltest is such a wonderful language, we use perl instead.
perl;
  my $log_file= $ENV{'_RPL_LOG_FILE'};
  my $result_file= $ENV{'_RPL_RESULT_FILE'};
  if ($ENV{'_RPL_DEBUG'})
  {
    print "# debug: log_file='$log_file'\n";
  }

  # Open the file in raw mode
  open LFILE, '<:raw', $log_file or die "Error opening $log_file: $!";
  open RFILE, "> $result_file" or die "Error opening $result_file: $!";

  # Read binlog magic
  my $bytes_read = read LFILE, my $magic, 4;
  die 'Got $bytes_read but expected 4' unless $bytes_read == 4;

  if ($ENV{'_RPL_DEBUG'})
  {
    print "# debug: magic='$magic'\n";
  }

  my $plain_magic = "\xfe\x62\x69\x6e";
  my $encrypted_magic = "\xfd\x62\x69\x6e";

  if ($magic eq $plain_magic) {
    # Ordinary binary log
    if ($ENV{'_RPL_DEBUG'})
    {
      print "# debug: ordinary log file\n";
    }
    print RFILE "--let \$rpl_encryption_key_id=None\n" or die "Error writing to $result_file: $!";
  } elsif ($magic eq $encrypted_magic) {
    # Encrypted binary log
    if ($ENV{'_RPL_DEBUG'})
    {
      print "# debug: encrypted log file\n";
    }
    $bytes_read = read LFILE, my $version, 1;
    die 'Got $bytes_read but expected 1' unless $bytes_read == 1;
    if ($version cmp "\x01")
    {
      die 'Unexpected replication encryption header version';
    }
    $bytes_read = read LFILE, my $field_type, 1;
    die 'Got $bytes_read but expected 1' unless $bytes_read == 1;
    if ($field_type cmp "\x01")
    {
      die 'Unexpected field type';
    }
    $bytes_read = read LFILE, my $length, 1;
    die 'Got $bytes_read but expected 1' unless $bytes_read == 1;
    $length = ord($length);
    $bytes_read = read LFILE, my $key_id, $length;
    die 'Got $bytes_read but expected $length' unless $bytes_read == $length;
    print RFILE "--let \$rpl_encryption_key_id=$key_id\n" or die "Error writing to $result_file: $!";
  }
  else {
    die 'Not a binary log file.';
  }

  close LFILE or die "Error closing $log_file: $!";
  close RFILE or die "Error closing $result_file: $!";

EOF

--source $_RPL_RESULT_FILE
--remove_file $_RPL_RESULT_FILE
--let _RPL_LOG_FILE=
--let _RPL_DEBUG=
--let _RPL_RESULT_FILE=
