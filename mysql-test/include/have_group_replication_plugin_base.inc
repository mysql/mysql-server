# ==== Purpose ====
#
# Check that Group Replication plugin is installed and that test
# case server configuration fulfills Group Replication requirements.
#
# This file is only to be included on "include" files, in test
# cases please include have_group_replication_plugin.inc
#
# ==== Usage ====
#
#   --source include/have_group_replication_plugin_base.inc
#

--source include/have_log_bin.inc
--source include/have_binlog_format_row.inc

#
# Check if server has support for loading plugins
#
if (`SELECT @@have_dynamic_loading != 'YES'`) {
  --skip Requires dynamic loading
}

if (!$GROUP_REPLICATION)
{
  --skip Need Group Replication plugin
}

if (!`select count(*) from information_schema.plugins where PLUGIN_NAME like 'group_replication'`)
{
  --skip Need Group Replication plugin
}
