include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
CREATE TABLE t1 (a INT);
include/sync_slave_sql_with_master.inc
[connection slave]
SET @saved_replica_parallel_workers = @@GLOBAL.replica_parallel_workers;
include/stop_slave.inc
SET @@GLOBAL.replica_parallel_workers = 0;
Warnings:
Warning	1287	'0' is deprecated and will be removed in a future release. Please use 1 instead
include/start_slave.inc
[connection master]
INSERT INTO t1 VALUES (1);
include/sync_slave_sql_with_master.inc
include/stop_slave.inc
SET @@GLOBAL.replica_parallel_workers = 0;
Warnings:
Warning	1287	'0' is deprecated and will be removed in a future release. Please use 1 instead
include/start_slave.inc
include/assert.inc [The GTID information about the last applied transaction was preserved after the slave was stopped]
include/assert.inc [The OCT information about the last applied transaction was preserved after the slave was stopped]
include/assert.inc [The ICT information about the last applied transaction was preserved after the slave was stopped]
include/assert.inc [The start apply timestamp information about the last applied transaction was preserved after the slave was stopped]
include/assert.inc [The end apply timestamp information about the last applied transaction was preserved after the slave was stopped]
include/assert.inc [The GTID information about the last applied transaction was preserved after the slave was started]
include/assert.inc [The OCT information about the last applied transaction was preserved after the slave was started]
include/assert.inc [The ICT information about the last applied transaction was preserved after the slave was started]
include/assert.inc [The start apply timestamp information about the last applied transaction was preserved after the slave was started]
include/assert.inc [The end apply timestamp information about the last applied transaction was preserved after the slave was started]
[connection master]
INSERT INTO t1 VALUES (2);
include/sync_slave_sql_with_master.inc
include/stop_slave.inc
SET @@GLOBAL.replica_parallel_workers = 1;
include/start_slave.inc
include/assert.inc [The GTID information about the last applied transaction was preserved after the slave was stopped]
include/assert.inc [The OCT information about the last applied transaction was preserved after the slave was stopped]
include/assert.inc [The ICT information about the last applied transaction was preserved after the slave was stopped]
include/assert.inc [The start apply timestamp information about the last applied transaction was preserved after the slave was stopped]
include/assert.inc [The end apply timestamp information about the last applied transaction was preserved after the slave was stopped]
include/assert.inc [The GTID information about the last applied transaction was cleared after the slave was started]
include/assert.inc [The OCT information about the last applied transaction was cleared after the slave was started]
include/assert.inc [The ICT information about the last applied transaction was cleared after the slave was started]
include/assert.inc [The start apply timestamp information about the last applied transaction was cleared after the slave was started]
include/assert.inc [The end apply timestamp information about the last applied transaction was cleared after the slave was started]
[connection master]
INSERT INTO t1 VALUES (3);
include/sync_slave_sql_with_master.inc
include/stop_slave.inc
SET @@GLOBAL.replica_parallel_workers = 1;
include/start_slave.inc
include/assert.inc [The GTID information about the last applied transaction was preserved after the slave was stopped]
include/assert.inc [The OCT information about the last applied transaction was preserved after the slave was stopped]
include/assert.inc [The ICT information about the last applied transaction was preserved after the slave was stopped]
include/assert.inc [The start apply timestamp information about the last applied transaction was preserved after the slave was stopped]
include/assert.inc [The end apply timestamp information about the last applied transaction was preserved after the slave was stopped]
include/assert.inc [The GTID information about the last applied transaction was cleared after the slave was started]
include/assert.inc [The OCT information about the last applied transaction was cleared after the slave was started]
include/assert.inc [The ICT information about the last applied transaction was cleared after the slave was started]
include/assert.inc [The start apply timestamp information about the last applied transaction was cleared after the slave was started]
include/assert.inc [The end apply timestamp information about the last applied transaction was cleared after the slave was started]
[connection master]
INSERT INTO t1 VALUES (4);
include/sync_slave_sql_with_master.inc
include/stop_slave.inc
SET @@GLOBAL.replica_parallel_workers = 0;
Warnings:
Warning	1287	'0' is deprecated and will be removed in a future release. Please use 1 instead
include/start_slave.inc
include/assert.inc [The GTID information about the last applied transaction was preserved after the slave was stopped]
include/assert.inc [The OCT information about the last applied transaction was preserved after the slave was stopped]
include/assert.inc [The ICT information about the last applied transaction was preserved after the slave was stopped]
include/assert.inc [The start apply timestamp information about the last applied transaction was preserved after the slave was stopped]
include/assert.inc [The end apply timestamp information about the last applied transaction was preserved after the slave was stopped]
include/assert.inc [The GTID information about the last applied transaction was cleared after the slave was started]
include/assert.inc [The OCT information about the last applied transaction was cleared after the slave was started]
include/assert.inc [The ICT information about the last applied transaction was cleared after the slave was started]
include/assert.inc [The start apply timestamp information about the last applied transaction was cleared after the slave was started]
include/assert.inc [The end apply timestamp information about the last applied transaction was cleared after the slave was started]
[connection master]
DROP TABLE t1;
include/sync_slave_sql_with_master.inc
include/stop_slave.inc
SET @@GLOBAL.replica_parallel_workers = @saved_replica_parallel_workers;
include/start_slave.inc
include/rpl_end.inc
