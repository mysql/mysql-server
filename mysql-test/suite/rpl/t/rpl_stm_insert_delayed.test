--source include/have_binlog_format_statement.inc
--source include/master-slave.inc
--source include/not_embedded.inc
--source include/not_windows.inc

disable_query_log;
call mtr.add_suppression("Unsafe statement written to the binary log using statement format since BINLOG_FORMAT = STATEMENT");
enable_query_log;
--source extra/rpl_tests/rpl_insert_delayed.test
--source include/rpl_end.inc
