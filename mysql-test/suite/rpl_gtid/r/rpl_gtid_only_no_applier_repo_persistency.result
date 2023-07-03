include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
#
# 1. Create a table on the source and replicate it.
#    Check the repo shows the same value as SHOW REPLICA STATUS
[connection slave]
SET @@GLOBAL.replica_checkpoint_period= 1;
[connection master]
CREATE TABLE t1 (c1 INT NOT NULL PRIMARY KEY);
INSERT INTO t1 VALUES (1);
include/sync_slave_sql_with_master.inc
include/assert.inc [The repository has the lastest position]
include/assert.inc [The file name for the repository and the show replica status match]
#
# 2. Stop replication and make the channel use GTID_ONLY=1
#    Start replication and add some values to the table.
#    Check the repo shows the old values in relation to SHOW REPLICA STATUS
include/stop_slave.inc
CHANGE REPLICATION SOURCE TO GTID_ONLY = 1, REQUIRE_ROW_FORMAT=1;
include/start_slave.inc
[connection master]
INSERT INTO t1 VALUES (2);
include/sync_slave_sql_with_master.inc
include/assert.inc [The repository position is not the one shown on show replica status]
include/assert.inc [The repository pos value is still the same]
include/assert.inc [The repository name value is still the same]
#
# 3. Stop replication
#    Check the repo still shows the old values in relation to SHOW REPLICA STATUS
include/stop_slave.inc
include/assert.inc [The repository position is not the one shown on show replica status]
include/assert.inc [The repository pos value is still the same]
include/assert.inc [The repository name value is still the same]
#
# 4. Start replication again
#    Check the repo still shows the old values in relation to SHOW REPLICA STATUS
include/start_slave.inc
[connection master]
INSERT INTO t1 VALUES (3);
include/sync_slave_sql_with_master.inc
include/assert.inc [The repository position is not the one shown on show replica status]
include/assert.inc [The repository file name is not the one shown on show replica status]
include/assert.inc [The repository pos value is still the same]
include/assert.inc [The repository name value is still the same]
#
# 5. Kill the SQL thread
#    Check the repo still shows the old values in relation to SHOW REPLICA STATUS
include/wait_for_slave_sql_error.inc [errno=1756]
include/assert.inc [The repository position is not the one shown on show replica status]
include/assert.inc [The repository file name is not the one shown on show replica status]
include/assert.inc [The repository pos value is still the same]
include/assert.inc [The repository name value is still the same]
#
# 6. Change a replication connection setting with CHANGE REPLICATION SOURCE
#    Check the repo now has the same value in relation to SHOW REPLICA STATUS
include/stop_slave.inc
CHANGE REPLICATION SOURCE TO SOURCE_CONNECT_RETRY = 10000;
include/assert.inc [The repository has the lastest position]
include/assert.inc [The file name for the repository and the show replica status match]
#
# 7.  Restart the replica server
#    Check the repo still shows the old values
include/start_slave.inc
[connection master]
INSERT INTO t1 VALUES (4);
include/sync_slave_sql_with_master.inc
include/assert.inc [The repository position is not the one shown on show replica status]
include/rpl_restart_server.inc [server_number=2 parameters: --skip_replica_start=FALSE]
include/assert.inc [The repository pos value is still the same]
#
# 8. Add some values and check the repo is again stale
#    Execute a RESET REPLICA command
#    Check the repo persisted the reset position
[connection master]
INSERT INTO t1 VALUES (5);
include/sync_slave_sql_with_master.inc
include/assert.inc [The repository position is not the one shown on show replica status]
include/stop_slave.inc
RESET REPLICA;
SELECT Relay_log_pos FROM mysql.slave_relay_log_info;;
Relay_log_pos	NULL
0

include/assert.inc [The position was reset as shown on show replica status]
include/assert.inc [The repository reset the position]
#
# 9. Check RESET REPLICA ALL removes all the information
include/start_slave.inc
[connection master]
INSERT INTO t1 VALUES (6);
include/sync_slave_sql_with_master.inc
include/assert.inc [The repository position was persisted when the channel was initialized after the reset]
include/stop_slave.inc
RESET REPLICA ALL;
include/assert.inc [The repository info was deleted]
#
# 10. Cleanup
SET @@GLOBAL.replica_checkpoint_period=CHECKPOINT_PERIOD;;
CHANGE REPLICATION SOURCE TO SOURCE_HOST='127.0.0.1', SOURCE_PORT=MASTER_PORT, SOURCE_USER='root';
Warnings:
Note	1759	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	1760	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
include/start_slave.inc
call mtr.add_suppression("The slave coordinator and worker threads are stopped, possibly leaving data in inconsistent state");
[connection master]
DROP TABLE t1;
include/rpl_end.inc
