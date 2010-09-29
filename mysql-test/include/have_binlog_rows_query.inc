# ==== Purpose ====
# 
# Ensure that the server is running with binlog_rows_query_log_events on
#
# ==== Usage ====
#
# source include/have_binlog_rows_query.inc;

-- require r/have_binlog_rows_query.require
disable_query_log;
--replace_result ON OFF
show variables like 'binlog_rows_query_log_events';
enable_query_log;
