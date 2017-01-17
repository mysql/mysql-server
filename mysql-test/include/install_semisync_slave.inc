# Setup semisync master
# Usage:
# --source include/install_semisync_slave.inc

--let $include_filename= install_semisync_slave.inc
--source include/begin_include_file.inc

--source include/not_embedded.inc
--source include/have_semisync_plugin.inc

--disable_query_log

--source include/stop_slave_io.inc

--let $value= query_get_value(show variables like 'rpl_semi_sync_slave_enabled', Value, 1)
if ($value == No such row)
{
  eval INSTALL PLUGIN rpl_semi_sync_slave SONAME '$SEMISYNC_SLAVE_PLUGIN';
  SET GLOBAL rpl_semi_sync_slave_enabled = 1;
}

--source include/start_slave_io.inc

--source include/end_include_file.inc
