# ==== Purpose ====
#
# This test case will force failures in the binlog encryption master key
# generation using debug instrumentation.
#
# On the master, it will exercise the recovery process while setting the
# option as a startup parameter.
#
# On the slave, it will exercise the recovery process while setting the
# option in a client session.
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

--let $rpl_server_number= 1
--source include/rpl/stop_server.inc
# Master is not able to run some queries from includes until restarted
--connection slave
--remove_file $master_error_log

--let $point=0
while ($point < 3)
{
  --inc $point
  --let $debug_point=rpl_encryption_first_time_enable_$point
  --let $rpl_server_error= 0
  if ($point == 1)
  {
    --echo # Force the server to fail storing new master key index
    --let $rpl_server_error= 1
    --let $error_message=Failed to store key
  }
  if ($point == 2)
  {
    --echo # Force the server to fail generating new master key
    --let $rpl_server_error= 1
    --let $error_message=Failed to generate key
  }
  if ($point == 3)
  {
    --echo # Force the server to fail storing master key index
    --let $rpl_server_error= 1
    --let $error_message=Failed to store key
  }
  --let $rpl_server_number= 1
  --let $rpl_server_parameters=$RPL_PLUGIN_DIR_OPT --binlog_encryption=ON --debug=d,$debug_point --log-error=$MYSQL_TMP_DIR/master.err
  --echo Try to restart the master enabling the option with debug instrumentation
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
  --let $assert_text= Server aborted to start
  --let $assert_select= Server.*Aborting
  --let $assert_count= 1
  --source include/assert_grep.inc
  --remove_file $master_error_log
}

--echo Restart the master enabling the option
--let $rpl_server_number= 1
--let $rpl_server_parameters= $RPL_PLUGIN_DIR_OPT --binlog_encryption=ON --log-error=$MYSQL_TMP_DIR/master.err
--let $rpl_server_error= 0
--let $rpl_omit_print_server_parameters= 1
--source include/rpl/start_server.inc
--connection master
--let $assert_text=binlog_encryption option shall be ON
--let $option_value = `SELECT variable_value FROM performance_schema.global_variables WHERE variable_name = "binlog_encryption"`
--let $assert_cond= "$option_value" = "ON"
--source include/assert.inc
--let $binlog_file=query_get_value(SHOW BINARY LOG STATUS, File, 1)
--let $assert_text=Binary log rotated
--let $assert_cond= RIGHT("$binlog_file", 6) = "000002"
--source include/assert.inc
--let $assert_text= 2th binary log is encrypted on master
--let $assert_cond= "[SHOW BINARY LOGS, Encrypted, 2]" = "Yes"
--source include/assert.inc
# Generate some traffic for replication
CREATE TABLE t1 (c1 INT PRIMARY KEY);
INSERT INTO t1 (c1) VALUES (1),(2),(3);
INSERT INTO t1 (c1) VALUES (4),(5),(6);

#
# On slave
#

# Restart the slave loading the keyring
--source include/rpl/connection_replica.inc
--let $SLAVE_DATADIR= `select @@datadir`
--let $assert_text= 1st binary log is not encrypted on slave
--let $assert_cond= "[SHOW BINARY LOGS, Encrypted, 1]" = "No"
--source include/assert.inc
--echo Restart the slave with keyring installed
--let $rpl_server_number= 2
--let $rpl_server_parameters= --skip-replica-start $RPL_PLUGIN_DIR_OPT
--let $rpl_server_error= 0
--let $rpl_omit_print_server_parameters= 1
--source include/rpl/restart_server.inc
--let $assert_text=binlog_encryption option shall be OFF
--let $option_value = `SELECT variable_value FROM performance_schema.global_variables WHERE variable_name = "binlog_encryption"`
--let $assert_cond= "$option_value" = "OFF"
--source include/assert.inc
--let $assert_text= 2nd binary log is not encrypted on slave
--let $assert_cond= "[SHOW BINARY LOGS, Encrypted, 2]" = "No"
--source include/assert.inc

--let $point=0
while ($point < 3)
{
  --inc $point
  --let $debug_point=rpl_encryption_first_time_enable_$point
  --let $debug_point_silent=1
  --source include/add_debug_point.inc
  --let $expected_error=0
  if ($point == 1)
  {
    --echo # Force the server to fail storing new master key index
    --let $expected_error=ER_RPL_ENCRYPTION_FAILED_TO_STORE_KEY
  }
  if ($point == 2)
  {
    --echo # Force the server to fail generating new master key
    --let $expected_error=ER_RPL_ENCRYPTION_FAILED_TO_GENERATE_KEY
  }
  if ($point == 3)
  {
    --echo # Force the server to fail storing master key index
    --let $expected_error=ER_RPL_ENCRYPTION_FAILED_TO_STORE_KEY
  }
  --error $expected_error
  SET GLOBAL binlog_encryption = ON;
  --source include/remove_debug_point.inc
  --let $assert_text=binlog_encryption option shall be OFF
  --let $option_value = `SELECT variable_value FROM performance_schema.global_variables WHERE variable_name = "binlog_encryption"`
  --let $assert_cond= "$option_value" = "OFF"
  --source include/assert.inc
  --let $binlog_file=query_get_value(SHOW BINARY LOG STATUS, File, 1)
  --let $assert_text=Binary log did not rotated
  --let $assert_cond= RIGHT("$binlog_file", 6) = "000002"
  --source include/assert.inc
}

# Without warnings
SET GLOBAL binlog_encryption = ON;
--let $assert_text=binlog_encryption option shall be ON
--let $option_value = `SELECT variable_value FROM performance_schema.global_variables WHERE variable_name = "binlog_encryption"`
--let $assert_cond= "$option_value" = "ON"
--let $binlog_file=query_get_value(SHOW BINARY LOG STATUS, File, 1)
--let $assert_text=Binary log rotated
--let $assert_cond= RIGHT("$binlog_file", 6) = "000003"
--source include/assert.inc
--let $assert_text= 5th binary log is encrypted on slave
--let $assert_cond= "[SHOW BINARY LOGS, Encrypted, 3]" = "Yes"
--source include/assert.inc

# Cleanup
--source include/rpl/start_replica.inc
--source include/rpl/connection_source.inc
SET GLOBAL binlog_encryption = OFF;
DROP TABLE t1;
--source include/rpl/sync_to_replica.inc
SET GLOBAL binlog_encryption = OFF;
--source include/rpl/connection_source.inc
