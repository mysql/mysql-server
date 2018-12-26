# ==== Purpose ====
#
# Auxiliary file used by main.mysqlbinlog-start-stop-streaming-input.
#
# This file runs mysqlbinlog for a streaming file with a fixed set of parameters
# specifying the source binlog, but varying how the start and stop positions/
# datetime are specified.
#
# ==== Usage ====
#
# --let $start_position= <binlog offset>
# --let $stop_position= <binlog offset>
# --let $start_datetime= <timestamp>
# --let $stop_datetime= <timestamp>
# --let $file_options= <Streaming input files to mysqlbinlog>
# --source extra/binlog_tests/mysqlbinlog_start_stop_1.inc
#
# Parameters:
#   $start_position
#     Offset to pass to --start-position
#   $stop_position
#     Offset to pass to --stop-position
#   $start_datetime
#     Offset to pass to --start-datetime
#   $stop_datetime
#     Offset to pass to --stop-datetime
#   $file_options
#     Specifying which input file to read.

--let $result_file_options = $MYSQLTEST_VARDIR/tmp/compressed_files.sql
--let $option= --base64-output=never
--let $error= 0
--source extra/binlog_tests/mysqlbinlog_start_stop_streaming_input_2.inc

--let $option= --offset=2
--let $error= 0
--source extra/binlog_tests/mysqlbinlog_start_stop_streaming_input_2.inc

--let $option= --start-position=$start_position
--let $error= 0
--source extra/binlog_tests/mysqlbinlog_start_stop_streaming_input_2.inc
--let $assert_text = "The events before start position should not appear in the log file"
--let $assert_file = $result_file_options
--let $assert_select = INSERT INTO t1 VALUES\(1\)
--let assert_count = 0
--source include/assert_grep.inc

--let $option= --stop-position=$stop_position
--let $error= 1
--source extra/binlog_tests/mysqlbinlog_start_stop_streaming_input_2.inc

--let $option= --start-position=$start_position --stop-position=$stop_position
--let $error= 1
--source extra/binlog_tests/mysqlbinlog_start_stop_streaming_input_2.inc

--let $option= "--start-datetime=$start_datetime"
--let $error= 0
--source extra/binlog_tests/mysqlbinlog_start_stop_streaming_input_2.inc
--let $assert_text = "The events after start datetime should appear in the log file"
--let $assert_file = $result_file_options
--let $assert_select = INSERT INTO t1 VALUES\(1\)
--let assert_count = 1
--source include/assert_grep.inc

--let $assert_text = "The events before start datetime should not appear in the log file"
--let $assert_file = $result_file_options
--let $assert_select = CREATE TABLE t1 \(\a INT\)\;
--let assert_count = 0
--source include/assert_grep.inc

--let $option= "--stop-datetime=$stop_datetime"
--let $error= 0
--source extra/binlog_tests/mysqlbinlog_start_stop_streaming_input_2.inc

--let $option= "--start-datetime=$start_datetime" "--stop-datetime=$stop_datetime"
--let $error= 0
--source extra/binlog_tests/mysqlbinlog_start_stop_streaming_input_2.inc

#cleanup
--remove_file $result_file_options
