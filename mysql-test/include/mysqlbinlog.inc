# ==== Purpose ====
#
# Run mysqlbinlog and filter out nondeterministic output.
#
# ==== Usage ====
#
# --let $mysqlbinlog_parameters= "PARAMETERS"
# [--let $mysqlbinlog_pipe= "| FILTER > FILE"]
# --source include/mysqlbinlog.inc
#
# Parameters:
#
#   $binlog_fullpath
#     Filename to use.
#
#   $binlog_position
#     Position to use.
#
#   $mysqlbinlog_parameters
#     Parameters to pass to mysqlbinlog
#
#   $mysqlbinlog_skip_binlog_statements
#     Remove all BINLOG statements from the output.
#     This requires 'source include/have_util_sed.inc' in the main test script.
#
#   $mysqlbinlog_only_decoded_rows
#     Only include decoded rows. This adds the argument -v and filters
#     the output so only files beginning with '###' are printed.
#     This requires 'source include/have_grep.inc' in the main test script.
#
#   $mysqlbinlog_pipe
#     Optional redirection of output.
#
#   $mysqlbinlog_capture_output
#     Instead of writing the output from the command to the result log,
#     save it in the mysqltest variable $result
#
#   $mysqlbinlog_skip_replace
#     DEBUGGING ONLY: Print unfiltered output to the log. This should
#     never be used in production since output will be
#     nondeterministic.
#
#   $mysqlbinlog_allow_error
#     If set, it is allowed for the command to fail with error 1. This
#     is useful in conjuction with $mysqlbinlog_only_decoded_rows, in case
#     no decoded rows are found.

--let $include_filename= mysqlbinlog.inc
--source include/begin_include_file.inc


--let $mysqlbinlog_extra_parameters=
if ($binlog_fullpath)
{
  --let $mysqlbinlog_extra_parameters= $binlog_fullpath
}
if ($binlog_position)
{
  --let $mysqlbinlog_extra_parameters= $mysqlbinlog_extra_parameters --start-position=$binlog_position
}

--let $mysqlbinlog_sed_pipe=
if ($mysqlbinlog_skip_binlog_statements)
{
  if ($SED == '')
  {
    --die !!!ERROR IN TEST: source include/have_util_sed.inc before using mysqlbinlog.inc with $mysqlbinlog_skip_binlog_statements set
  }
  --let $mysqlbinlog_sed_pipe= | $SED '/^BINLOG/,/\\/\\*!\\*\\//d'
}

--let $mysqlbinlog_grep_pipe=
if ($mysqlbinlog_only_decoded_rows)
{
  if ($GREP == '')
  {
    --die !!!ERROR IN TEST: source include/have_grep.inc before using mysqlbinlog.inc with $mysqlbinlog_only_decoded_rows set
  }
  --let $mysqlbinlog_grep_pipe= | $GREP '^### '
  --let $mysqlbinlog_extra_parameters= $mysqlbinlog_extra_parameters -v
}

if ($mysqlbinlog_skip_replace)
{
  --echo DO_NOT_CHECK_IN_THIS_LINE: printing unfiltered mysqlbinlog output. This should only be used for debugging. Never check in a result file that contains unfiltered mysqlbinlog output.
}
if (!$mysqlbinlog_skip_replace)
{
  --replace_result $MYSQLTEST_VARDIR MYSQLTEST_VARDIR
  replace_regex
    /TIMESTAMP=[0-9]*/TIMESTAMP=#/
    /#[0-9]*[ ]*[0-9]*:[0-9]*:[0-9]* server id [0-9]*/# # server id #/
    /SQL_LOAD_MB-[0-9]-[0-9]/SQL_LOAD_MB-#-#/
    /exec_time=[0-9]*/exec_time=#/
    /last_committed=[0-9]*/last_committed=#/
    /sequence_number=[0-9]*/sequence_number=#/
    /original_committed_timestamp=[0-9]*.immediate_commit_timestamp=[0-9]*/original_committed_timestamp=#   immediate_commit_timestamp=#/
    /transaction_length=[0-9]*/transaction_length=#/
    /end_log_pos [0-9]*/end_log_pos #/
    /# at [0-9]*/# at #/
    /Xid = [0-9]*/Xid = #/
    /thread_id=[0-9]*/thread_id=#/
    /table id [0-9]*/table id #/
    /mapped to number [0-9]*/mapped to number #/
    /server v [^ ]*/server v #/
    /Start: binlog v [0-9]*/Start: binlog v #/
    /created [0-9]*[ ]*[0-9]*:[0-9]*:[0-9]* at startup/created # at startup/
    /(@[0-9]*=[0-9-]*[.][0-9]{1,3})[0-9e+-]*[^ ]*[ ]*(.*(FLOAT|DOUBLE).*[*].)/\1... \2/
    /# immediate_commit_timestamp=[0-9]* \([0-9]*-[0-9]*-[0-9]* [0-9]*:[0-9]*:[0-9]*.[0-9]* GMT\)/# immediate_commit_timestamp= MICROSECONDS-FROM-EPOCH (YYYY-MM-DD HOURS:MINUTES:SECONDS TZ)/
    /# original_commit_timestamp=[0-9]* \([0-9]*-[0-9]*-[0-9]* [0-9]*:[0-9]*:[0-9]*.[0-9]* GMT\)/# original_commit_timestamp= MICROSECONDS-FROM-EPOCH (YYYY-MM-DD HOURS:MINUTES:SECONDS TZ)/
    /SET @@session.original_commit_timestamp=[0-9]*/SET @@session.original_commit_timestamp= MICROSECONDS-FROM-EPOCH/
    /SET @@SESSION.GTID_NEXT= '.*'/SET @@SESSION.GTID_NEXT= '#'/
    /CRC32 0x[0-9a-f]{8}/CRC32 #/
    /# [a-f0-9]{8}-[a-f0-9]{4}-[a-f0-9]{4}-[a-f0-9]{4}-[a-f0-9]{12}:/# #:/;
}

--let $mysqlbinlog_cmd= $MYSQL_BINLOG $mysqlbinlog_parameters $mysqlbinlog_extra_parameters $mysqlbinlog_grep_pipe $mysqlbinlog_sed_pipe $mysqlbinlog_pipe

if ($rpl_debug)
{
  --echo DEBUG: Executing '$mysqlbinlog_cmd'
}

if (!$mysqlbinlog_capture_output)
{
  if ($mysqlbinlog_allow_error)
  {
    --error 0, 1
    --exec $mysqlbinlog_cmd
  }
  if (!$mysqlbinlog_allow_error)
  {
    --exec $mysqlbinlog_cmd
  }
}
if ($mysqlbinlog_capture_output)
{
  --let $command= $mysqlbinlog_cmd
  --source include/get_command_output.inc

}

--let $include_filename= mysqlbinlog.inc
--source include/end_include_file.inc
