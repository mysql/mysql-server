include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
[connection slave]
include/rpl_reset.inc
[connection master]
CREATE TABLE t1 (c1 INT);
include/sync_slave_sql_with_master.inc
CALL mtr.add_suppression("Recovery from master pos");
include/stop_slave_sql.inc
[connection master]
INSERT INTO t1 VALUES (1);
include/sync_slave_io_with_master.inc
include/assert.inc ["Relay_log_pos points at the beginning of the INSERT trx."]
include/assert.inc ["Exec_master_log_pos points at the CREATE TABLE statement."]
include/assert.inc ["Read_master_log_pos points at the end of the INSERT trx."]
Omitting server parameters, since there is a path in one of them.
include/rpl_restart_server.inc [server_number=2]
include/assert_grep.inc [Assert that the expected entry is in the error log]
include/assert.inc ["Relay_log_pos points at the beginning of the new relay log file now."]
include/assert.inc ["Read_master_log_pos after restart points at the CREATE TABLE position."]
CHANGE MASTER TO Relay_log_pos= RELAY_LOG_POS, Relay_log_file='SLAVE_RELAY_LOG_FILE_NAME';
include/start_slave_sql.inc
include/start_slave_io.inc
[connection master]
DROP TABLE t1;
include/sync_slave_sql_with_master.inc
include/rpl_end.inc
