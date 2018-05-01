include/rpl_replication_observers_example_plugin_server_startup.inc
#
# Cause an explicit transaction in order to Trans Observer to be called
#
CREATE TABLE t1 (c1 INT PRIMARY KEY) Engine=InnoDB;
INSERT t1 VALUES(1);
BEGIN;
INSERT t1 VALUES(2);
ROLLBACK;
DROP TABLE t1;
#
# Uninstall the Plugin to cause output of the hooks
#
#
# Verify that all Server Listening hooks were called
#
Matching lines are:
replication_observers_example_plugin:before_handle_connection'
Occurrences of 'replication_observers_example_plugin:before_handle_connection' in the input file: 1
Matching lines are:
replication_observers_example_plugin:before_recovery'
Occurrences of 'replication_observers_example_plugin:before_recovery' in the input file: 1
Matching lines are:
replication_observers_example_plugin:after_engine_recovery'
Occurrences of 'replication_observers_example_plugin:after_engine_recovery' in the input file: 1
Matching lines are:
replication_observers_example_plugin:after_recovery'
Occurrences of 'replication_observers_example_plugin:after_recovery' in the input file: 1
#
# Re-installing the Plugin
#
INSTALL PLUGIN replication_observers_example SONAME 'replication_observers_example_plugin.so';
