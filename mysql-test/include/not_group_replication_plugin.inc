#  This inc file checks for the Group Replication plugin in
#  the information_schema.plugins and if the inc file is not
#  available the particular test should be skipped.
#
#  Used to suppress the test run with Group Replication.
#
--let $include_silent= 1
--source include/rpl/group_replication_plugin_installed.inc
if ($group_replication_plugin_installed)
{
  --skip Test doesn't support Group Replication plugin
}
--let $include_silent= 0
