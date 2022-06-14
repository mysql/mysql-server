include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
[connection slave]
call mtr.add_suppression(".*worker thread retried transaction 5 time.*");
call mtr.add_suppression(".*The slave coordinator and worker threads are stopped, possibly leaving data in inconsistent state.*");
SET @saved_replica_parallel_workers= @@GLOBAL.replica_parallel_workers;
SET @saved_innodb_lock_wait_timeout = @@GLOBAL.innodb_lock_wait_timeout;
SET @saved_replica_transaction_retries = @@GLOBAL.replica_transaction_retries;
SET GLOBAL replica_transaction_retries = 5;
SET GLOBAL innodb_lock_wait_timeout = 2;
[connection master]
CREATE TABLE t1 (a INT UNIQUE);
include/sync_slave_sql_with_master.inc
BEGIN;
INSERT INTO t1 VALUES (1);;
[connection master]
INSERT INTO t1 VALUES (1);;
[connection slave]
include/wait_for_slave_sql_to_stop.inc
ROLLBACK;
include/start_slave_sql.inc
[connection master]
include/sync_slave_sql_with_master.inc
include/assert.inc [Confirm that we are checking the correct transaction]
include/assert.inc [Start apply timestamp must be more recent after start slave to prove that the transaction info was reset]
include/stop_slave.inc
SET GLOBAL replica_parallel_workers = 1;
include/start_slave.inc
BEGIN;
INSERT INTO t1 VALUES (2);;
[connection master]
INSERT INTO t1 VALUES (2);;
[connection slave]
include/wait_for_slave_sql_to_stop.inc
ROLLBACK;
include/start_slave_sql.inc
[connection master]
include/sync_slave_sql_with_master.inc
include/assert.inc [Confirm that we are checking the correct transaction]
include/assert.inc [Start apply timestamp must be more recent after start slave to prove that the transaction info was reset]
include/stop_slave.inc
SET GLOBAL replica_parallel_workers = 1;
include/start_slave.inc
[connection master]
DROP TABLE t1;
include/sync_slave_sql_with_master.inc
include/stop_slave.inc
SET GLOBAL replica_parallel_workers = @saved_replica_parallel_workers;
include/start_slave.inc
SET GLOBAL innodb_lock_wait_timeout = @saved_innodb_lock_wait_timeout;
SET GLOBAL replica_transaction_retries = @saved_replica_transaction_retries;
include/rpl_end.inc
