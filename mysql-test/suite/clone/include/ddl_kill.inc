## KILL DDL command while waiting for clone

--let $CON_ID= `SELECT CONNECTION_ID()`

SET DEBUG_SYNC = 'clone_notify_ddl SIGNAL kill_ddl';
--send_eval $ddl_stmt

--connection con1
--echo # In connection CON1

--echo # Waiting for DDL to reach notification
SET DEBUG_SYNC = 'now WAIT_FOR kill_ddl';
--echo # Wait finished

--replace_result $CON_ID CON_ID
--eval KILL QUERY $CON_ID

--connection con2
--echo # In connection CON2

if (!$no_interrupt) {
  --echo # Waiting for DDL to exit with error
  --error ER_QUERY_INTERRUPTED
  --reap
}

--echo # Query should no longer be interrupted and should pass
if ($no_interrupt) {
  --reap
}

--echo # Wait finished
SET DEBUG_SYNC = 'RESET';
