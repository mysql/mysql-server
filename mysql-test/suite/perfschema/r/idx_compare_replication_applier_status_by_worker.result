include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
include/stop_slave.inc
SET @old_slave_parallel_workers= @@global.slave_parallel_workers;
SET GLOBAL slave_parallel_workers= 4;
include/start_slave.inc
select THREAD_ID
into @worker_thread_id
from performance_schema.replication_applier_status_by_worker
where WORKER_ID = 1
limit 1;
select NAME from performance_schema.threads
where THREAD_ID = @worker_thread_id;
NAME
thread/sql/slave_worker

## Create index_test() procedure
DROP PROCEDURE IF EXISTS index_test;
SET @pfs_columns = "CHANNEL_NAME, WORKER_ID, THREAD_ID";
TABLE: "replication_applier_status_by_worker"
INDEXES: PK (CHANNEL_NAME)

===== BEGIN TEST =====
========
SELECT CHANNEL_NAME, WORKER_ID, THREAD_ID FROM performance_schema.replication_applier_status_by_worker WHERE CHANNEL_NAME is null

OK: Handler_read_key

OK: Result set: Empty  Expected: Empty

OK: Extra rows: 0  Missing rows: 0
========
SELECT CHANNEL_NAME, WORKER_ID, THREAD_ID FROM performance_schema.replication_applier_status_by_worker WHERE CHANNEL_NAME is not null

OK: Handler_read_key

OK: Result set: Non-empty  Expected: Non-empty

OK: Extra rows: 0  Missing rows: 0
========
SELECT CHANNEL_NAME, WORKER_ID, THREAD_ID FROM performance_schema.replication_applier_status_by_worker WHERE CHANNEL_NAME = ''

OK: Handler_read_key

OK: Result set: Non-empty  Expected: Non-empty

OK: Extra rows: 0  Missing rows: 0
========
SELECT CHANNEL_NAME, WORKER_ID, THREAD_ID FROM performance_schema.replication_applier_status_by_worker WHERE CHANNEL_NAME != ''

OK: Handler_read_key

OK: Result set: Empty  Expected: Empty

OK: Extra rows: 0  Missing rows: 0
========
SELECT CHANNEL_NAME, WORKER_ID, THREAD_ID FROM performance_schema.replication_applier_status_by_worker WHERE CHANNEL_NAME = 'impossible'

OK: Handler_read_key

OK: Result set: Empty  Expected: Empty

OK: Extra rows: 0  Missing rows: 0
========
SELECT CHANNEL_NAME, WORKER_ID, THREAD_ID FROM performance_schema.replication_applier_status_by_worker WHERE CHANNEL_NAME != 'impossible' ORDER BY CHANNEL_NAME DESC

OK: Handler_read_key

OK: Result set: Non-empty  Expected: Non-empty

OK: Extra rows: 0  Missing rows: 0
========
SELECT CHANNEL_NAME, WORKER_ID, THREAD_ID FROM performance_schema.replication_applier_status_by_worker WHERE CHANNEL_NAME = '' AND WORKER_ID is null

OK: Handler_read_key

OK: Result set: Empty  Expected: Empty

OK: Extra rows: 0  Missing rows: 0
========
SELECT CHANNEL_NAME, WORKER_ID, THREAD_ID FROM performance_schema.replication_applier_status_by_worker WHERE CHANNEL_NAME = '' AND WORKER_ID is not null

OK: Handler_read_key

OK: Result set: Non-empty  Expected: Non-empty

OK: Extra rows: 0  Missing rows: 0
========
SELECT CHANNEL_NAME, WORKER_ID, THREAD_ID FROM performance_schema.replication_applier_status_by_worker WHERE CHANNEL_NAME = '' AND WORKER_ID = 4

OK: Handler_read_key

OK: Result set: Non-empty  Expected: Non-empty

OK: Extra rows: 0  Missing rows: 0
========
SELECT CHANNEL_NAME, WORKER_ID, THREAD_ID FROM performance_schema.replication_applier_status_by_worker WHERE CHANNEL_NAME = '' AND WORKER_ID != 4

OK: Handler_read_key

OK: Result set: Non-empty  Expected: Non-empty

OK: Extra rows: 0  Missing rows: 0
========
SELECT CHANNEL_NAME, WORKER_ID, THREAD_ID FROM performance_schema.replication_applier_status_by_worker WHERE CHANNEL_NAME = '' AND WORKER_ID = 9999

OK: Handler_read_key

OK: Result set: Empty  Expected: Empty

OK: Extra rows: 0  Missing rows: 0
SET @target_id = @worker_thread_id;
TABLE: "replication_applier_status_by_worker"
INDEXES: THREAD_ID

===== BEGIN TEST =====
========
SELECT CHANNEL_NAME, WORKER_ID, THREAD_ID FROM performance_schema.replication_applier_status_by_worker WHERE THREAD_ID is null

OK: Handler_read_key

OK: Result set: Empty  Expected: Empty

OK: Extra rows: 0  Missing rows: 0
========
SELECT CHANNEL_NAME, WORKER_ID, THREAD_ID FROM performance_schema.replication_applier_status_by_worker WHERE THREAD_ID is not null

OK: Handler_read_key

OK: Result set: Non-empty  Expected: Non-empty

OK: Extra rows: 0  Missing rows: 0
========
SELECT CHANNEL_NAME, WORKER_ID, THREAD_ID FROM performance_schema.replication_applier_status_by_worker WHERE THREAD_ID = @target_id

OK: Handler_read_key

OK: Result set: Non-empty  Expected: Non-empty

OK: Extra rows: 0  Missing rows: 0
========
SELECT CHANNEL_NAME, WORKER_ID, THREAD_ID FROM performance_schema.replication_applier_status_by_worker WHERE THREAD_ID = @target_id ORDER BY THREAD_ID

OK: Handler_read_key

OK: Result set: Non-empty  Expected: Non-empty

OK: Extra rows: 0  Missing rows: 0
========
SELECT CHANNEL_NAME, WORKER_ID, THREAD_ID FROM performance_schema.replication_applier_status_by_worker WHERE THREAD_ID = @target_id ORDER BY THREAD_ID DESC

OK: Handler_read_key

OK: Result set: Non-empty  Expected: Non-empty

OK: Extra rows: 0  Missing rows: 0
========
SELECT CHANNEL_NAME, WORKER_ID, THREAD_ID FROM performance_schema.replication_applier_status_by_worker WHERE THREAD_ID = 9999999 ORDER BY THREAD_ID

OK: Handler_read_key

OK: Result set: Empty  Expected: Empty

OK: Extra rows: 0  Missing rows: 0

===== CLEAN UP =====
DROP PROCEDURE index_test;
DROP TABLE IF EXISTS test.indexes_off;
DROP TABLE IF EXISTS test.indexes_on;
include/stop_slave.inc
SET GLOBAL slave_parallel_workers= @old_slave_parallel_workers;
include/start_slave.inc
include/rpl_end.inc
