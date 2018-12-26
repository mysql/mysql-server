# Setup semisync master
# Usage:
# [--let $semisync_master_enabled = [ON|OFF]]
# --source include/install_semisync_master.inc
#
# $semisync_master_enabled is optional. semisync_master will be enabled if
# $semisync_master_enabled is not set.

--let $include_filename= install_semisync_master.inc
--source include/begin_include_file.inc

--source include/have_semisync_plugin.inc

--disable_query_log

--let $value = query_get_value(show variables like 'rpl_semi_sync_master_enabled', Value, 1)
if ($value == No such row)
{
  eval INSTALL PLUGIN rpl_semi_sync_master SONAME '$SEMISYNC_MASTER_PLUGIN';
  if ($semisync_master_enabled == '')
  {
    --let $semisync_master_enabled= ON
  }
  eval SET GLOBAL rpl_semi_sync_master_enabled = $semisync_master_enabled;
}

SET @save_sql_log_bin= @@sql_log_bin;
SET @@sql_log_bin = 0;
CALL mtr.add_suppression(".*Got an error reading communication packets.*");
SET @@sql_log_bin = @save_sql_log_bin;
--source include/end_include_file.inc
