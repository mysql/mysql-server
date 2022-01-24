# Setup semisync replica
#
# ==== Usage ====
#
# [--let $use_old_semisync_plugin_names = 1]
# --source include/install_semisync_replica.inc
#
# Parameters:
#
#   $use_old_semisync_plugin_names
#     By default uses the new-named plugin, semisync_replica.  If this is
#     set to 1, uses the old-named plugin, semisync_slave.

--let $include_filename= install_semisync_replica.inc
--source include/begin_include_file.inc

--source include/have_semisync_plugin.inc

--disable_query_log

--let $_isr_enabled = _enabled

--let $_isr_replica = replica
--let $_isr_library = $SEMISYNC_REPLICA_PLUGIN
if ($use_old_semisync_plugin_names) {
  --let $_isr_replica = slave
  --let $_isr_library = $SEMISYNC_SLAVE_PLUGIN
}

--source include/stop_slave_io.inc

--let $value= query_get_value(show variables like 'rpl_semi_sync_$_isr_replica$enabled', Value, 1)
if ($value == No such row)
{
  eval INSTALL PLUGIN rpl_semi_sync_$_isr_replica SONAME '$_isr_library';
  eval SET GLOBAL rpl_semi_sync_$_isr_replica$_isr_enabled = 1;
}

--source include/start_slave_io.inc

--source include/end_include_file.inc
