-- connection master
-- source include/rpl_reset.inc

-- connection master

CREATE TABLE t1 (a INT);
INSERT INTO t1 VALUES (1);
INSERT INTO t1 VALUES (2);
INSERT INTO t1 VALUES (3);

--let $is_relay_log= 0
--let $binlog_file= query_get_value(SHOW MASTER STATUS, File, 1)
--source extra/rpl_tests/rpl_show_log_events_with_varying_options.inc

--sync_slave_with_master
--let $binlog_file= query_get_value(SHOW MASTER STATUS, File, 1)
--source extra/rpl_tests/rpl_show_log_events_with_varying_options.inc

--let $is_relay_log= 1
--let $binlog_file= query_get_value(SHOW SLAVE STATUS, Relay_Log_File, 1)
--source extra/rpl_tests/rpl_show_log_events_with_varying_options.inc

FLUSH LOGS;

-- connection master
FLUSH LOGS;
DROP TABLE t1;

--let $is_relay_log= 0
--let $binlog_file= query_get_value(SHOW MASTER STATUS, File, 1)
--source extra/rpl_tests/rpl_show_log_events_with_varying_options.inc

--sync_slave_with_master
--let $binlog_file= query_get_value(SHOW MASTER STATUS, File, 1)
--source extra/rpl_tests/rpl_show_log_events_with_varying_options.inc

--let $is_relay_log= 1
--let $binlog_file= query_get_value(SHOW SLAVE STATUS, Relay_Log_File, 1)
--source extra/rpl_tests/rpl_show_log_events_with_varying_options.inc


# clear show_binlog_event/show_relaylog_events parameters
let $binlog_file= ;
let $binlog_limit= ;
