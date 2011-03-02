-- connection master
-- source include/rpl_reset.inc

-- connection master

CREATE TABLE t1 (a INT);
INSERT INTO t1 VALUES (1);
INSERT INTO t1 VALUES (2);
INSERT INTO t1 VALUES (3);

# PART I
#
# SHOWs contents of binary logs on the master and both, binary and
# relay logs, on the slave.
#

--let $is_relay_log= 0
--let $binlog_file= query_get_value(SHOW MASTER STATUS, File, 1)
--source extra/rpl_tests/rpl_show_log_events_with_varying_options.inc

--sync_slave_with_master
--let $binlog_file= query_get_value(SHOW MASTER STATUS, File, 1)
--source extra/rpl_tests/rpl_show_log_events_with_varying_options.inc

--let $is_relay_log= 1
--let $binlog_file= query_get_value(SHOW SLAVE STATUS, Relay_Log_File, 1)
--source extra/rpl_tests/rpl_show_log_events_with_varying_options.inc

#
# PART II
#
# Although this second part of the test may seem redudant it is
# actually needed to assert that SHOW RELAYLOG EVENTS works properly
# with respect to the ordering of the relay log in relay-log.index. 
#
# If no file is specified with "IN" then first relay log file in
# relay-log.index (ie, the oldest one) should be picked and its
# contents displayed. The same happens for SHOW BINLOG EVENTS, so we
# show them both. All in all, this is the reason for re-assert after
# MASTER and SLAVE's FLUSH LOGS operations.
#

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
