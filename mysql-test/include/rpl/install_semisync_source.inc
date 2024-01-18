# Setup semisync master
# Usage:
# [--let $semisync_source_enabled = [ON|OFF]]
# [--let $use_old_semisync_plugin_names = 1]
# --source include/rpl/install_semisync_source.inc
#
# Parameters:
#
#   $semisync_source_enabled
#     By default, or if this parameter is set to ON or 1, this script
#     will enable the plugin by setting rpl_semi_sync_source_enabled =
#     ON.  If set to OFF or 0, does not enable the plugin.
#
#   $use_old_semisync_plugin_names
#     By default uses the new-named plugin, semisync_source.  If this is
#     set to 1, uses the old-named plugin, semisync_master.

--let $include_filename= rpl/install_semisync_source.inc
--source include/begin_include_file.inc

--source include/have_semisync_plugin.inc

--disable_query_log

--let $_iss_enabled = _enabled

--let $_iss_source = source
--let $_iss_library = $SEMISYNC_SOURCE_PLUGIN
if ($use_old_semisync_plugin_names) {
  --let $_iss_source = master
  --let $_iss_library = $SEMISYNC_MASTER_PLUGIN
}

--let $value = query_get_value(show variables like 'rpl_semi_sync_$_iss_source$_iss_enabled', Value, 1)
if ($value == No such row)
{
  eval INSTALL PLUGIN rpl_semi_sync_$_iss_source SONAME '$_iss_library';
  if ($semisync_source_enabled == '')
  {
    --let $semisync_source_enabled= ON
  }
  eval SET GLOBAL rpl_semi_sync_$_iss_source$_iss_enabled = $semisync_source_enabled;
}

SET @save_sql_log_bin= @@sql_log_bin;
SET @@sql_log_bin = 0;
CALL mtr.add_suppression(".*Got an error reading communication packets.*");
# Suppress additional errors on MacOS
CALL mtr.add_suppression(".*The client was disconnected by the server.*");
SET @@sql_log_bin = @save_sql_log_bin;
--source include/end_include_file.inc
