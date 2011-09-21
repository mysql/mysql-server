# Purpose
# Ensure server has v2 Binlog row events
#

--require r/have_binlog_row_v2.require
disable_query_log;
show variables like 'log_bin_use_v1_row_events';
enable_query_log;