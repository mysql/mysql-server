# ==== Purpose ====
#
# Execute a command and check:
#  - the exit status is as specified
#  - the output (stdout) matches a given regex

# ==== Usage ====

--let $include_filename= assert_command_output.inc
--source include/begin_include_file.inc

if ($rpl_debug)
{
  --echo # debug: assert_command='$assert_command' assert_regex='$assert_regex' assert_negated='$assert_negated' assert_status='$assert_status'
}

--let _ASSERT_COMMAND= $assert_command
--let _ASSERT_REGEX= $assert_regex
--let _ASSERT_NEGATED= $assert_negated
--let _ASSERT_STATUS= $assert_status
--let $_assert_suffix= `SELECT UUID()`
--let _ASSERT_ERROR_FILE= $MYSQLTEST_VARDIR/tmp/_assert_$_assert_suffix.inc
--let _ASSERT_DEBUG= $rpl_debug

if ($rpl_debug)
{
  --echo # debug: assert_error_file='$_ASSERT_ERROR_FILE'
}

perl;
  my $cmd= $ENV{'_ASSERT_COMMAND'};
  my $positive= $ENV{'_ASSERT_NEGATED'} ? 0 : 1;
  my $regex= $ENV{'_ASSERT_REGEX'};
  my $status= $ENV{'_ASSERT_STATUS'};
  my $debug= $ENV{'_ASSERT_DEBUG'};
  my $output= `$cmd`;
  my $error= 0;
  $status= 0 if $status eq '';
  if ($status ne '*' and ($status ? $? >> 8 != $status : $? != 0))
  {
    print "ERROR: Command returned wrong exit status! " .
      "Expected '$status', got '".($?>>8)."'\n";
    $error= 1;
  }
  if ((($output =~ m{$regex}ms) ? 1 : 0) != $positive)
  {
    print "ERROR: Command produced wrong output!\n";
    print "ERROR: Command: '$cmd'\n";
    print "ERROR: Output expected to " . ($positive?'':'not '). "match " .
      "perl regex: '$regex'\n";
    $error= 1;
  }
  if (!$error)
  {
    my $file= $ENV{_ASSERT_ERROR_FILE};
    open FILE, "> $file" or die "Error opening $file: $!";
    print FILE "X" or die "Error writing to $file: $!";
    close FILE or die "Error closing $file: $!";
  }
  if ($error || $debug)
  {
    print "======== BEGIN output ========\n$output\n" .
      "======== END OUTPUT ========\n";
  }
EOF

--file_exists $_ASSERT_ERROR_FILE
--remove_file $_ASSERT_ERROR_FILE

--let $include_filename= assert_command_output.inc
--source include/end_include_file.inc
