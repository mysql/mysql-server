# ==== Purpose ====
#
# This test case will enable binlog encryption master key rotation on
# server startup forcing some failures to exercise the binlog encryption
# master key recovery process.
#
# The first failure is after generating the 1st master key and before
# storing its index in the keyring.
#
# Then, it shall fail to rotate the master key at startup because it will
# not be able to remove the "new" master key index in the cleanup process
# before the key rotation.
#
# After the two failures above, the test case will start the server with
# both binlog encryption and rotation options ON many times asserting it
# is generating the keys and rotating binary logs as expected.
#
# Finally, the slave is started making the master to read from all encrypted
# binary log files with distinct encryption keys.
#
# ==== Related Bugs and Worklogs ====
#
# WL#10957: Binary log encryption at rest
#

#
# On master
#

--disable_query_log
call mtr.add_suppression("Could not find the data corresponding to Data ID:");
--enable_query_log

--let $master_debug=`SELECT @@GLOBAL.debug`
--let $MASTER_DATADIR= `select @@datadir`
--let $master_error_log= $MYSQL_TMP_DIR/master.err

--echo Restart the master enabling the rotate option only
--let $rpl_server_number= 1
--let $rpl_server_parameters= $RPL_PLUGIN_DIR_OPT --binlog_rotate_encryption_master_key_at_startup=ON --log-error=$MYSQL_TMP_DIR/master.err
--let $rpl_omit_print_server_parameters= 1
--source include/rpl_restart_server.inc
--let $assert_text=binlog_encryption option shall be OFF
--let $option_value = `SELECT variable_value FROM performance_schema.global_variables WHERE variable_name = "binlog_encryption"`
--let $assert_cond= "$option_value" = "OFF"
--source include/assert.inc
--let $assert_file= $master_error_log
--let $assert_text= Server ignored binlog_rotate_encryption_master_key_at_startup
--let $assert_select=Ignoring binlog_rotate_encryption_master_key_at_startup
--let $assert_count= 1
--source include/assert_grep.inc
--let $binlog_file=query_get_value(SHOW MASTER STATUS, File, 1)
--let $assert_text=Binary log rotated
--let $assert_cond= RIGHT("$binlog_file", 6) = "000002"
--source include/assert.inc
--let $assert_text= 2nd binary log is not encrypted on master
--let $assert_cond= "[SHOW BINARY LOGS, Encrypted, 2]" = "No"
--source include/assert.inc

--let $rpl_server_number= 1
--source include/rpl_stop_server.inc
# Master is not able to run some queries from includes until restarted
--connection slave
--remove_file $master_error_log

--let $debug_point=rpl_encryption_first_time_enable_3
# The 1st master key will be generated in this point
--echo # Force the server to fail storing master key index
--let $error_message=Failed to store key
--let $rpl_server_number= 1
--let $rpl_server_parameters= $RPL_PLUGIN_DIR_OPT --binlog_encryption=ON --binlog_rotate_encryption_master_key_at_startup=ON --debug=d,$debug_point --log-error=$MYSQL_TMP_DIR/master.err
--echo Try to restart the master enabling the options with debug instrumentation
--error 0,1
--exec $MYSQLD --defaults-file=$MYSQLTEST_VARDIR/my.cnf --defaults-group-suffix=.$rpl_server_number $rpl_server_parameters
--let $assert_file= $master_error_log
--let $assert_text= Server reported: $error_message
--let $assert_select= $error_message
--let $assert_count= 1
--source include/assert_grep.inc
--let $assert_text= Server failed to initialize binlog encryption
--let $assert_select= Failed to initialize binlog encryption
--let $assert_count= 1
--source include/assert_grep.inc
--let $assert_text= Server did not failed to rotate binlog encryption master key at startup
--let $assert_select= Failed to rotate binlog encryption master key at startup
--let $assert_count= 0
--source include/assert_grep.inc
--let $assert_text= Server aborted to start
--let $assert_select= Server.*Aborting
--let $assert_count= 1
--source include/assert_grep.inc
--remove_file $master_error_log

--echo Restart the master enabling the options
--let $rpl_server_parameters= $RPL_PLUGIN_DIR_OPT --binlog_encryption=ON --binlog_rotate_encryption_master_key_at_startup=ON --log-error=$MYSQL_TMP_DIR/master.err
--let $rpl_omit_print_server_parameters= 1
--source include/rpl_start_server.inc
--connection master
--let $assert_text=binlog_encryption option shall be ON
--let $option_value = `SELECT variable_value FROM performance_schema.global_variables WHERE variable_name = "binlog_encryption"`
--let $assert_cond= "$option_value" = "ON"
--source include/assert.inc
--let $binlog_file=query_get_value(SHOW MASTER STATUS, File, 1)
--let $assert_text=Binary log rotated
--let $assert_cond= RIGHT("$binlog_file", 6) = "000003"
--source include/assert.inc
--let $assert_text= 3rd binary log is encrypted on master
--let $assert_cond= "[SHOW BINARY LOGS, Encrypted, 3]" = "Yes"
--source include/assert.inc
--let $rpl_log_file=$MASTER_DATADIR$binlog_file
--source include/rpl_get_log_encryption_key_id.inc
--let $assert_text=Binary log is encrypted with 2nd key
--let $assert_cond= RIGHT("$rpl_encryption_key_id", 2) = "_2"
--source include/assert.inc

# Recovery of a failed key rotation with key seqno > 1
--let $rpl_server_number= 1
--source include/rpl_stop_server.inc
# Master is not able to run some queries from includes until restarted
--connection slave
--remove_file $master_error_log
--let $point=3
--let $debug_point=rpl_encryption_first_time_enable_$point
# The 3rd master key will be generated in this point
--echo # Force the server to fail storing master key index
--let $error_message=Failed to store key
--let $rpl_server_parameters= $RPL_PLUGIN_DIR_OPT --binlog_encryption=ON --binlog_rotate_encryption_master_key_at_startup=ON --debug=d,$debug_point --log-error=$MYSQL_TMP_DIR/master.err
--echo Try to restart the master enabling the options with debug instrumentation
--error 0,1
--exec $MYSQLD --defaults-file=$MYSQLTEST_VARDIR/my.cnf --defaults-group-suffix=.$rpl_server_number $rpl_server_parameters
--let $assert_file= $master_error_log
--let $assert_text= Server reported: $error_message
--let $assert_select= $error_message
--let $assert_count= 1
--source include/assert_grep.inc
--let $assert_text= Server failed to initialize binlog encryption
--let $assert_select= Failed to initialize binlog encryption
--let $assert_count= 1
--source include/assert_grep.inc
--let $assert_text= Server did not failed to rotate binlog encryption master key at startup
--let $assert_select= Failed to rotate binlog encryption master key at startup
--let $assert_count= 0
--source include/assert_grep.inc
--let $assert_text= Server aborted to start
--let $assert_select= Server.*Aborting
--let $assert_count= 1
--source include/assert_grep.inc
--remove_file $master_error_log

--echo Restart the master enabling just the binlog_encryption option
--let $rpl_server_parameters= $RPL_PLUGIN_DIR_OPT --binlog_encryption=ON --log-error=$MYSQL_TMP_DIR/master.err
--let $rpl_omit_print_server_parameters= 1
--source include/rpl_start_server.inc
--connection master
--let $assert_text=binlog_encryption option shall be ON
--let $option_value = `SELECT variable_value FROM performance_schema.global_variables WHERE variable_name = "binlog_encryption"`
--let $assert_cond= "$option_value" = "ON"
--source include/assert.inc
--let $binlog_file=query_get_value(SHOW MASTER STATUS, File, 1)
--let $assert_text=Binary log rotated
--let $assert_cond= RIGHT("$binlog_file", 6) = "000004"
--source include/assert.inc
--let $assert_text= 4th binary log is encrypted on master
--let $assert_cond= "[SHOW BINARY LOGS, Encrypted, 4]" = "Yes"
--source include/assert.inc
--let $rpl_log_file=$MASTER_DATADIR$binlog_file
--source include/rpl_get_log_encryption_key_id.inc
--let $assert_text=Binary log is encrypted with 3th key
--let $assert_cond= RIGHT("$rpl_encryption_key_id", 2) = "_3"
--source include/assert.inc

# Generate some traffic for replication
CREATE TABLE t1 (c1 INT PRIMARY KEY AUTO_INCREMENT, c2 TEXT);
INSERT INTO t1 (c2) VALUES ("MySQL");
--let $key=3
--let $last_binlog_file=query_get_value(SHOW MASTER STATUS, File, 1)
while ($key < 9)
{
  --inc $key
  --echo Restart the master enabling the options to generate key #$key
  --let $rpl_server_number= 1
  --let $rpl_server_parameters= $RPL_PLUGIN_DIR_OPT --binlog_encryption=ON --binlog_rotate_encryption_master_key_at_startup=ON --log-error=$MYSQL_TMP_DIR/master.err
  --let $rpl_omit_print_server_parameters= 1
  --source include/rpl_restart_server.inc
  --let $assert_text=binlog_encryption option shall be ON
  --let $option_value = `SELECT variable_value FROM performance_schema.global_variables WHERE variable_name = "binlog_encryption"`
  --let $assert_cond= "$option_value" = "ON"
  --source include/assert.inc
  --let $binlog_file=query_get_value(SHOW MASTER STATUS, File, 1)
  --let $assert_text=Binary log rotated as expected
  --let $assert_cond= RIGHT("$binlog_file", 6) = 1 + RIGHT("$last_binlog_file", 6)
  --source include/assert.inc
  --let $rpl_log_file=$MASTER_DATADIR$binlog_file
  --source include/rpl_get_log_encryption_key_id.inc
  --let $assert_text=Binary log is encrypted with key #$key
  --let $assert_cond= RIGHT("$rpl_encryption_key_id", 2) = "_$key"
  --source include/assert.inc
  --let $last_binlog_file=query_get_value(SHOW MASTER STATUS, File, 1)
  INSERT INTO t1 (c2) VALUES ("MySQL");
}

# Cleanup
--source include/rpl_connection_slave.inc
--source include/start_slave.inc
--source include/rpl_connection_master.inc
SET GLOBAL binlog_encryption = OFF;
DROP TABLE t1;
# Slave shall read from all encrypted binary log files
--source include/sync_slave_sql_with_master.inc
--source include/stop_slave.inc
--let $rpl_server_number= 1
--let $rpl_server_parameters=
--let $rpl_omit_print_server_parameters= 1
--source include/rpl_restart_server.inc
--source include/rpl_connection_slave.inc
SET GLOBAL keyring_operations= ON;
