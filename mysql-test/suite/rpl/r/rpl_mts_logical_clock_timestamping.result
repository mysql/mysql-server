include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
call mtr.add_suppression("Transaction is tagged with inconsistent logical timestamps");
call mtr.add_suppression("Cannot execute the current event group in the parallel mode.");
include/stop_slave.inc
SET @save_replica_parallel_type = @@GLOBAL.replica_parallel_type;
Warnings:
Warning	1287	'@@replica_parallel_type' is deprecated and will be removed in a future release.
SET @save_replica_parallel_workers = @@GLOBAL.replica_parallel_workers;
SET @save_replica_transaction_retries = @@GLOBAL.replica_transaction_retries;
SET GLOBAL replica_parallel_type = LOGICAL_CLOCK;
Warnings:
Warning	1287	'@@replica_parallel_type' is deprecated and will be removed in a future release.
SET GLOBAL replica_parallel_workers = 3;
SET GLOBAL replica_transaction_retries = 0;
RESET MASTER;
CREATE TABLE t1 (a int) ENGINE= innodb;
INSERT INTO t1 SET a=1;
BEGIN;
INSERT INTO t1 SET a=2;
BEGIN;
INSERT INTO t1 SET a=3;
FLUSH LOGS;
COMMIT;
COMMIT;
Occurrences of 'last_committed=0\tsequence_number=[12]' in the input file: 2
INSERT INTO t1 SET a=1;
BEGIN;
INSERT INTO t1 SET a=2;
BEGIN;
INSERT INTO t1 SET a=3;
FLUSH LOGS;
COMMIT;
FLUSH LOGS;
COMMIT;
INSERT INTO t1 SET a=1;
INSERT INTO t1 SET a=2;
INSERT INTO t1 SET a=3;
include/start_slave.inc
include/diff_tables.inc [master:t1,slave:t1]
SET @@session.debug ='+d,feign_commit_parent';
INSERT INTO t1 SET a=0;
SET @@session.debug ='-d,feign_commit_parent';
include/wait_for_slave_sql_to_stop.inc
include/stop_slave_io.inc
RESET MASTER;
RESET SLAVE;
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
RESET MASTER;
include/start_slave.inc
DROP TABLE t1;
include/stop_slave.inc
SET @@GLOBAL.replica_parallel_type= @save_replica_parallel_type;
Warnings:
Warning	1287	'@@replica_parallel_type' is deprecated and will be removed in a future release.
SET @@GLOBAL.replica_parallel_workers= @save_replica_parallel_workers;
SET @@GLOBAL.replica_transaction_retries= @save_replica_transaction_retries;
include/start_slave.inc
include/rpl_end.inc
