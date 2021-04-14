# Setup semisync master
# Usage:
# [--let $semisync_source_enabled = [ON|OFF]]
# --source include/install_semisync_source.inc
#
# $semisync_source_enabled is optional. semisync_source will be enabled if
# $semisync_source_enabled is not set.

--let $include_filename= install_semisync_source.inc
--source include/begin_include_file.inc

--source include/have_semisync_plugin.inc

--disable_query_log

--let $value = query_get_value(show variables like 'rpl_semi_sync_source_enabled', Value, 1)
if ($value == No such row)
{
  eval INSTALL PLUGIN rpl_semi_sync_source SONAME '$SEMISYNC_SOURCE_PLUGIN';
  if ($semisync_source_enabled == '')
  {
    --let $semisync_source_enabled= ON
  }
  eval SET GLOBAL rpl_semi_sync_source_enabled = $semisync_source_enabled;
}

SET @save_sql_log_bin= @@sql_log_bin;
SET @@sql_log_bin = 0;
CALL mtr.add_suppression(".*Got an error reading communication packets.*");
# Suppress additional errors on MacOS
CALL mtr.add_suppression(".*The client was disconnected by the server.*");
SET @@sql_log_bin = @save_sql_log_bin;
--source include/end_include_file.inc
