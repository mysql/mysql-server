# Helper file to share the major test process for cases.

--source include/rpl_connection_slave1.inc

# Make sure master_pos_wait is waiting.
SET debug_sync = "now WAIT_FOR wait_func_begin";
--source include/stop_slave.inc

--send RESET SLAVE ALL

--let $rpl_connection_name= server_2
--source include/rpl_connection.inc

# RESET SLAVE ALL is blocked by master_pos_wait and show correct state.
--let $wait_condition= SELECT count(*) = 1 FROM performance_schema.threads WHERE PROCESSLIST_STATE = "Waiting for no channel reference."
--source include/wait_condition.inc

# resume master_pos_wait
SET debug_sync = "now signal wait_func_continue";

--source include/rpl_connection_slave1.inc
--reap
SET debug_sync = 'RESET';

--source include/rpl_connection_slave.inc
--reap
SET debug_sync = 'RESET';


# Reconfigure the default channel.
--source include/rpl_connection_master.inc
--source include/save_binlog_position.inc

--source include/rpl_connection_slave.inc
--replace_result $binlog_file BINLOG_FILE $binlog_position BINLOG_POS $MASTER_MYPORT MASTER_PORT
eval CHANGE MASTER TO MASTER_LOG_FILE = "$binlog_file",
     MASTER_LOG_POS = $binlog_position, MASTER_USER='root',
     MASTER_HOST='127.0.0.1', MASTER_PORT=$MASTER_MYPORT;
