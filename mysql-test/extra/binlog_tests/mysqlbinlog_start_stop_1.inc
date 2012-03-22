# ==== Purpose ====
#
# Auxiliary file used by main.mysqlbinlog-start-stop.
#
# This file runs mysqlbinlog with a fixed set of parameters specifying
# the source binlog, but varying how the start and stop positions are
# specified.
#
# ==== Usage ====
#
# --let $start_position= <binlog offset>
# --let $stop_position= <binlog offset>
# --let $start_datetime= <timestamp>
# --let $stop_datetime= <timestamp>
# --let $extra_options= <options to mysqlbinlog>
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
#   $extra_options
#     Options for mysqlbinlog, specifying which input file to read.

--let $option= --base64-output=never
--source extra/binlog_tests/mysqlbinlog_start_stop_2.inc

--let $option= --offset=2
--source extra/binlog_tests/mysqlbinlog_start_stop_2.inc

--let $option= --start-position=$start_position
--source extra/binlog_tests/mysqlbinlog_start_stop_2.inc

--let $option= --stop-position=$stop_position
--source extra/binlog_tests/mysqlbinlog_start_stop_2.inc

--let $option= --start-position=$start_position --stop-position=$stop_position
--source extra/binlog_tests/mysqlbinlog_start_stop_2.inc

--let $option= "--start-datetime=$start_datetime"
--source extra/binlog_tests/mysqlbinlog_start_stop_2.inc

--let $option= "--stop-datetime=$stop_datetime"
--source extra/binlog_tests/mysqlbinlog_start_stop_2.inc

--let $option= "--start-datetime=$start_datetime" "--stop-datetime=$stop_datetime"
--source extra/binlog_tests/mysqlbinlog_start_stop_2.inc
