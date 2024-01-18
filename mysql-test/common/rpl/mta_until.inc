--source include/rpl/connection_replica.inc
--source include/rpl/stop_applier.inc
--source include/rpl/connection_source.inc

SET debug="d,set_commit_parent_100";
eval INSERT INTO db1.t1 VALUES(1, '$until_type');
eval INSERT INTO db1.t1 VALUES(2, '$until_type');
eval INSERT INTO db2.t1 VALUES(1, '$until_type');
eval INSERT INTO db2.t1 VALUES(2, '$until_type');
eval INSERT INTO db3.t1 VALUES(1, '$until_type');
eval INSERT INTO db3.t1 VALUES(2, '$until_type');
FLUSH BINARY LOGS;
eval INSERT INTO db1.t1 VALUES(3, '$until_type');
eval INSERT INTO db1.t1 VALUES(4, '$until_type');
eval INSERT INTO db2.t1 VALUES(3, '$until_type');
eval INSERT INTO db2.t1 VALUES(4, '$until_type');
eval INSERT INTO db3.t1 VALUES(3, '$until_type');
eval INSERT INTO db3.t1 VALUES(4, '$until_type');

if ($until_type == 'SOURCE_POS')
{
  --let $source_log_file= query_get_value(SHOW BINARY LOG STATUS, File, 1)
  --let $source_log_pos= query_get_value(SHOW BINARY LOG STATUS, Position, 1)
}

if ($until_type == 'RELAY_POS')
{
  --source include/rpl/sync_to_replica_received.inc
  --source include/rpl/save_relay_log_file_position.inc
  --source include/rpl/connection_source.inc
}

if ($until_type == 'BEFORE_GTIDS')
{
  SET @gtids_should_applied = @@GLOBAL.gtid_executed;
}

eval INSERT INTO db1.t1 VALUES(5, '$until_type');

if ($until_type == 'BEFORE_GTIDS')
{
  --let $before_gtids= `SELECT gtid_subtract(@@GLOBAL.gtid_executed, @gtids_should_applied)`
}

eval INSERT INTO db1.t1 VALUES(6, '$until_type');
eval INSERT INTO db2.t1 VALUES(5, '$until_type');
eval INSERT INTO db2.t1 VALUES(6, '$until_type');
eval INSERT INTO db3.t1 VALUES(5, '$until_type');
eval INSERT INTO db3.t1 VALUES(6, '$until_type');
--source include/rpl/save_server_position.inc

--let $rpl_connection_name= server_2
--source include/connection.inc
BEGIN;
INSERT INTO db1.t1 VALUES(4, "block a worker");

--source include/rpl/connection_replica.inc

--let $__until_clause=
if ($until_type == 'SOURCE_POS')
{
  --replace_result $source_log_file SOURCE_LOG_FILE $source_log_pos SOURCE_LOG_POS
  --let $__until_clause= SOURCE_LOG_FILE="$source_log_file", SOURCE_LOG_POS=$source_log_pos
}

if ($until_type == 'RELAY_POS')
{
  --replace_result $relay_log_file RELAY_LOG_FILE $relay_log_size RELAY_LOG_POS
  --let $__until_clause= RELAY_LOG_FILE="$relay_log_file", RELAY_LOG_POS=$relay_log_size
}

if ($until_type == 'BEFORE_GTIDS')
{
  --replace_result $before_gtids GTIDS
  --let $__until_clause= SQL_BEFORE_GTIDS="$before_gtids"
}

eval START REPLICA SQL_THREAD UNTIL $__until_clause;

--let $wait_condition= SELECT count(*) = 1 FROM information_schema.processlist WHERE state = "Waiting for workers to exit" OR state = "Waiting for Replica Worker to release partition"
--source include/wait_condition.inc

--let $rpl_connection_name= server_2
--source include/connection.inc
ROLLBACK;

--source include/rpl/wait_for_applier_to_stop.inc
--let $diff_tables= master:db1.v1, slave:db1.t1, slave:db2.t1, slave:db3.t1
--source include/diff_tables.inc

# Since until condition is already satisfied, it should stop immediately.
--replace_result $__until_clause SAME_UNTIO_OPTION
eval START REPLICA SQL_THREAD UNTIL $__until_clause;

--source include/rpl/wait_for_applier_to_stop.inc
--let $diff_tables= master:db1.v1, slave:db1.t1, slave:db2.t1, slave:db3.t1
--source include/diff_tables.inc

--source include/rpl/start_applier.inc
--source include/rpl/connection_source.inc
TRUNCATE db1.t1;
TRUNCATE db2.t1;
TRUNCATE db3.t1;
--source include/rpl/sync_to_replica.inc
