# ==== Purpose ====
#
# This include will INSTALL group_replication plugin.
#
# Notes:
#
# Post INSTALL PLUGIN group_replication, group_replication entry needs
# to be cleaned from mysql.plugin table. So we set install_clean_up_required=1
# and group_replication_end.inc calls clean_group_replication_install_entry.inc
# and does the cleanup.
# Delayed cleanup is needed since server may be in read-only mode
# causing INSTALL to succeed while DELETE command fails.
#
#   $install_clean_up_required (set by this file)
#     This flag informs group_replication_end.inc that group_replication
#     entry from mysql.plugin table needs to be removed.
#
# ==== Usage ====
#
# --source include/install_group_replication_plugin.inc
#

--let $include_filename= install_group_replication_plugin.inc
--source include/begin_include_file.inc

# Install group replication plugin
--disable_query_log
--disable_warnings

--let $value= query_get_value("SELECT Plugin_Name FROM information_schema.plugins WHERE Plugin_Name LIKE 'group_replication'", Plugin_Name, 1)
if ($value == No such row)
{
  --replace_result $GROUP_REPLICATION GROUP_REPLICATION
  --eval INSTALL PLUGIN group_replication SONAME '$GROUP_REPLICATION'
  --let $install_clean_up_required= 1
}

--enable_warnings
--enable_query_log
--source include/end_include_file.inc
