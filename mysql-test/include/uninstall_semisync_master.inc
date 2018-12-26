# Uninstall semisync master plugin
# Usage:
# --source include/uninstall_semisync_master.inc

--let $include_filename= uninstall_semisync_master.inc
--source include/begin_include_file.inc

--disable_query_log
--disable_warnings

# wait till semisync clients are '0' before uninstalling
# semisync master plugin.
--let $status_var= Rpl_semi_sync_master_clients
--let $status_var_value= 0
--source include/wait_for_status_var.inc

UNINSTALL PLUGIN rpl_semi_sync_master;

--source include/end_include_file.inc
