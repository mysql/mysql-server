# ==== Purpose ====
#
# Auxiliary file used by mysql-test/common/binlog/mysqlbinlog_start_stop_streaming_input_1.inc
#
# This prints a header, then runs mysqlbinlog once with given parameters.
#
# ==== Usage ====
#
# --let $options= X
# --let $file_options= Y
# --let $error= Z
# --source include/mysqlbinlog-start-stop-streaming-input_2.inc
#
# Parameters:
#
#   $options
#     Options that will be printed as a header in the test log.
#   $file_options
#     Streaming input files to mysqlbinlog
#   $error
#     Flag to determine if the execution expects an error or not

# Pretty-print $option_text
--let $option_text= `SELECT SUBSTR(REPLACE('$option', '"', ''), 3)`
while (`SELECT LOCATE('=', '$option_text')`)
{
  --let $dash_pos= `SELECT LOCATE('--', '$option_text')`
  --let $option_text= `SELECT CONCAT(SUBSTR('$option_text', 1, LOCATE('=', '$option_text') - 1), IF($dash_pos, CONCAT(' ', SUBSTR('$option_text', $dash_pos + 2)), ''))`
}
--echo
--echo ---- $option_text with streaming input----

# Print output
--replace_regex /SET @@SESSION.GTID_NEXT= '.*'/SET @@SESSION.GTID_NEXT= 'GTID';/ /([0-9A-F\-]{36})\:[0-9]+\-[0-9]+/UUID:#-#/ /_timestamp=[0-9A-Za-z:\(\)\. \-]*/_timestamp=TIMESTAMP/

--let _BINLOG_FILE= $file_options
--let _RESULT_FILE= $result_file_options
--let _MYSQLBINLOG= $MYSQL_BINLOG
--let _OPTIONS= $option
--let _ERROR= $error
# use this to find out which command to use 'cat' or 'type'
--let _IS_WINDOWS = 0
if (`SELECT CONVERT(@@VERSION_COMPILE_OS USING latin1) IN ('Win32', 'Win64', 'Windows')`)
{
  --let _IS_WINDOWS= 1
}
--perl
  use strict;
  use warnings;
  my $binlog_file_perl = $ENV{'_BINLOG_FILE'};
  my $result_file_perl = $ENV{'_RESULT_FILE'};
  my $mysqlbinlog = $ENV{'_MYSQLBINLOG'};
  my $is_windows = $ENV{'_IS_WINDOWS'};
  my $options = $ENV{'_OPTIONS'};
  my $error = $ENV{'_ERROR'};

  if ($is_windows)
  {
    $binlog_file_perl =~ tr{/}{\\};
    if ($error)
    {
      my $result = `type  $binlog_file_perl | $mysqlbinlog  $options -`;
      if (length $result)
      {
        die;
      }
    }
    if (!$error)
    {
      my $result = `type  $binlog_file_perl | $mysqlbinlog  $options - > $result_file_perl`;
    }
  }
  else
  {
    if ($error)
    {
      my $result = `cat  $binlog_file_perl | $mysqlbinlog  $options -`;
      if (length $result)
      {
        die;
      }
    }
    if (!$error)
    {
      my $result = `cat  $binlog_file_perl | $mysqlbinlog  $options - > $result_file_perl`;
    }
  }
EOF
