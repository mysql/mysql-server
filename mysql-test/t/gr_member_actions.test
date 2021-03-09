###############################################################################
# Validate that the UDFs:
#   group_replication_enable_member_action
#   group_replication_disable_member_action
#   group_replication_reset_member_actions
# and the tables
#   performance_schema.replication_group_member_actions
#   performance_schema.replication_group_configuration_version
# are not available when Group Replication is not installed.
###############################################################################
--source include/not_group_replication_plugin.inc

--error ER_SP_DOES_NOT_EXIST
SELECT group_replication_disable_member_action("mysql_disable_super_read_only_if_primary", "AFTER_PRIMARY_ELECTION");

--error ER_SP_DOES_NOT_EXIST
SELECT group_replication_enable_member_action("mysql_disable_super_read_only_if_primary", "AFTER_PRIMARY_ELECTION");

--error ER_SP_DOES_NOT_EXIST
SELECT group_replication_reset_member_actions();

--error ER_NO_SUCH_TABLE
SELECT * FROM performance_schema.replication_group_member_actions;

--error ER_NO_SUCH_TABLE
SELECT * FROM performance_schema.replication_group_configuration_version;
