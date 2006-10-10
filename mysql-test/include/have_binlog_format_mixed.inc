-- require r/have_binlog_format_mixed.require
disable_query_log;
show variables like "binlog_format";
enable_query_log;
