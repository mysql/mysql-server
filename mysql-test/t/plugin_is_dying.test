
--source include/have_example_plugin.inc
--source include/have_debug.inc
--source include/have_debug_sync.inc

connect(con1, localhost, root,,);

--connection default

--disable_result_log
SHOW PLUGINS;
--enable_result_log

SELECT PLUGIN_NAME, PLUGIN_STATUS, LOAD_OPTION from INFORMATION_SCHEMA.PLUGINS
  WHERE PLUGIN_NAME like 'pfs_example%';

--connection con1

SET DEBUG_SYNC='in_plugin_initialize WAIT_FOR go_init';

--echo "con1 installing plugin ..."
--replace_regex /\.dll/.so/
--send_eval INSTALL PLUGIN pfs_example_plugin_employee SONAME '$PFS_EXAMPLE_PLUGIN_EMPLOYEE'

--connection default

# Wait for INSTALL PLUGIN to hit in_plugin_initialize
let $wait_condition= SELECT count(*) = 1 FROM INFORMATION_SCHEMA.PLUGINS
                     WHERE PLUGIN_NAME like 'pfs_example%' and
                     PLUGIN_STATUS='INACTIVE';
--source include/wait_condition.inc

# Observe the INACTIVE status

--disable_result_log
SHOW PLUGINS;
--enable_result_log

SELECT PLUGIN_NAME, PLUGIN_STATUS, LOAD_OPTION from INFORMATION_SCHEMA.PLUGINS
  WHERE PLUGIN_NAME like 'pfs_example%';

SET DEBUG_SYNC='now SIGNAL go_init';

--connection con1
--reap

--connection default

# Wait for INSTALL PLUGIN to complete
let $wait_condition= SELECT count(*) = 1 FROM INFORMATION_SCHEMA.PLUGINS
                     WHERE PLUGIN_NAME like 'pfs_example%' and
                     PLUGIN_STATUS='ACTIVE';
--source include/wait_condition.inc

# Observe the ACTIVE status

--disable_result_log
SHOW PLUGINS;
--enable_result_log

SELECT PLUGIN_NAME, PLUGIN_STATUS, LOAD_OPTION from INFORMATION_SCHEMA.PLUGINS
  WHERE PLUGIN_NAME like 'pfs_example%';

--connection con1

SET DEBUG_SYNC='in_plugin_check_uninstall WAIT_FOR go_deinit';

--send UNINSTALL PLUGIN pfs_example_plugin_employee;

--connection default

# Wait for UNINSTALL PLUGIN to hit in_plugin_check_uninstall
let $wait_condition= SELECT count(*) = 1 FROM INFORMATION_SCHEMA.PLUGINS
                     WHERE PLUGIN_NAME like 'pfs_example%' and
                     PLUGIN_STATUS ='DELETING';
--source include/wait_condition.inc

# Observe the DELETING status

--disable_result_log
SHOW PLUGINS;
--enable_result_log

SELECT PLUGIN_NAME, PLUGIN_STATUS, LOAD_OPTION from INFORMATION_SCHEMA.PLUGINS
  WHERE PLUGIN_NAME like 'pfs_example%';

SET DEBUG_SYNC='now SIGNAL go_deinit';

--connection con1
--reap

--connection default

# Observe the plugin is gone

--disable_result_log
SHOW PLUGINS;
--enable_result_log

SELECT PLUGIN_NAME, PLUGIN_STATUS, LOAD_OPTION from INFORMATION_SCHEMA.PLUGINS
  WHERE PLUGIN_NAME like 'pfs_example%';

--disconnect con1

