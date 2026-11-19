--connection server_2
--echo [connection server_2]
STOP REPLICA;

--connection server_1
--echo [connection server_1]
--echo # Trx1, Trx2, Trx3 and Trx4
eval INSERT INTO t1 (id, c0) VALUES ($start_id, 'abc'), ($start_id+1, 'efg'), ($start_id+2, 'xyz');
eval DELETE FROM t1 WHERE id =$start_id+2;
eval UPDATE t1 SET id=$start_id+2, c0='AliSQL' WHERE id=$start_id;
eval INSERT INTO t1 (id, c0) VALUES ($start_id, 'xyz');

--connection server_2
--echo [connection server_2]
eval SET GLOBAL debug=$crash_point;
--source include/expect_crash.inc
START REPLICA;
--source include/wait_until_disconnected.inc

--let $rpl_server_number= 2
--source include/rpl/start_server.inc
START REPLICA;

--source include/rpl/sync.inc
--let $rpl_diff_statement = SELECT id, c0 FROM test.t1 ORDER BY id;
--source include/rpl/diff.inc
