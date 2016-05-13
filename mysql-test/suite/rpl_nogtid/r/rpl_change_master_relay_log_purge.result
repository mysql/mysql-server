include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
FLUSH BINARY LOGS;
include/sync_slave_sql_with_master.inc

We now stop the IO thread and ensure that the relaylog files
are not deleted on doing a change master with a running SQL thread.

include/stop_slave_io.inc
CHANGE MASTER TO MASTER_HEARTBEAT_PERIOD= 10;
include/assert.inc [The relaylog file should not change if change master command was executed while SQL thread was running.]

We now stop the SQL thread and ensure that the relaylog files
are not deleted on doing a change master with a running IO thread.

include/stop_slave_sql.inc
include/start_slave_io.inc
CHANGE MASTER TO MASTER_DELAY= 20;
include/assert.inc [The relaylog file should not change if change master command was executed while IO thread was running.]

If the relay_log_file and relay_log_pos options are used, we dont purge
relaylogs.

include/stop_slave_io.inc
CHANGE MASTER TO RELAY_LOG_FILE= 'RELAY_LOG_FILE';
CHANGE MASTER TO RELAY_LOG_POS= RELAY_LOG_POS;
include/assert.inc [The relaylog file should not change if change master command contained relay_log_file or relay_log_pos option.]

With both the threads stopped and no use of relay_log_file or
relay_log_pos options, the relaylogs should be deleted.

CHANGE MASTER TO MASTER_HEARTBEAT_PERIOD= 20;
include/assert.inc [With both the threads stopped and no use of relay_log_file or relay_log_pos options in change master command, the relaylogs should be deleted.]

cleanup

include/start_slave.inc
include/rpl_end.inc
