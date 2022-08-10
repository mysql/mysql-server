###############################################################################
# This test verifies that if the missing transactions on a joining members are
# above threshold the server will use clone as its recovery strategy with SSL
#
# 0. The test requires two servers: M1 and M2.
# 1. Install the clone plugin on server 1
# 2. Create a user that requires SSL
# 3. Bootstrap server1 and add some data
# 4. Restart server 2 with a monitoring process (mysqld_safe) if needed
# 5. Install the clone plugin
#    Setup the server so group replication starts on boot
# 6. Configure the plugin to use SSL while Cloning
#    Start group replication on server 2
#    Wait for it to restart and come back
#    Check clone was completed
# 7. Cleanup
#

--source include/have_mysqld_monitoring_process.inc
--source include/have_clone_plugin.inc
--source include/force_restart.inc
--source include/have_group_replication_plugin.inc
--let $rpl_skip_group_replication_start= 1
--source include/group_replication.inc

# Validate plugins
--let plugins = CLONE_PLUGIN,GROUP_REPLICATION
--source include/check_plugin_dir.inc

--echo
--echo ############################################################
--echo # 1. Install clone plugin on server1.

--let $rpl_connection_name= server1
--source include/rpl_connection.inc

--replace_result $CLONE_PLUGIN CLONE_PLUGIN
--eval INSTALL PLUGIN clone SONAME '$CLONE_PLUGIN'

--echo
--echo ############################################################
--echo # 2. Create a user that requires SSL

SET SESSION sql_log_bin=0;
CREATE USER 'rec_ssl_user'@'%' REQUIRE SSL;
GRANT replication slave ON *.* TO 'rec_ssl_user'@'%';
GRANT BACKUP_ADMIN ON *.* TO "rec_ssl_user";
GRANT GROUP_REPLICATION_STREAM ON *.* TO 'rec_ssl_user'@'%';
FLUSH PRIVILEGES;
SET SESSION sql_log_bin=1;

--let $conf_rec_ssl_mode_s1= `SELECT @@GLOBAL.group_replication_ssl_mode;`
--eval SET GLOBAL group_replication_ssl_mode= REQUIRED

--disable_warnings
CHANGE REPLICATION SOURCE TO SOURCE_USER="rec_ssl_user" FOR CHANNEL "group_replication_recovery";
--enable_warnings

--echo
--echo ############################################################
--echo # 3. Bootstrap server1 and add some data

--source include/start_and_bootstrap_group_replication.inc

CREATE TABLE t1 (c1 INT NOT NULL PRIMARY KEY) ENGINE=InnoDB;
INSERT INTO t1 VALUES (1);
INSERT INTO t1 VALUES (2);

--echo
--echo #######################################################################
--echo # 4. Restart server 2 with a monitoring process (mysqld_safe) if needed

--let $rpl_connection_name= server2
--source include/rpl_connection.inc

SET SESSION sql_log_bin=0;
CREATE USER 'rec_ssl_user'@'%' REQUIRE SSL;
GRANT replication slave ON *.* TO 'rec_ssl_user'@'%';
GRANT BACKUP_ADMIN ON *.* TO "rec_ssl_user";
GRANT GROUP_REPLICATION_STREAM ON *.* TO 'rec_ssl_user'@'%';
FLUSH PRIVILEGES;
SET SESSION sql_log_bin=1;

--let $conf_rec_ssl_mode_s2= `SELECT @@GLOBAL.group_replication_ssl_mode;`
--eval SET GLOBAL group_replication_ssl_mode= REQUIRED

--let $_group_replication_local_address= `SELECT @@GLOBAL.group_replication_local_address`
--let $_group_replication_group_seeds= `SELECT @@GLOBAL.group_replication_group_seeds`
--let $_group_replication_comm_stack= `SELECT @@GLOBAL.group_replication_communication_stack`
--let $_group_replication_ssl_mode= `SELECT @@GLOBAL.group_replication_ssl_mode`

--let $plugin_list= $GROUP_REPLICATION
--source include/spawn_monitoring_process.inc

--echo
--echo ############################################################
--echo # 5. Setup the server so group replication starts on boot

--disable_query_log
--eval SET PERSIST group_replication_group_name= "$group_replication_group_name"
--eval SET PERSIST group_replication_local_address= "$_group_replication_local_address"
--eval SET PERSIST group_replication_group_seeds= "$_group_replication_group_seeds"
--eval SET PERSIST group_replication_communication_stack= "$_group_replication_comm_stack"
--eval SET PERSIST group_replication_ssl_mode= "$_group_replication_ssl_mode"

SET PERSIST group_replication_start_on_boot= ON;

# Configure also SSL settings

--eval SET PERSIST group_replication_recovery_use_ssl=1
--replace_result $MYSQL_TEST_DIR MYSQL_TEST_DIR
--eval SET PERSIST group_replication_recovery_ssl_ca= '$MYSQL_TEST_DIR/std_data/cacert.pem'
--replace_result $MYSQL_TEST_DIR MYSQL_TEST_DIR
--eval SET PERSIST group_replication_recovery_ssl_cert= '$MYSQL_TEST_DIR/std_data/client-cert.pem'
--replace_result $MYSQL_TEST_DIR MYSQL_TEST_DIR
--eval SET PERSIST group_replication_recovery_ssl_key= '$MYSQL_TEST_DIR/std_data/client-key.pem'
--enable_query_log

--replace_result $CLONE_PLUGIN CLONE_PLUGIN
--eval INSTALL PLUGIN clone SONAME '$CLONE_PLUGIN'

--echo
--echo ############################################################
--echo # 6. Configure the plugin to use SSL while Cloning
--echo #    Start group replication on server 2
--echo #    Wait for it to restart and come back
--echo #    Check clone was completed

--disable_warnings
CHANGE REPLICATION SOURCE TO SOURCE_USER="rec_ssl_user" FOR CHANNEL "group_replication_recovery";
--enable_warnings

--let $_group_replication_threshold_save= `SELECT @@GLOBAL.group_replication_clone_threshold`
SET GLOBAL group_replication_clone_threshold= 1;

START GROUP_REPLICATION;

--source include/wait_until_disconnected.inc

--let $rpl_server_number= 2
--source include/rpl_reconnect.inc

--let $group_replication_member_state=ONLINE
--source include/gr_wait_for_member_state.inc

--let $assert_text= Clone must be completed
--let $assert_cond= [SELECT state="Completed" FROM performance_schema.clone_status] = 1;
--source include/assert.inc

# See if the data has been properly cloned in server2
--let $diff_tables=server1:test.t1 ,server2:test.t1
--source include/diff_tables.inc

--echo
--echo ############################################################
--echo # 7. Cleanup

--eval SET GLOBAL group_replication_clone_threshold= $_group_replication_threshold_save;

RESET PERSIST IF EXISTS group_replication_group_name;
RESET PERSIST IF EXISTS group_replication_local_address;
RESET PERSIST IF EXISTS group_replication_group_seeds;
RESET PERSIST IF EXISTS group_replication_start_on_boot;
RESET PERSIST IF EXISTS group_replication_communication_stack;
RESET PERSIST IF EXISTS group_replication_ssl_mode;

RESET PERSIST IF EXISTS group_replication_recovery_use_ssl;
RESET PERSIST IF EXISTS group_replication_recovery_ssl_ca;
RESET PERSIST IF EXISTS group_replication_recovery_ssl_cert;
RESET PERSIST IF EXISTS group_replication_recovery_ssl_key;

--disable_query_log
--eval SET GLOBAL group_replication_ssl_mode= $conf_rec_ssl_mode_s2
--enable_query_log

DROP TABLE t1;

--source include/rpl_sync.inc

set session sql_log_bin=0;
call mtr.add_suppression("This member will start distributed recovery using clone. It is due to the number of missing transactions being higher than the configured threshold of*");
call mtr.add_suppression("Clone removing all user data for provisioning: Started");
call mtr.add_suppression("Clone removing all user data for provisioning: Finished");
set session sql_log_bin=1;

--source include/clean_monitoring_process.inc

--connection server1
--echo server1

--disable_query_log
--eval SET GLOBAL group_replication_ssl_mode= $conf_rec_ssl_mode_s1
--enable_query_log

--source include/group_replication_end.inc
