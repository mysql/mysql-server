#
# The purpose of this include file is to demonstrate the capabilities of the
# Replication Observers example plugin and to assess its correct functionality
#
# The plugin must be loaded at startup in order to be able to capture Server
# events.
#
# Since one wants to test only the hook execution in place, a test
# plugin was created to serve those needs. The plugin will print once
# if the hook method is executed and the test will verify if the hook
# was called.
#
# This test will use two different groups of hooks/observers:
# - Server State
#
# Server State uses hooks:
# - before_handle_connection: Called before a new client connection.
# - before_recovery: Called before the server initializes all
#                    recovery procedures.
# - after_engine_recovery: After binlog engine is ready.
# - after_recovery: Called after the server ends all
#                   recovery procedures.
#

--let $include_filename= rpl_replication_observers_example_plugin_server_startup.inc
--source include/begin_include_file.inc

--source include/have_binlog_format_mixed.inc
--source include/not_group_replication_plugin.inc
--source include/have_replication_observers_example_plugin.inc

--echo #
--echo # Cause an explicit transaction in order to Trans Observer to be called
--echo #

CREATE TABLE t1 (c1 INT PRIMARY KEY) Engine=InnoDB;

INSERT t1 VALUES(1);

BEGIN;
INSERT t1 VALUES(2);
ROLLBACK;

DROP TABLE t1;

--echo #
--echo # Uninstall the Plugin to cause output of the hooks
--echo #

--source include/uninstall_replication_observers_example.inc

--echo #
--echo # Verify that all Server Listening hooks were called
--echo #

--let $grep_file=$MYSQLTEST_VARDIR/tmp/rpl_replication_observers_example_plugin_server_startup.err
--let $grep_pattern=replication_observers_example_plugin:before_handle_connection
--source include/grep_pattern.inc

--let $grep_file=$MYSQLTEST_VARDIR/tmp/rpl_replication_observers_example_plugin_server_startup.err
--let $grep_pattern=replication_observers_example_plugin:before_recovery
--source include/grep_pattern.inc

--let $grep_file=$MYSQLTEST_VARDIR/tmp/rpl_replication_observers_example_plugin_server_startup.err
--let $grep_pattern=replication_observers_example_plugin:after_engine_recovery
--source include/grep_pattern.inc

--let $grep_file=$MYSQLTEST_VARDIR/tmp/rpl_replication_observers_example_plugin_server_startup.err
--let $grep_pattern=replication_observers_example_plugin:after_recovery
--source include/grep_pattern.inc

--let $include_filename= rpl_replication_observers_example_plugin_server_startup.inc
--source include/end_include_file.inc
