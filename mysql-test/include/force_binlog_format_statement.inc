--source include/have_log_bin.inc

--disable_query_log
SET @previous_binlog_format__htnt542nh=@@GLOBAL.binlog_format;
SET @@GLOBAL.binlog_format=STATEMENT;
SET binlog_format=STATEMENT;
--enable_query_log
