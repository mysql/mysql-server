# This test verifies that the CHANGE REPLICATION SOURCE FOR recovery channel
# command is logged in the slow log, query_log and error log, but the password
# value is either masked with '<secret>' or is not logged.
#

--let $skip_recovery_configuration=1
--source include/have_group_replication_plugin.inc
--let $rpl_skip_group_replication_start= 1
--source include/group_replication.inc

# Start group replication on server1

--let $rpl_connection_name= server1
--source include/rpl_connection.inc

--echo # Creating a connection on the first server to be used by the recovery channel
SET sql_log_bin=0;
CREATE USER 'manish'@'%' IDENTIFIED BY 'unique_password';
GRANT REPLICATION SLAVE ON *.* TO 'manish'@'%';

CREATE USER 'internal_usr'@'%' IDENTIFIED BY 'unique_password';
GRANT GROUP_REPLICATION_STREAM ON *.* TO 'internal_usr'@'%';
FLUSH PRIVILEGES;
SET sql_log_bin=1;

--disable_warnings
CHANGE MASTER TO MASTER_USER='internal_usr', MASTER_PASSWORD='unique_password' FOR CHANNEL 'group_replication_recovery';
--enable_warnings

SET @old_log_output= @@log_output;
SET GLOBAL log_output= 'TABLE';
TRUNCATE mysql.general_log;

--source include/start_and_bootstrap_group_replication.inc

--echo # CHANGE REPLICATION SOURCE with valid options will work fine and the password part will be masked with <secret>
--disable_warnings
CHANGE REPLICATION SOURCE TO SOURCE_USER='manish', SOURCE_PASSWORD='unique_password' FOR CHANNEL 'group_replication_recovery';
--enable_warnings

--echo # Since MASTER_PASSWORD is not stored in the performance schema table
--echo # (performance_schema.replication_connection_configuration) it won't be visible.
--error ER_BAD_FIELD_ERROR
SELECT password FROM performance_schema.replication_connection_configuration;

--echo server1
--let $change_count= `SELECT COUNT(*) FROM mysql.general_log WHERE argument LIKE 'CHANGE REPLICATION SOURCE TO SOURCE_USER = \'manish\', SOURCE_PASSWORD = <secret> FOR CHANNEL \'group_replication_recovery\''`
--let $assert_cond= "$change_count" = 1
--let $assert_text= The CHANGE REPLICATION SOURCE TO is logged with the password value replaced with the keyword <secret>.
--source include/assert.inc

# Checking the default values for the User_name and User_password field.

--connection server2

--let $expected_value_user=`SELECT User_name FROM mysql.slave_master_info WHERE Channel_name LIKE 'group_replication_recovery'`
--let $expected_value_password=`SELECT User_password FROM mysql.slave_master_info WHERE Channel_name LIKE 'group_replication_recovery'`
--let $assert_text= The default values of the User_name and User_password are empty without a previous change master.
--let $assert_cond= "$expected_value_user" = "" AND "$expected_value_password"= ""
--source include/assert.inc

--connection server1

# Cleanup
RESET REPLICA ALL FOR CHANNEL 'group_replication_recovery';
SET sql_log_bin=0;
DROP USER manish;
DROP USER internal_usr;
SET sql_log_bin=1;
SET @@global.log_output= @old_log_output;
TRUNCATE TABLE mysql.general_log;
--source include/group_replication_end.inc
