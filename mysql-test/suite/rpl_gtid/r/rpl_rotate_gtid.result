include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
# Preparing the relaylog
[connection slave]
include/stop_slave.inc
CHANGE REPLICATION SOURCE TO SOURCE_AUTO_POSITION=0;
include/start_slave_io.inc
SET @save_debug=@@global.debug;
SET GLOBAL DEBUG='+d,stop_io_after_reading_gtid_log_event';
# Create a transaction on the master
# that will have it's GTID and other events in separated relay log files
[connection master]
CREATE TABLE t1 (c1 INT) ENGINE=InnoDB;
[connection slave]
include/wait_for_slave_io_to_stop.inc
SET GLOBAL DEBUG=@save_debug;
include/start_slave_io.inc
# Create other transaction on the master
[connection master]
CREATE TABLE t2 (c1 INT) ENGINE=InnoDB;
include/sync_slave_io_with_master.inc
# Restart the SQL thread until the beginning of the last relay log file
[connection slave]
START SLAVE SQL_THREAD UNTIL
RELAY_LOG_FILE= 'slave-relay-bin.000003',
RELAY_LOG_POS= 4;
include/wait_for_slave_sql_to_stop.inc
# Restart the SQL thread
include/start_slave_sql.inc
[connection master]
include/sync_slave_sql_with_master.inc
include/assert.inc [t1 should be replicated]
# Cleanup
[connection master]
DROP TABLE t1, t2;
include/rpl_end.inc
