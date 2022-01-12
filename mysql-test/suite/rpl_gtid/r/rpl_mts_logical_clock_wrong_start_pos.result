include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
[connection slave]
call mtr.add_suppression("Either event.*is from an old master");
call mtr.add_suppression("An unexpected event sequence was detected by the IO thread");
call mtr.add_suppression("QUERY.* is not expected in an event stream");
include/stop_slave.inc
SET @save_replica_parallel_type = @@GLOBAL.replica_parallel_type;
Warnings:
Warning	1287	'@@replica_parallel_type' is deprecated and will be removed in a future release.
SET @save_replica_parallel_workers = @@GLOBAL.replica_parallel_workers;
SET @save_replica_transaction_retries = @@GLOBAL.replica_transaction_retries;
SET GLOBAL replica_parallel_type = LOGICAL_CLOCK;
Warnings:
Warning	1287	'@@replica_parallel_type' is deprecated and will be removed in a future release.
SET GLOBAL replica_parallel_workers = 2;
SET GLOBAL replica_transaction_retries = 0;
include/start_slave.inc
[connection master]
CREATE TABLE t1(a INT);
INSERT INTO t1 SET a=1;
include/sync_slave_sql_with_master.inc
include/stop_slave.inc
CHANGE REPLICATION SOURCE TO SOURCE_LOG_POS=QUERY_POS, SOURCE_AUTO_POSITION=0;
include/start_slave_io.inc
START SLAVE SQL_THREAD;
Warnings:
Warning	1287	'START SLAVE' is deprecated and will be removed in a future release. Please use START REPLICA instead
[connection master]
DROP TABLE t1;
include/sync_slave_sql_with_master.inc
include/stop_slave.inc
SET GLOBAL replica_parallel_workers = @save_replica_parallel_workers;
SET GLOBAL replica_parallel_type = @save_replica_parallel_type;
Warnings:
Warning	1287	'@@replica_parallel_type' is deprecated and will be removed in a future release.
SET GLOBAL replica_transaction_retries = @save_replica_transaction_retries;
include/start_slave.inc
include/rpl_end.inc
