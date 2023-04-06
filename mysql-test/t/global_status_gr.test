################################################################################
# Validate that Group Replication global status variables are not available
# when the plugin is not installed.
################################################################################
--source include/not_group_replication_plugin.inc

--let $assert_text= 'There are no Group Replication global status variables'
--let $assert_cond= COUNT(*)=0 FROM performance_schema.global_status WHERE VARIABLE_NAME LIKE "Gr\_%"
--source include/assert.inc

SHOW GLOBAL STATUS LIKE 'Gr\_%';
