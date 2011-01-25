# Include file for rpl_show_log_events.inc 

--let $binlog_limit=
--source extra/rpl_tests/rpl_show_binlog_events.inc
--let $binlog_limit= 1
--source extra/rpl_tests/rpl_show_binlog_events.inc
--let $binlog_limit= 1,3
--source extra/rpl_tests/rpl_show_binlog_events.inc
--let $binlog_limit=
--let $binlog_file=
--source extra/rpl_tests/rpl_show_binlog_events.inc 
