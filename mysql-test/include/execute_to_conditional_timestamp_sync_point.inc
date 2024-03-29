# ==== Purpose ====
#
# Begin to execute a statement, stop at a given conditional timestamp sync
# point.
#
# ==== About conditional sync points for timestamp ====
#
# Conditional timestamp sync points are used to control the execution flow
# of a statement for which the session variable `TIMESTAMP` as been set to
# a given value, meaning, the given named sync point will only be activated
# when the transaction being executed has a given session `TIMESTAMP` value
# associated.
#
# Note that the usage of the `TIMESTAMP` session variable doens't imply
# that the conditional execution is dependent on time measuring or the
# measured value of any clock. The `TIMESTAMP` variable is used for mere
# convenience and due to low impact on server behavior.
#
# Usage example for executing a statement and triggering a conditional sync
# point for timestamp on a single server:
#
# - On the server code, setup the conditional timestamp using
#   `CONDITIONAL_SYNC_POINT_FOR_TIMESTAMP`, as an example, just before the
#   transaction is committed in the storage engines:
#
#     bool trx_coordinator::commit_in_engines(THD *thd, bool all,
#                                             bool run_after_commit) {
#       if (all) CONDITIONAL_SYNC_POINT_FOR_TIMESTAMP("before_commit_in_engines");
#       ...
# 
# - On the MTR test, include this script, passing the connection used to
#   execute the statement and the connection used to activate the sync
#   point. 
#
#     --connect(con1, localhost, root,,)
#     --connection con1
#     
#     --let $timestamp = 100
#     --let $auxiliary_connection = default
#     --let $statement_connection = con1
#     --let $statement = INSERT INTO t VALUES (100);
#     --let $sync_point = before_commit_in_engines
#     --source include/execute_to_conditional_timestamp_sync_point.inc
#     ...
#
# Usage example for executing a statement on the source server and
# triggering a conditional sync point for timestamp on a replica:
#
# - On the server code, setup the conditional timestamp using
#   `CONDITIONAL_SYNC_POINT_FOR_TIMESTAMP`, as an example, just before the
#   transaction is committed in the storage engines:
#
#     bool trx_coordinator::commit_in_engines(THD *thd, bool all,
#                                             bool run_after_commit) {
#       if (all) CONDITIONAL_SYNC_POINT_FOR_TIMESTAMP("before_commit_in_engines");
#       ...
# 
# - On the MTR test, include this script, passing the connection used to
#   execute the statement and the connection used to activate the sync
#   point. 
#
#     --let $timestamp = 100
#     --let $auxiliary_connection = slave
#     --let $statement_connection = master
#     --let $statement = INSERT INTO t VALUES (100);
#     --let $sync_point = before_commit_in_engines
#     --source include/execute_to_conditional_timestamp_sync_point.inc
#     ...
#
# ==== Usage ====
#
# --let $statement_connection = {connection name}
# --let $statement = {statement to execute}
# --let $sync_point = {name of the sync point}
# [--let $timestamp = {timestamp to set}]
# [--let $auxiliary_connection = {connection name}]
# --source include/execute_to_conditional_timestamp_sync_point.inc
#
#   $statement_connection
#     The connection on which $statement is to be executed.
#     This must be different from $auxiliary_connection.
#
#   $statement
#     The statement that will be paused on the sync point.
#
#   $sync_point
#     The name of the sync point (not quoted).
#
#   $timestamp
#     By default, the current unix timestamp, it's the value used in `SET
#     TIMESTAMP = ...` in order to be used with the conditional timestamp
#     sync point.
#
#   $auxiliary_connection 
#     The connection used to set the debug sync points and
#     symbols. $auxiliary_connection is set to the currenct active
#     connection, if no value is passed. It must be different from the
#     $statement connection.
#
# Side effects:
#   If $auxiliary_connection and $statement_connection target the same
#   server, the $statement is executed using `--send` and without invoking
#   `--reap`.
#

if ($sync_point == '') {
  --die ERROR IN TEST: You must set $sync_point before sourcing execute_to_conditional_timestamp_sync_point.inc
}

--let $include_filename= execute_to_conditional_timestamp_sync_point.inc [$sync_point]
--source include/begin_include_file.inc

if ($statement_connection == '') {
  --die ERROR IN TEST: You must set $statement_connection before sourcing execute_to_conditional_timestamp_sync_point.inc
}

if ($timestamp  == '') {
  --let $timestamp = 1
}

--let $_ectsp_auxiliary_connection = $CURRENT_CONNECTION
if ($auxiliary_connection != '') {
  --let $_ectsp_auxiliary_connection = $auxiliary_connection
}

if ($statement_connection == $_ectsp_auxiliary_connection) {
  --echo statement_connection=$statement_connection
  --echo auxiliary_connection=$_ectsp_auxiliary_connection
  --die ERROR IN TEST: You must set $statement_connection to something else than $auxiliary_connection
}

if ($statement == '') {
  --die ERROR IN TEST: You must set $statement before sourcing execute_to_conditional_timestamp_sync_point.inc
}

--let $underscore = _

--connection $_ectsp_auxiliary_connection
--let $_ectsp_server_aux = `SELECT @@global.server_uuid`

--let $debug_point_silent = 1
--let $debug_point = syncpoint_$sync_point$underscore$timestamp
--source include/add_debug_point.inc

--connection $statement_connection
--let $_ectsp_server_stmt = `SELECT @@global.server_uuid`
--disable_query_log
if ($_ectsp_server_aux == $_ectsp_server_stmt) {
  --send
}
--eval SET TIMESTAMP = $timestamp; $statement
--enable_query_log
--echo $statement;

--connection $_ectsp_auxiliary_connection
--disable_query_log
--eval SET @@SESSION.DEBUG_SYNC = "now WAIT_FOR reached_$sync_point$underscore$timestamp"
--enable_query_log

--source include/remove_debug_point.inc
--let $debug_point_silent = 0

--let $skip_restore_connection= 1
--let $include_filename= execute_to_conditional_timestamp_sync_point.inc
--source include/end_include_file.inc
