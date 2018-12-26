include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
include/stop_slave.inc
SET @old_slave_parallel_workers= @@global.slave_parallel_workers;
SET GLOBAL slave_parallel_workers= 4;
include/start_slave.inc
select count(*) from performance_schema.replication_applier_status_by_coordinator;
count(*)
1
select THREAD_ID
into @coordinator_thread_id
from performance_schema.replication_applier_status_by_coordinator
limit 1;
select NAME from performance_schema.threads
where THREAD_ID = @coordinator_thread_id;
NAME
thread/sql/slave_sql

## Create index_test() procedure
DROP PROCEDURE IF EXISTS index_test;
set @pfs_columns = "CHANNEL_NAME, THREAD_ID, SERVICE_STATE";
TABLE: "replication_applier_status_by_coordinator"
INDEXES: PK (CHANNEL_NAME)

===== BEGIN TEST =====
========
SELECT CHANNEL_NAME, THREAD_ID, SERVICE_STATE FROM performance_schema.replication_applier_status_by_coordinator WHERE CHANNEL_NAME is null

OK: Handler_read_key

OK: Result set: Empty  Expected: Empty

OK: Extra rows: 0  Missing rows: 0
========
SELECT CHANNEL_NAME, THREAD_ID, SERVICE_STATE FROM performance_schema.replication_applier_status_by_coordinator WHERE CHANNEL_NAME is not null

OK: Handler_read_key

OK: Result set: Non-empty  Expected: Non-empty

OK: Extra rows: 0  Missing rows: 0
========
SELECT CHANNEL_NAME, THREAD_ID, SERVICE_STATE FROM performance_schema.replication_applier_status_by_coordinator WHERE CHANNEL_NAME = ''

OK: Handler_read_key

OK: Result set: Non-empty  Expected: Non-empty

OK: Extra rows: 0  Missing rows: 0
========
SELECT CHANNEL_NAME, THREAD_ID, SERVICE_STATE FROM performance_schema.replication_applier_status_by_coordinator WHERE CHANNEL_NAME != ''

OK: Handler_read_key

OK: Result set: Empty  Expected: Empty

OK: Extra rows: 0  Missing rows: 0
========
SELECT CHANNEL_NAME, THREAD_ID, SERVICE_STATE FROM performance_schema.replication_applier_status_by_coordinator WHERE CHANNEL_NAME = 'impossible'

OK: Handler_read_key

OK: Result set: Empty  Expected: Empty

OK: Extra rows: 0  Missing rows: 0
========
SELECT CHANNEL_NAME, THREAD_ID, SERVICE_STATE FROM performance_schema.replication_applier_status_by_coordinator WHERE CHANNEL_NAME != 'impossible' ORDER BY CHANNEL_NAME DESC

OK: Handler_read_key

OK: Result set: Non-empty  Expected: Non-empty

OK: Extra rows: 0  Missing rows: 0
SET @target_id = @coordinator_thread_id;
TABLE: "replication_applier_status_by_coordinator"
INDEXES: THREAD_ID

===== BEGIN TEST =====
========
SELECT CHANNEL_NAME, THREAD_ID, SERVICE_STATE FROM performance_schema.replication_applier_status_by_coordinator WHERE THREAD_ID is null

OK: Handler_read_key

OK: Result set: Empty  Expected: Empty

OK: Extra rows: 0  Missing rows: 0
========
SELECT CHANNEL_NAME, THREAD_ID, SERVICE_STATE FROM performance_schema.replication_applier_status_by_coordinator WHERE THREAD_ID is not null

OK: Handler_read_key

OK: Result set: Non-empty  Expected: Non-empty

OK: Extra rows: 0  Missing rows: 0
========
SELECT CHANNEL_NAME, THREAD_ID, SERVICE_STATE FROM performance_schema.replication_applier_status_by_coordinator WHERE THREAD_ID = @target_id

OK: Handler_read_key

OK: Result set: Non-empty  Expected: Non-empty

OK: Extra rows: 0  Missing rows: 0
========
SELECT CHANNEL_NAME, THREAD_ID, SERVICE_STATE FROM performance_schema.replication_applier_status_by_coordinator WHERE THREAD_ID = @target_id ORDER BY THREAD_ID

OK: Handler_read_key

OK: Result set: Non-empty  Expected: Non-empty

OK: Extra rows: 0  Missing rows: 0
========
SELECT CHANNEL_NAME, THREAD_ID, SERVICE_STATE FROM performance_schema.replication_applier_status_by_coordinator WHERE THREAD_ID = @target_id ORDER BY THREAD_ID DESC

OK: Handler_read_key

OK: Result set: Non-empty  Expected: Non-empty

OK: Extra rows: 0  Missing rows: 0
========
SELECT CHANNEL_NAME, THREAD_ID, SERVICE_STATE FROM performance_schema.replication_applier_status_by_coordinator WHERE THREAD_ID = 9999999 ORDER BY THREAD_ID

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
