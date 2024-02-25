
--echo # Switch to connection default
--connection default

# Wait for payload to complete cleanup

let $wait_timeout= 10;
let $wait_condition= SELECT COUNT(*) = 2 FROM performance_schema.events_waits_current where EVENT_NAME = 'idle' and END_EVENT_ID is null;
--source include/wait_condition.inc

--disable_query_log

eval CALL test.verify_thread($con1_thread_id, "CON1", $ctrl_mem_debug);

eval CALL test.verify_thread($con2_thread_id, "CON2", $ctrl_mem_debug);

--enable_query_log
