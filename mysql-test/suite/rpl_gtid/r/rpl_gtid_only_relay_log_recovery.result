include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
#
# 1. Make the channel use GTID_ONLY=1
#    Create a table on the source and replicate it.
#    Add some data making the binlog and relay rotate so the info on the repos is stale
[connection slave]
include/gtid_utils.inc
CHANGE REPLICATION SOURCE TO GTID_ONLY = 1, REQUIRE_ROW_FORMAT=1;
include/start_slave.inc
[connection master]
CREATE TABLE t1 (c1 INT NOT NULL PRIMARY KEY);
include/sync_slave_sql_with_master.inc
[connection master]
INSERT INTO t1 VALUES (3);
FLUSH LOGS;
include/sync_slave_sql_with_master.inc
include/stop_slave.inc
include/start_slave.inc
[connection master]
INSERT INTO t1 VALUES (2);
FLUSH LOGS;
include/sync_slave_sql_with_master.inc
include/stop_slave.inc
include/start_slave.inc
[connection master]
INSERT INTO t1 VALUES (1);
FLUSH LOGS;
include/sync_slave_sql_with_master.inc
include/stop_slave.inc
include/start_slave.inc
#
# 2. Restart the replica with relay log recovery
#    Check that the replica wont replicate old transactions even if position are outdated
include/rpl_restart_server.inc [server_number=2 parameters: --skip_replica_start=FALSE --relay_log_recovery=1]
[connection master]
INSERT INTO t1 VALUES(4);
include/sync_slave_sql_with_master.inc
include/assert.inc [Exactly 1 GTIDs should have been retrieved since last invocation]
include/assert_grep.inc [The relay log recovery message does not mention positions]
#
# 3. Cleanup
include/stop_slave.inc
CHANGE REPLICATION SOURCE TO GTID_ONLY = 0, REQUIRE_ROW_FORMAT=0;
include/start_slave.inc
include/gtid_utils_end.inc
include/suppress_messages.inc
# Connection 1 suppresses message <Relay log recovery on channel with GTID_ONLY=1. The channel will switch to a new relay log and the GTID protocol will be used to replicate unapplied transactions.>.
# Connection 2 suppresses message <Relay log recovery on channel with GTID_ONLY=1. The channel will switch to a new relay log and the GTID protocol will be used to replicate unapplied transactions.>.
[connection master]
DROP TABLE t1;
include/rpl_end.inc
