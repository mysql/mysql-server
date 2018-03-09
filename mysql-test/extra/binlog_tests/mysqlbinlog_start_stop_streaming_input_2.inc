# ==== Purpose ====
#
# Auxiliary file used by mysql-test/extra/binlog_tests/mysqlbinlog_start_stop_streaming_input_1.inc
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

if ($error)
{
  --error 1
  --exec cat $file_options | $MYSQL_BINLOG --short-form $option -
}
if (!$error)
{
  --exec cat $file_options | $MYSQL_BINLOG --short-form $option -
}
