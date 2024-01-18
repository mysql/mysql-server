# Helper file to share the major test process for cases.

--source include/rpl/connection_replica1.inc

# Make sure source_pos_wait is waiting.
SET debug_sync = "now WAIT_FOR wait_func_begin";
--source include/rpl/stop_replica.inc

--send RESET REPLICA ALL

--let $rpl_connection_name= server_2
--source include/connection.inc

# RESET REPLICA ALL is blocked by source_pos_wait and show correct state.
--let $wait_condition= SELECT count(*) = 1 FROM performance_schema.threads WHERE PROCESSLIST_STATE = "Waiting for no channel reference."
--source include/wait_condition.inc

# resume source_pos_wait
SET debug_sync = "now signal wait_func_continue";

--source include/rpl/connection_replica1.inc
--reap
SET debug_sync = 'RESET';

--source include/rpl/connection_replica.inc
--reap
SET debug_sync = 'RESET';


# Reconfigure the default channel.
--source include/rpl/connection_source.inc
--source include/rpl/save_binlog_file_position.inc

--source include/rpl/connection_replica.inc
--replace_result $binlog_file BINLOG_FILE $binlog_position BINLOG_POS $MASTER_MYPORT SOURCE_PORT
eval CHANGE REPLICATION SOURCE TO SOURCE_LOG_FILE = "$binlog_file",
     SOURCE_LOG_POS = $binlog_position, SOURCE_USER='root',
     SOURCE_HOST='127.0.0.1', SOURCE_PORT=$MASTER_MYPORT;
