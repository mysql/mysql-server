include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
#
# 1. Setup:
#     Start the replica channel with GTID_ONLY=1
#     Add some info in the source and replicate it
#     Record the source positions to wait later on
[connection slave]
CHANGE REPLICATION SOURCE TO REQUIRE_ROW_FORMAT = 1, GTID_ONLY = 1;
[connection master]
CREATE TABLE t(s INT);
INSERT INTO t VALUES(1);
INSERT INTO t VALUES(2);
#
# 2. Start replication
#    Check SOURCE_POS_WAIT does not time out when giving it a valid positioning info
[connection slave]
include/start_slave.inc
[connection master]
include/sync_slave_sql_with_master.inc
include/assert.inc [The function did not time out]
#
# 3. Restart the server so positions become invalid
#    Use relay log recovery so the new relay log contains no source info when the applier starts
include/rpl_restart_server.inc [server_number=2 parameters: --skip_slave_start=1 --relay-log-recovery=1]
include/start_slave_sql.inc
include/assert.inc [The source log file name in the applier is invalid]
include/assert.inc [The source log file position in the applier is 0]
#
# 4. Test that the command times out cause positions are invalid
[connection server_2]
include/assert.inc [The function timed out]
#
# 5. Execute a wait with a large timeout
#    Turn applier positions valid again
#    Check the function does not time out when positions become valid
CREATE TABLE query_result(res INT);
INSERT INTO query_result SELECT SOURCE_POS_WAIT('LOG_FILE_ON_SOURCE', LOG_POS_ON_SOURCE, 100);;
[connection slave]
include/start_slave_io.inc
[connection master]
INSERT INTO t VALUES(3);
[connection server_2]
include/assert.inc [The function did not time out]
#
# 6. SOURCE_POS_WAIT does not timeout when invoked with valid positioning info
[connection slave]
include/assert.inc [The function did not time out]
#
# 7. Cleanup
include/stop_slave.inc
CHANGE REPLICATION SOURCE TO REQUIRE_ROW_FORMAT = 0, GTID_ONLY = 0;
include/start_slave.inc
CALL mtr.add_suppression("Relay log recovery on channel with GTID_ONLY=1*");
DROP TABLE query_result;
[connection master]
DROP TABLE t;
include/rpl_end.inc
