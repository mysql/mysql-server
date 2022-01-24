include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
include/rpl_stop_server.inc [server_number=1]
# Force the server to fail storing new master key index
Try to restart the master enabling the option with debug instrumentation
include/assert_grep.inc [Server reported: Failed to store key]
include/assert_grep.inc [Server failed to initialize binlog encryption]
include/assert_grep.inc [Server aborted to start]
# Force the server to fail generating new master key
Try to restart the master enabling the option with debug instrumentation
include/assert_grep.inc [Server reported: Failed to generate key]
include/assert_grep.inc [Server failed to initialize binlog encryption]
include/assert_grep.inc [Server aborted to start]
# Force the server to fail storing master key index
Try to restart the master enabling the option with debug instrumentation
include/assert_grep.inc [Server reported: Failed to store key]
include/assert_grep.inc [Server failed to initialize binlog encryption]
include/assert_grep.inc [Server aborted to start]
Restart the master enabling the option
include/rpl_start_server.inc [server_number=1]
include/assert.inc [binlog_encryption option shall be ON]
include/assert.inc [Binary log rotated]
include/assert.inc [2th binary log is encrypted on master]
CREATE TABLE t1 (c1 INT PRIMARY KEY);
INSERT INTO t1 (c1) VALUES (1),(2),(3);
INSERT INTO t1 (c1) VALUES (4),(5),(6);
[connection slave]
include/assert.inc [1st binary log is not encrypted on slave]
Restart the slave with keyring installed
include/rpl_restart_server.inc [server_number=2]
include/assert.inc [binlog_encryption option shall be OFF]
include/assert.inc [2nd binary log is not encrypted on slave]
# Force the server to fail storing new master key index
SET GLOBAL binlog_encryption = ON;
ERROR HY000: Failed to store key, please check if keyring is loaded.
include/assert.inc [binlog_encryption option shall be OFF]
include/assert.inc [Binary log did not rotated]
# Force the server to fail generating new master key
SET GLOBAL binlog_encryption = ON;
ERROR HY000: Failed to generate key, please check if keyring is loaded.
include/assert.inc [binlog_encryption option shall be OFF]
include/assert.inc [Binary log did not rotated]
# Force the server to fail storing master key index
SET GLOBAL binlog_encryption = ON;
ERROR HY000: Failed to store key, please check if keyring is loaded.
include/assert.inc [binlog_encryption option shall be OFF]
include/assert.inc [Binary log did not rotated]
SET GLOBAL binlog_encryption = ON;
include/assert.inc [Binary log rotated]
include/assert.inc [5th binary log is encrypted on slave]
==== Cleanup ====
* Disable encryption, cleanup database, and consume all encrypted logs.
[connection slave]
include/start_slave.inc
SET GLOBAL binlog_encryption = OFF;
[connection master]
SET GLOBAL binlog_encryption = OFF;
DROP TABLE t1;
include/sync_slave_sql_with_master.inc
* Remove all encrypted logs.
include/rpl_reset.inc
* Uninstall the plugins and delete the keyrings.
[connection master]
UNINSTALL PLUGIN keyring_file;
[connection slave]
UNINSTALL PLUGIN keyring_file;
* Restart the servers with the same command lines as before the test.
include/rpl_restart_server.inc [server_number=1]
include/rpl_restart_server.inc [server_number=2]
* Clean up replication test framework
[connection slave]
include/start_slave.inc
include/rpl_end.inc
