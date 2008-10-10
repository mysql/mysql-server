--source include/have_log_bin.inc

-- require r/have_binlog_format_row.require
disable_query_log;
show variables like 'binlog_format';
enable_query_log;
