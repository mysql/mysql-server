# ==== Purpose ====
#
# To assert that all transactions in server's binary have the correct
# transaction length information.
#
# ==== Usage ====
#
# [--let $binlog_file= slave-relay-bin.000002]
# [--let $is_relay_log= 1]
# [--let $skip_output= 1]
# --source include/assert_transaction_length.inc
#
# Parameters:
#
# $binlog_file
#      The binary or relay log file to be inspected. If not specified,
#      the current server binary log file will be inspected.
#
# $is_relay_log
#      The log file to be inspected is a relay log. In a relay log
#      file is not possible to assert the transaction length of the
#      last transaction, as the end_log_pos information is not usable.
#
# $skip_output
#      When this option is set, the include will not display the results
#      with the summary of the log file evaluation.
#

--let $_atl_data_dir= `SELECT @@datadir`
--let $_atl_log_file= query_get_value(SHOW MASTER STATUS, File, 1)
if ($binlog_file)
{
  --let $_atl_log_file= $binlog_file
}
--let $_atl_is_relay_log= 0
if ($is_relay_log)
{
  --let $_atl_is_relay_log= 1
}

--let $_atl_prefix= `SELECT UUID()`
--let $_atl_out_file = $MYSQLTEST_VARDIR/tmp/$_atl_prefix.out
--let $_atl_err_file = $MYSQLTEST_VARDIR/tmp/$_atl_prefix.err
--let $_atl_dump_file = $MYSQLTEST_VARDIR/tmp/$_atl_prefix.sql
--exec $MYSQL_BINLOG --force-read --force-if-open $_atl_data_dir/$_atl_log_file > $_atl_dump_file

--let ATL_OUT_FILE = $_atl_out_file
--let ATL_ERR_FILE = $_atl_err_file
--let ATL_DUMP_FILE = $_atl_dump_file
--let ATL_IS_RELAY_LOG = $_atl_is_relay_log

if (!$skip_output)
{
  --echo Asserting all GTID transaction length information in '$_atl_log_file'
}

perl;
  use strict;

  my $dump = $ENV{'ATL_DUMP_FILE'} or die "ATL_DUMP_FILE not set";
  my $out = $ENV{'ATL_OUT_FILE'} or die "ATL_OUT_FILE not set";
  my $err = $ENV{'ATL_ERR_FILE'} or die "ATL_ERR_FILE not set";
  my $is_relay_log = $ENV{'ATL_IS_RELAY_LOG'};
  open(FILE, "$dump") or die("Unable to open $dump: $!\n");

  my $end_log_pos= 0;
  my $error= 0;
  my $line;
  my $last_log_pos= 0;
  my $log_pos= 0;
  my $trx_counter= 0;
  my $trx_gtid= "n/a";
  my $trx_size= 0;
  my $trx_start= 0;
  my $trx_remaining_size= 0;

  sub trx_too_small
  {
    $trx_remaining_size= abs($trx_remaining_size);
    my $calculated_length= $trx_size + $trx_remaining_size;
    open ERR, "> $err" or die("Error $? opening $err: $!");
    print ERR "\n";
    print ERR "Wrong transaction length found for ($trx_gtid) at log file position = $trx_start.\n";
    print ERR "Transaction length information ($trx_size) is smaller than calculated length ($calculated_length).\n";
    print ERR "Transaction is over sized by $trx_remaining_size bytes at log file position = $log_pos.";
    close ERR or die("Error $? closing $err: $!");
    $error= 1;
  }

  sub trx_too_big
  {
    my $calculated_length= $trx_size - $trx_remaining_size;
    open ERR, "> $err" or die("Error $? opening $err: $!");
    print ERR "\n";
    print ERR "Wrong transaction length found for ($trx_gtid) at log file position = $trx_start.\n";
    print ERR "Transaction length information ($trx_size) is bigger than calculated length ($calculated_length).\n";
    print ERR "Transaction is missing $trx_remaining_size bytes at log file position = $log_pos.";
    close ERR or die("Error $? closing $err: $!");
    $error= 1;
  }

  while (my $line = <FILE>)
  {
    chomp $line;

    # Event begin
    if ($line =~ m/^# at /)
    {
      $last_log_pos = $log_pos;
      $log_pos = $line;
      $log_pos =~ s/^# at //g;
      # Consume the event size from the transaction size
      if ($trx_size > 0)
      {
        $trx_remaining_size = $trx_remaining_size - ($log_pos - $last_log_pos);
        if ($trx_remaining_size < 0)
        {
          trx_too_small();
          last;
        }
      }
    }

    # Assume end of transaction on INCIDENT or ROTATE with server id 0
    if ($line =~ m/^# Incident:/ ||
        $line =~ m/^#.*server id 0.*Rotate/)
    {
      # If we found an INCIDENT, all the events from the last transaction are consumed
      if ($trx_remaining_size != 0)
      {
        trx_too_big();
        last;
      }
      $trx_size = 0;
      $trx_remaining_size = 0;
      $trx_start = 0;
      $trx_gtid = "n/a";
    }

    # Store the end log pos to be used on the last transaction of binary logs
    if ($line =~ m/^#.*end_log_pos/)
    {
      $end_log_pos = $line;
      $end_log_pos =~ s/.*end_log_pos //g;
      $end_log_pos =~ s/ .*//g;
    }

    # Store the GTID being processed (just informed on errors)
    if ($line =~ m/^SET.*SESSION.GTID_NEXT=/)
    {
      $trx_gtid = $line;
      $trx_gtid =~ s/.*GTID_NEXT= \'//g; #'
      $trx_gtid =~ s/\'.*//g;            #'
    }

    # Gtid information
    if ($line =~ m/^#.*original_committed_timestamp.*transaction_length/ )
    {
      # If we found a GTID, all the events from the last transaction are consumed
      if ($trx_remaining_size != 0)
      {
        trx_too_big();
        last;
      }
      $trx_size = $line;
      $trx_size =~ s/.*transaction_length=//g;
      $trx_remaining_size = $trx_size;
      $trx_start = $log_pos;
      $trx_gtid = "n/a";
      $trx_counter++;
    }
  }

  close(FILE) or die("Error $? closing $dump: $!");

  if (!$is_relay_log)
  {
    # Consumes the last event by using end_log_pos
    if ($error == 0 && $trx_remaining_size > 0)
    {
      $trx_remaining_size = $trx_remaining_size - ($end_log_pos - $log_pos);
      if ($trx_remaining_size < 0)
      {
        trx_too_small();
      }
      if ($trx_remaining_size > 0)
      {
        trx_too_big();
      }
    }
  }
  else
  {
    # Discard last transaction information
    if ($trx_size > 0 && $trx_remaining_size > 0)
    {
      $trx_counter--;
      $trx_size = 0;
      $trx_remaining_size = 0;
    }
  }

  open OUT, "> $out" or die("Error $? opening $out: $!");
  if ($error)
  {
    print OUT ("assert_transaction_length.inc failed");
  }
  else {
    if ($trx_counter > 0)
    {
      print OUT "Inspected $trx_counter transactions, all with correct transaction length.";
    }
    else
    {
      print OUT "Log file had no transactions with length information.";
    }
  }
  close OUT or die("Error $? closing $out: $!");

EOF

# Load result
--let $_atl_outcome= `SELECT LOAD_FILE('$_ATL_OUT_FILE')`

# Cleanup
--remove_file $_atl_out_file
--remove_file $_atl_dump_file

# If failed, show extra info
if ($_atl_outcome == 'assert_transaction_length.inc failed')
{
  --let $extra_debug_info= `SELECT LOAD_FILE('$_ATL_ERR_FILE')`
  --remove_file $_atl_err_file
  --source include/show_rpl_debug_info.inc
  --echo Error asserting GTID transaction length information in '$_atl_log_file'
  --die assert_transaction_length.inc failed.
}

# Display successful info if requested
if ($_atl_outcome != 'assert_transaction_length.inc failed')
{
  if (!$skip_output)
  {
    --echo $_atl_outcome
  }
}
