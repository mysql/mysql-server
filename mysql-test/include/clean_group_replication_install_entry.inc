# ==== Purpose ====
#
# This include will remove group_replication entry from mysql.plugin table.
#
# Post INSTALL PLUGIN group_replication, group_replication entry needs
# to be cleaned from mysql.plugin table else it can cause warnings in error
# log file. Reason of warning: Group replication is loaded by default
# --plugin-load option but INSTALL PLUGIN command also adds an entry in
# mysql.plugin table for auto-load on reboot. Auto-load conflicts with
# Group replication plugin-load option on boot causing a double install
# attempt, so post INSTALL cleanup is needed.
#
# ==== Usage ====
#
# --source include/clean_group_replication_install_entry.inc
#
--disable_query_log
--disable_warnings

--let $_install_rpl_server= $rpl_server_count
while ($_install_rpl_server)
{
#  --let $rpl_connection_name= server_$_install_rpl_server
#  --source include/rpl_connection.inc
  --connection server_$_install_rpl_server
  --let $_install_tmp= query_get_value(SELECT COUNT(*) AS count FROM mysql.plugin WHERE name="group_replication", count, 1)
  if ($_install_tmp)
  {
    SET SESSION sql_log_bin= 0;
    DELETE FROM mysql.plugin WHERE name='group_replication';
    SET SESSION sql_log_bin= 1;
  }
  --dec $_install_rpl_server
}

--enable_warnings
--enable_query_log
--let $install_clean_up_required= 0
