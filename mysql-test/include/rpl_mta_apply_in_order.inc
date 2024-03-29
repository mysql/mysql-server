# ==== Purpose ====
#
# Execute a set of transactions on the source and start applying them in a
# given order on the replica, ensuring that all transactions are ran in
# parallel.
#
# Transactions are executed in the requested order by taking advantage of a
# server behavior that blocks execution of a thread trying to instantiate
# `GTID_NEXT` with a GTID that is being used and not yet added to
# `GTID_EXECUTED` (or `GTID_PURGED`, for that matter). Therefore, assuming
# that N is the amount of transactions executed on the source, the utility
# will:
#
# 1) Execute the transactions on the source with pre-arranged GTIDs (!=
#    AUTOMATIC).
#
# 2) Open N connections to the replica and set `GTID_NEXT` to each one of
#    the GTIDs used for commiting the transactions on the source.
#
# 3) Let the applier process transaction up until the point that it gets
#    blocked on setting the `GTID_NEXT` for the applying transaction.
#
# 4) After all workers are waiting to set `GTID_NEXT`, use each one of the
#    connections and rollback (releasing the GTID) in the requested order.
#
# IMPORTANT NOTE: The `include/destroy_json_functions.inc` will be already
# included by `include/rpl_end.inc`.
#
# ==== Notes on script behavior ====
#
# 1. The amount of applier worker threads will be set to same amount of
#    transactions to executed.
#
# 2. The `include/destroy_json_functions.inc` must be included by the test
#    including this script.
#
# 3. This script will stop the applier in order to configure it.
#
# 4. The applier will be left running after this script exits.
#
# ==== Usage ====
#
# --let $transactions_to_exec = <JSON ARRAY>
# [--let $replica_execution_order = <JSON ARRAY>]
# [--let $applier_worker_wait_stage = <JSON ARRAY>]
# --source include/rpl_mta_apply_in_order.inc
#
# Parameters:
#
#   $transactions_to_exec
#     JSON array containing the list of transactions to execute on the
#     source. Example:
#
#       let $transactions_to_exec = [
#         "INSERT INTO t VALUES (1)",
#         "INSERT INTO t VALUES (2)",
#         "BEGIN; DELETE FROM t WHERE a = 1; INSERT INTO t VALUES (1); COMMIT",
#         "BEGIN; UPDATE t SET a = 4 WHERE a = 2; INSERT INTO t VALUES (2); COMMIT",
#         "INSERT INTO t VALUES (5)"
#       ];
#
#   $replica_execution_order
#     JSON array containing the intended apply order of the transactions on
#     the replica, where each array element is the position of the
#     transaction in the `$transactions_to_exec` array (starts at `1`).
#     Example:
#
#       --let $replica_execution_order = [ 1, 2, 5, 3, 4 ]
#
#     The default value is `[ 1, 2, 3, ..., n ]` being `n` the size of
#     `$transactions_to_exec`.
#
#   $applier_worker_wait_stage
#     JSON array containing the applier worker running stage to wait for
#     before allowing the next transaction in the sequence to be
#     executed. Example:
#
#       let $applier_worker_wait_stage = [
#         "Waiting for an event from Coordinator",
#         "Waiting for an event from Coordinator",
#         "Waiting for preceding transaction to commit",
#         "Waiting for an event from Coordinator",
#         "Waiting for an event from Coordinator"
#       ];
#
#     The default is an array where each element is either "Waiting for an
#     event from Coordinator" or "Waiting for preceding transaction to
#     commit", depending on whether the transaction is expected to wait for
#     preceding transactions or not, according to `$replica_execution_order`
#     and `@@GLOBAL.replica_preserve_commit_order`.
#
# The full example:
#
#       let $transactions_to_exec = [
#         "INSERT INTO t VALUES (1)",
#         "INSERT INTO t VALUES (2)",
#         "BEGIN; DELETE FROM t WHERE a = 1; INSERT INTO t VALUES (1); COMMIT",
#         "BEGIN; UPDATE t SET a = 4 WHERE a = 2; INSERT INTO t VALUES (2); COMMIT",
#         "INSERT INTO t VALUES (5)"
#       ];
#       --let $replica_execution_order = [ 1, 2, 5, 3, 4 ]
#       let $applier_worker_wait_stage = [
#         "Waiting for an event from Coordinator",
#         "Waiting for an event from Coordinator",
#         "Waiting for preceding transaction to commit",
#         "Waiting for an event from Coordinator",
#         "Waiting for an event from Coordinator"
#       ];
#
#

if ($transactions_to_exec == '') {
  --die ERROR: `transactions_to_exec` parameter must be set
}
if (!$rpl_gtid_utils) {
  --die rpl_mta_apply_in_order.inc requires that you use the rpl_gtid_utils=1 option with master-slave.inc (or rpl_init.inc)
}

--source include/only_mts_replica_parallel_type_logical_clock.inc
--let $option_name = gtid_mode
--let $option_value = 'ON'
--source include/only_with_option.inc

--source include/rpl_connection_slave.inc
--let $rpl_only_running_threads = 1
--source include/stop_slave.inc

--let $json_label = transactions_to_exec
--source include/create_json_iterator.inc

--let $_num_statements = `SELECT JSON_LENGTH('$transactions_to_exec')`

# Default value for $replica_execution_order
--let $_replica_execution_order = $replica_execution_order
if ($replica_execution_order == '') {
  --let $_replica_execution_order = [
  --let $json_array = $transactions_to_exec
  --source $json_transactions_to_exec_start
  --let $_n_statement = 0
  while (!$json_transactions_to_exec_done) {
    --inc $_n_statement
    if ($_replica_execution_order != [) {
      --let $_replica_execution_order = $_replica_execution_order,
    }
    --let $_replica_execution_order = $_replica_execution_order $_n_statement
    --source $json_transactions_to_exec_next
  }
  --let $_replica_execution_order = $_replica_execution_order ]
}

--let $json_label = order_of_statements
--source include/create_json_iterator.inc

# Default value for $applier_worker_wait_stage
--let $_applier_worker_wait_stage = $applier_worker_wait_stage
if ($applier_worker_wait_stage == '') {
  --let $_preserve_commit_order = `SELECT @@global.replica_preserve_commit_order`
  --let $_n_transaction = 1
  --let $_applier_worker_wait_stage = [
  --let $json_array = $_replica_execution_order
  --source $json_order_of_statements_start
  while (!$json_order_of_statements_done) {
    if ($_applier_worker_wait_stage != [) {
      --let $_applier_worker_wait_stage = $_applier_worker_wait_stage,
    }
    --let $_default_stage = "Waiting for an event from Coordinator"
    if ($_preserve_commit_order) {
      --let $_order = $json_order_of_statements_value
      --let $dollar = `SELECT CHAR(36)`
      let $_n_preceding =
        `SELECT COUNT(*) + 1
         FROM JSON_TABLE('$_replica_execution_order', '$[*]' COLUMNS (
           idx FOR ORDINALITY,
           preceding INT PATH '$dollar'
         )) AS jt
         WHERE jt.idx < $_n_transaction AND jt.preceding < $_order`;
      if ($_n_preceding != $_order) {
        --let $_default_stage = "Waiting for preceding transaction to commit"
      }
    }
    --let $_applier_worker_wait_stage = $_applier_worker_wait_stage $_default_stage
    --inc $_n_transaction
    --source $json_order_of_statements_next
  }
  --let $_applier_worker_wait_stage = $_applier_worker_wait_stage ]
}

# First event to be executed on replica after applier start is executed by
# the coordinator. So, send a DDL transaction that doesn't change any data
# to ensure that all configured transactions will be assigned to worker
# threads.
--source include/rpl_connection_master.inc
--disable_warnings
--disable_query_log
DROP TABLE IF EXISTS _first_event;
--enable_query_log
--enable_warnings

--let $_source_gtid_executed = `SELECT @@GLOBAL.gtid_executed`
--let $_source_server_uuid = `SELECT @@GLOBAL.server_uuid`

--let $debug_point = set_commit_parent_100
--source include/add_debug_point.inc

# Execute using specific GTIDs on the source
--let $_n_statement = 0
--let $json_array = $transactions_to_exec
--source $json_transactions_to_exec_start
while (!$json_transactions_to_exec_done) {
  --let $_statement = $json_transactions_to_exec_value
  --inc $_n_statement
  --echo Executing transaction $_n_statement on source
  --eval $_statement
  --source $json_transactions_to_exec_next
}
--let $debug_point = set_commit_parent_100
--source include/remove_debug_point.inc

--source include/rpl_connection_slave.inc
let $sysvars_to_save = [
    "GLOBAL.replica_parallel_workers"
];
--source include/save_sysvars.inc
--eval SET @@GLOBAL.replica_parallel_workers = $_num_statements

# Use multiple connections to the replica to block the transaction
# execution using the specific GTIDs
--let $_conn = 1
while ($_conn <= $_num_statements) {
  --connect (gtid_next_conn$_conn,localhost,root,,,$SLAVE_MYPORT,$SLAVE_MYSOCK)
  --connection gtid_next_conn$_conn
  --let $_gtid_next = `SELECT GTID_NEXT_GENERATED('$_source_gtid_executed', '$_source_server_uuid')`
  --let $_gtid_next = $_source_server_uuid:$_gtid_next
  --let $_source_gtid_executed = `SELECT GTID_UNION('$_source_gtid_executed', '$_gtid_next')`
  --echo Taking ownership of transaction $_conn GTID on replica
  --replace_result $_gtid_next GTID
  --eval SET GTID_NEXT = '$_gtid_next'
  --inc $_conn
}

--source include/rpl_connection_slave.inc
--source include/start_slave.inc

--let $json_label = wait_for_state
--source include/create_json_iterator.inc

# Progressively rollback and disconnect the connections holding the GTIDs
--let $json_array = $_applier_worker_wait_stage
--source $json_wait_for_state_start
--let $json_array = $_replica_execution_order
--source $json_order_of_statements_start
while (!$json_order_of_statements_done) {
  --let $_order = $json_order_of_statements_value
  --let $_wait_for_state = $json_wait_for_state_value

  --echo include/wait_condition.inc [ Workers waiting: $_n_statement ]
  --let $wait_condition = SELECT count(*) = LEAST($_n_statement, @@GLOBAL.replica_parallel_workers) FROM information_schema.processlist WHERE STATE = "Waiting for GTID to be committed"
  --source include/wait_condition.inc

  --let $_wait_for_num = `SELECT count(*) FROM information_schema.processlist WHERE STATE = "$_wait_for_state"`

  --connection gtid_next_conn$_order
  --echo Unblocking transaction $_order
  ROLLBACK;
  SET GTID_NEXT = AUTOMATIC;
  --disconnect gtid_next_conn$_order

  --source include/rpl_connection_slave.inc

  --echo include/wait_condition.inc [ Wait for '$_wait_for_state' ]
  --let $wait_condition = SELECT count(*) >= ($_wait_for_num + 1) FROM information_schema.processlist WHERE STATE = "$_wait_for_state"
  --source include/wait_condition.inc

  --dec $_n_statement
  --source $json_order_of_statements_next
  --source $json_wait_for_state_next
}

--source include/rpl_connection_master.inc
--source include/sync_slave_sql_with_master.inc

--source include/rpl_connection_slave.inc
--source include/stop_slave.inc
--disable_warnings
--source include/restore_sysvars.inc
--enable_warnings
--source include/start_slave.inc
