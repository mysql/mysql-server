# ==== Purpose ====
#
# Retrieve a given replication log file encryption key id.
#
# ==== Usage ====
#
# --let $rpl_log_file= <BINARY OR RELAY LOG FILE>
# [--let $rpl_debug= 0]
# --source include/rpl/get_log_encryption_key_id.inc
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
#   - None: The file is not encrypted.
#   - MySQLReplicationKey_<UUID>_<SEQNO>: a replication encryption key.
#   - Error: <message>: Error fetching the info from the file.
#

--let $_rgeki_suffix= `SELECT UUID()`
--let _RPL_RESULT_FILE= $MYSQLTEST_VARDIR/tmp/_rgeki_$_rgeki_suffix
--let _RPL_LOG_FILE= $rpl_log_file
--let _RPL_DEBUG= $rpl_debug

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
  open RFILE, "> $result_file" or die "Error opening $result_file: $!";
  if (!open LFILE, '<:raw', $log_file)
  {
    print RFILE "Error: unable to open the file" or die "Error writing to $result_file: $!";
  }
  else
  {
    my $error = 0;

    # Read binlog magic
    my $bytes_read = read LFILE, my $magic, 4;
    if ($bytes_read != 4)
    {
      print RFILE "Error: unable to read binlog magic" or die "Error writing to $result_file: $!";
      $error = 1;
    }

    if ($ENV{'_RPL_DEBUG'})
    {
      print "# debug: magic='$magic'\n";
    }

    my $plain_magic = "\xfe\x62\x69\x6e";
    my $encrypted_magic = "\xfd\x62\x69\x6e";

    while (!$error)
    {
      if ($magic eq $plain_magic) {
        # Ordinary binary log
        if ($ENV{'_RPL_DEBUG'})
        {
          print "# debug: plain log file\n";
        }
        print RFILE "None" or die "Error writing to $result_file: $!";
      } elsif ($magic eq $encrypted_magic) {
        # Encrypted binary log
        if ($ENV{'_RPL_DEBUG'})
        {
          print "# debug: encrypted log file\n";
        }
        $bytes_read = read LFILE, my $version, 1;
        if ($bytes_read != 1)
        {
          print RFILE "Error: unable to read encrypted header version number" or die "Error writing to $result_file: $!";
          last;
        }
        if ($version cmp "\x01")
        {
          print RFILE "Error: unexpected encryption header version number" or die "Error writing to $result_file: $!";
          last;
        }
        $bytes_read = read LFILE, my $field_type, 1;
        if ($bytes_read != 1)
        {
          print RFILE "Error: unable to read first field type" or die "Error writing to $result_file: $!";
          last;
        }
        if ($field_type cmp "\x01")
        {
          print RFILE "Error: unexpected field type" or die "Error writing to $result_file: $!";
          last;
        }
        $bytes_read = read LFILE, my $length, 1;
        if ($bytes_read != 1)
        {
          print RFILE "Error: unable to read key ID length" or die "Error writing to $result_file: $!";
          last;
        }
        $length = ord($length);
        $bytes_read = read LFILE, my $key_id, $length;
        if ($bytes_read != $length)
        {
          print RFILE "Error: unable to read the key ID" or die "Error writing to $result_file: $!";
          last;
        }
        print RFILE "$key_id" or die "Error writing to $result_file: $!";
      } else {
        print RFILE "Error: not a binary log file" or die "Error writing to $result_file: $!";
      }
      last;
    }
    close LFILE or die "Error closing $log_file: $!";
  }
  close RFILE or die "Error closing $result_file: $!";

EOF

--disable_query_log
--let $_rgeki_sql_log_bin= `SELECT @@SESSION.sql_log_bin`
SET @sql_log_bin=0;
CREATE TEMPORARY TABLE `_rgeki_` (msg TEXT);
--eval LOAD DATA INFILE '$_RPL_RESULT_FILE' INTO TABLE `_rgeki_`
--let $rpl_encryption_key_id=`SELECT msg FROM `_rgeki_` LIMIT 1`
DROP TEMPORARY TABLE `_rgeki_`;
--remove_file $_RPL_RESULT_FILE
--eval SET sql_log_bin=$_rgeki_sql_log_bin
--enable_query_log

--let _RPL_LOG_FILE=
--let _RPL_DEBUG=
--let _RPL_RESULT_FILE=
--let $rpl_debug=
