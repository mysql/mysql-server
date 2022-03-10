include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
include/stop_slave.inc
SET @old_replica_parallel_workers= @@global.replica_parallel_workers;
SET GLOBAL replica_parallel_workers= 4;
include/start_slave.inc

## Create index_test() procedure
DROP PROCEDURE IF EXISTS index_test;
SET @pfs_columns = "*";
TABLE: "replication_connection_configuration"
INDEXES: PK (CHANNEL_NAME)

===== BEGIN TEST =====
========
SELECT * FROM performance_schema.replication_connection_configuration WHERE CHANNEL_NAME is null

OK: Handler_read_key

OK: Result set: Empty  Expected: Empty

OK: Extra rows: 0  Missing rows: 0
========
SELECT * FROM performance_schema.replication_connection_configuration WHERE CHANNEL_NAME is not null

OK: Handler_read_key

OK: Result set: Non-empty  Expected: Non-empty

OK: Extra rows: 0  Missing rows: 0
========
SELECT * FROM performance_schema.replication_connection_configuration WHERE CHANNEL_NAME = ''

OK: Handler_read_key

OK: Result set: Non-empty  Expected: Non-empty

OK: Extra rows: 0  Missing rows: 0
========
SELECT * FROM performance_schema.replication_connection_configuration WHERE CHANNEL_NAME != ''

OK: Handler_read_key

OK: Result set: Empty  Expected: Empty

OK: Extra rows: 0  Missing rows: 0
========
SELECT * FROM performance_schema.replication_connection_configuration WHERE CHANNEL_NAME = 'impossible'

OK: Handler_read_key

OK: Result set: Empty  Expected: Empty

OK: Extra rows: 0  Missing rows: 0
========
SELECT * FROM performance_schema.replication_connection_configuration WHERE CHANNEL_NAME != 'impossible' ORDER BY CHANNEL_NAME DESC

OK: Handler_read_key

OK: Result set: Non-empty  Expected: Non-empty

OK: Extra rows: 0  Missing rows: 0

===== CLEAN UP =====
DROP PROCEDURE index_test;
DROP TABLE IF EXISTS test.indexes_off;
DROP TABLE IF EXISTS test.indexes_on;
include/stop_slave.inc
SET GLOBAL replica_parallel_workers= @old_replica_parallel_workers;
include/start_slave.inc
include/rpl_end.inc
