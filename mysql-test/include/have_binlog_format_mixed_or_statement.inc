--require r/have_binlog_format_statement.require
--disable_query_log
--replace_result MIXED STATEMENT
show variables like "binlog_format";
--enable_query_log
