# ==== Purpose ====
#
# The purpose of this script is to clean the setup from test file:
#   common/rpl/replica_start_after_clone.inc
#
# ==== Usage ====
#
# This script uses no parameters. After you included
#
#--source common/rpl/replica_start_after_clone.inc
#
# You can this point do extra validations.
# To clean the test setup you should invoke
#
#--source common/rpl/replica_start_after_clone_cleanup.inc
#
# ==== References ====
#
# Bug#29995256: PRIVILEGE_CHECKS_USER NOT CLONED FOR RECOVERY CHANNEL
# WL#7491: GTID-based replication applier recovery and positioning
#

--echo #
--echo # 5. Cleanup

--let $rpl_connection_name= server_1
--source include/connection.inc

DROP TABLE test.t1;

--source include/rpl/sync.inc

--let $rpl_connection_name= server_2
--source include/connection.inc

UNINSTALL PLUGIN clone;

--let $rpl_connection_name= server_3
--source include/connection.inc

STOP REPLICA;

RESET REPLICA ALL;

UNINSTALL PLUGIN clone;

--source include/rpl/clear_priv_checks_user_configuration.inc

--source include/clean_monitoring_process.inc

SET SESSION sql_log_bin=0;
call mtr.add_suppression("Clone removing all user data for provisioning: Started");
call mtr.add_suppression("Clone removing all user data for provisioning: Finished");
call mtr.add_suppression("Recovery from source pos [0-9]+ and file [a-zA-Z-]*\.*[0-9]* for channel ''*");
call mtr.add_suppression("Relay log recovery on channel with GTID_ONLY=1*");
call mtr.add_suppression("Applier metadata information for channel '' was found after a clone operation. Relay log recovery");
SET SESSION sql_log_bin=1;
