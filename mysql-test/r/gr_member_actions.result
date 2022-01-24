SELECT group_replication_disable_member_action("mysql_disable_super_read_only_if_primary", "AFTER_PRIMARY_ELECTION");
ERROR 42000: FUNCTION test.group_replication_disable_member_action does not exist
SELECT group_replication_enable_member_action("mysql_disable_super_read_only_if_primary", "AFTER_PRIMARY_ELECTION");
ERROR 42000: FUNCTION test.group_replication_enable_member_action does not exist
SELECT group_replication_reset_member_actions();
ERROR 42000: FUNCTION test.group_replication_reset_member_actions does not exist
SELECT * FROM performance_schema.replication_group_member_actions;
ERROR 42S02: Table 'performance_schema.replication_group_member_actions' doesn't exist
SELECT * FROM performance_schema.replication_group_configuration_version;
ERROR 42S02: Table 'performance_schema.replication_group_configuration_version' doesn't exist
