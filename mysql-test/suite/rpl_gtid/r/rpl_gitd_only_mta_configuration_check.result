include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]

############################################################
# 1. Configure the channel with GTID_ONLY = 1
#    Add some data to populate repository tables
[connection slave]
CHANGE REPLICATION SOURCE TO REQUIRE_ROW_FORMAT = 1, GTID_ONLY = 1;
include/start_slave.inc
[connection master]
CREATE TABLE t1 (a INT PRIMARY KEY);
INSERT INTO t1 VALUES(1);
include/sync_slave_sql_with_master.inc

############################################################
# 2. Restart the replica.
#    Check there is no worker info as it was purged after start
include/rpl_restart_server.inc [server_number=2 parameters: --skip_replica_start=1]
include/assert.inc [No info exists in the repository table]

############################################################
# 3. Set the number of applier workers to 0
#    Check the channel still has one worker.
SET @@GLOBAL.replica_parallel_workers = 0;
Warnings:
Warning	1287	'0' is deprecated and will be removed in a future release. Please use 1 instead
include/start_slave.inc
include/assert.inc [There is one worker runnning with id 1]

############################################################
# 4. Disable GTID_ONLY
#    Check the channel has no workers.
include/stop_slave.inc
CHANGE REPLICATION SOURCE TO GTID_ONLY = 0;
include/start_slave.inc
include/assert.inc [There is no worker with an ID]

############################################################
# 5. Cleanup
[connection master]
DROP TABLE t1;
include/sync_slave_sql_with_master.inc
SET @@GLOBAL.replica_parallel_workers = NUMBER_OF_WORKERS;
include/rpl_end.inc
