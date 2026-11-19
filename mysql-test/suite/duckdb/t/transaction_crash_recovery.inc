--source include/rpl/connection_replica.inc
STOP REPLICA;

--source include/rpl/connection_source.inc
eval $dml_query;

--source include/rpl/connection_replica.inc
eval SET GLOBAL debug=$crash_point;
--source include/expect_crash.inc
START REPLICA;
--source include/wait_until_disconnected.inc

--let $rpl_server_number= 2
--source include/rpl/start_server.inc
START REPLICA;

--source include/rpl/sync.inc
