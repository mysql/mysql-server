--connection server_2
--echo [connection server_2]
STOP REPLICA;
--let $server2_binlog_postion=query_get_value("SHOW BINARY LOG STATUS", Position, 1)

--connection server_3
--echo [connection server_3]
STOP REPLICA;
--let $server3_binlog_postion=query_get_value("SHOW BINARY LOG STATUS", Position, 1)
--let $server3_binlog_file=query_get_value("SHOW BINARY LOG STATUS", File, 1)

--connection server_1
--echo [connection server_1]
--echo # Trx1 and Trx2
eval INSERT INTO t1 (id, c0) VALUES ($start_id, 'abc'), ($start_id+1, 'efg'), ($start_id+2, 'xyz');
eval DELETE FROM t1 WHERE id =$start_id+2;

FLUSH LOGS;

--echo # Trx3 and Trx4
eval UPDATE t1 SET id=$start_id+2, c0='AliSQL' WHERE id=$start_id;
eval INSERT INTO t1 (id, c0) VALUES ($start_id, 'xyz');

--connection server_2
--echo [connection server_2]
--echo # Will put Trx1 and Trx2 in one batch, put Trx3 and Trx4 in one batch 
START REPLICA;

--let $rpl_only_running_threads=1
--source include/rpl/sync.inc

--connection server_3
--echo [connection server_3]
--echo # Will put Trx1, Trx2, Trx3 and Trx4 in one batch when duckdb_multi_trx_in_batch=on
--echo # Same as server_2 when duckdb_multi_trx_in_batch=off
START REPLICA;

--let $rpl_only_running_threads=0
--source include/rpl/sync.inc
--let $rpl_diff_statement = SELECT id, c0 FROM test.t1 ORDER BY id;
--source include/rpl/diff.inc

--connection server_2
--echo [connection server_2]
--let $keep_gtid_events=1
--let $keep_gtid_events_gno=1
--let $mask_thread_id=1
--let $show_binlog_events_mask_columns=1,2,4,5
--let $binlog_start=$server2_binlog_postion
--source include/rpl/deprecated/show_binlog_events.inc

--connection server_3
--echo [connection server_3]
--let $binlog_start=$server3_binlog_postion
--let $binlog_file=$server3_binlog_file
--source include/rpl/deprecated/show_binlog_events.inc
