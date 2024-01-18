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
# --source common/binlog/mysqlbinlog_start_stop_1.inc
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

--let $options= --base64-output=never
--source common/binlog/mysqlbinlog_start_stop_2.inc

--let $options= --offset=2
--source common/binlog/mysqlbinlog_start_stop_2.inc

--let $options= --start-position=$start_position
--source common/binlog/mysqlbinlog_start_stop_2.inc

--let $options= --stop-position=$stop_position
--source common/binlog/mysqlbinlog_start_stop_2.inc

--let $options= --start-position=$start_position --stop-position=$stop_position
--source common/binlog/mysqlbinlog_start_stop_2.inc

--let $options= "--start-datetime=$start_datetime"
--source common/binlog/mysqlbinlog_start_stop_2.inc

--let $options= "--stop-datetime=$stop_datetime"
--source common/binlog/mysqlbinlog_start_stop_2.inc

--let $options= "--start-datetime=$start_datetime" "--stop-datetime=$stop_datetime"
--source common/binlog/mysqlbinlog_start_stop_2.inc
