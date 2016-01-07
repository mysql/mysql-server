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
#   $mysqlbinlog_parameters
#     Parameters to pass to mysqlbinlog
#
#   $mysqlbinlog_pipe
#     Optional redirection of output.

--let $include_filename= mysqlbinlog.inc
--source include/begin_include_file.inc


if ($rpl_debug)
{
  --echo $MYSQL_BINLOG $mysqlbinlog_parameters $mysqlbinlog_pipe
}

if ($mysqlbinlog_skip_replace)
{
  --echo DO_NOT_CHECK_IN_THIS_LINE: printing unfiltered mysqlbinlog output. This should only be used for debugging. Never check in a result file that contains unfiltered mysqlbinlog output.
}
if (!$mysqlbinlog_skip_replace)
{
  --replace_result $MYSQLTEST_VARDIR MYSQLTEST_VARDIR
  --replace_regex /TIMESTAMP=[0-9]*/TIMESTAMP=#/ /#[0-9]*[ ]*[0-9]*:[0-9]*:[0-9]* server id [0-9]*/# # server id #/ /SQL_LOAD_MB-[0-9]-[0-9]/SQL_LOAD_MB-#-#/ /exec_time=[0-9]*/exec_time=#/ /last_committed=[0-9]*/last_committed=#/ /sequence_number=[0-9]*/sequence_number=#/ /end_log_pos [0-9]*/end_log_pos #/ /# at [0-9]*/# at #/ /Xid = [0-9]*/Xid = #/ /thread_id=[0-9]*/thread_id=#/ /table id [0-9]*/table id #/ /mapped to number [0-9]*/mapped to number #/ /server v [^ ]*/server v #/ /Start: binlog v [0-9]*/Start: binlog v #/ /created [0-9]*[ ]*[0-9]*:[0-9]*:[0-9]* at startup/created # at startup/ /(@[0-9]*=[0-9-]*[.][0-9]{1,3})[0-9e+-]*[^ ]*[ ]*(.*(FLOAT|DOUBLE).*[*].)/\1... \2/ /SET @@SESSION.GTID_NEXT= '.*'/SET @@SESSION.GTID_NEXT= '#'/ /CRC32 0x[0-9a-f]{8}/CRC32 #/ /# [a-f0-9]{8}-[a-f0-9]{4}-[a-f0-9]{4}-[a-f0-9]{4}-[a-f0-9]{12}:/# #:/
}
--exec $MYSQL_BINLOG $mysqlbinlog_parameters $mysqlbinlog_pipe


--source include/end_include_file.inc
