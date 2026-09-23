# Kill some query of the worker session though there is no already running query.

# Reset DEBUG_SYNC facility.
SET DEBUG_SYNC= 'RESET';

--echo Bug#29969769 - KILL QUERY KILLS THE NEXT QUERY
# Sending KILL QUERY before statement begins execution shall not interrupt it.
connect (con1, localhost, root,,);
--let $ID= `SELECT connection_id()`
SET DEBUG_SYNC='before_command_dispatch SIGNAL kill_query WAIT_FOR continue';
--send SELECT 1;

connection default;
SET DEBUG_SYNC='now WAIT_FOR kill_query';
--replace_result $ID ID
--eval KILL QUERY $ID
SET DEBUG_SYNC='now SIGNAL continue';

connection con1;
#Without fix, SELECT fails with ER_QUERY_INTERRUPTED error.
--reap

connection default;
SET DEBUG_SYNC='RESET';
disconnect con1;

# The KILL QUERY shall terminate the statement that is currently executing on the connection.

--connection worker
SET DEBUG_SYNC= 'mdl_acquire_lock_wait SIGNAL sync_start WAIT_FOR continue';
send SELECT * FROM t1;

--connection default
--disable_query_log
SET DEBUG_SYNC='now WAIT_FOR sync_start';
--replace_result $worker_id <worker_id>
eval KILL QUERY $worker_id;
SET DEBUG_SYNC='now SIGNAL continue';

--enable_query_log

--connection worker
--error ER_QUERY_INTERRUPTED
--reap
send SELECT * FROM t1;

--connection default
--disable_query_log
--replace_result $worker_id <worker_id>
--error 0
eval KILL $worker_id;
--replace_result $worker_id <worker_id>
--error 0,ER_NO_SUCH_THREAD
eval KILL $worker_id;

# Reset DEBUG_SYNC facility after use.
SET DEBUG_SYNC= "RESET";

--enable_query_log
# Wait till the worker session has disappeared.
# let $wait_timeout= 10;
let $wait_condition= SELECT COUNT(*) = 0 FROM information_schema.processlist
WHERE ID = $worker_id;
--source include/wait_condition.inc

