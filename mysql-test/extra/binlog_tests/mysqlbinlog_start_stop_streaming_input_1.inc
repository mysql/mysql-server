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

--let $option= --base64-output=never
--let $error= 0
--source extra/binlog_tests/mysqlbinlog_start_stop_streaming_input_2.inc

--let $option= --offset=2
--let $error= 0
--source extra/binlog_tests/mysqlbinlog_start_stop_streaming_input_2.inc

--let $option= --start-position=$start_position
--let $error= 0
--source extra/binlog_tests/mysqlbinlog_start_stop_streaming_input_2.inc

--let $option= --stop-position=$stop_position
--let $error= 1
--source extra/binlog_tests/mysqlbinlog_start_stop_streaming_input_2.inc

--let $option= --start-position=$start_position --stop-position=$stop_position
--let $error= 1
--source extra/binlog_tests/mysqlbinlog_start_stop_streaming_input_2.inc

--let $option= "--start-datetime=$start_datetime"
--let $error= 0
--source extra/binlog_tests/mysqlbinlog_start_stop_streaming_input_2.inc

--let $option= "--stop-datetime=$stop_datetime"
--let $error= 0
--source extra/binlog_tests/mysqlbinlog_start_stop_streaming_input_2.inc

--let $option= "--start-datetime=$start_datetime" "--stop-datetime=$stop_datetime"
--let $error= 0
--source extra/binlog_tests/mysqlbinlog_start_stop_streaming_input_2.inc
