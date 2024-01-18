# ==== Purpose ====
#
# Reverts the changes done by rpl/priv_checks_user_configuration.inc
# This file is normally sourced by rpl/deinit.inc for replication use cases
# or gr_clear_configuration.inc for group replication use cases.
#
# ==== Usage ====
# [--let $rpl_change_topology_user_created= 1:rpl_applier_priv_user'@'localhost
#    2:rpl_applier_priv_user'@'localhost]
#  --source include/rpl/clear_priv_checks_user_configuration.inc
#
# Parameters:
#   $rpl_change_topology_user_created
#     Specifies the user account created for each server in format :
#     server_number1:user1, server_number2:user2 ...
#


--let $include_filename= rpl/clear_priv_checks_user_configuration.inc
--source include/begin_include_file.inc

--let $_sql_log_bin_saved= `SELECT @@sql_log_bin`
--let $_read_only_saved= `SELECT @@global.read_only`
--let $_super_read_only_saved = `SELECT @@global.read_only`
--disable_query_log
SET @@sql_log_bin = 0;

# Ensure that rpl servers are writable when reverting the changes
# required to configure privilege_checks_user
SET @@global.read_only = 0;
SET @@global.super_read_only = 0;

# Drop applier user, if created
--let $_rpl_change_topology_user_created_index= `SELECT LOCATE('$_rpl_server:', "$rpl_change_topology_user_created")`
if ($_rpl_change_topology_user_created_index)
{
  --let $_rpl_change_topology_user_created= `SELECT SUBSTRING_INDEX(SUBSTRING_INDEX(SUBSTRING("$rpl_change_topology_user_created", $_rpl_change_topology_user_created_index), ' ', 1), ':', -1)`
  --let $_rpl_change_topology_user_name= `SELECT SUBSTRING("$_rpl_change_topology_user_created", 1, $_privilege_checks_user_at_index - 1)`
  --let $_rpl_change_topology_user_host= '%'
  if ($_privilege_checks_user_at_index != 0)
  {
    --let $_rpl_change_topology_user_host= `SELECT SUBSTRING("$_rpl_change_topology_user_created", $_privilege_checks_user_at_index + 1)`
  }

  # Revert changes done to mysql.db
  # --eval INSERT INTO mysql.db VALUES ('','test','%','Y','Y','Y','Y','Y','Y','N','Y','Y','Y','Y','Y','Y','Y','Y','N','N','Y','Y')
  UPDATE mysql.db SET
      Select_priv = 'Y',
      Insert_priv = 'Y',
      Update_priv = 'Y',
      Delete_priv = 'Y',
      Create_priv = 'Y',
      Drop_priv = 'Y',
      Grant_priv = 'N',
      References_priv = 'Y',
      Index_priv = 'Y',
      Alter_priv = 'Y',
      Create_tmp_table_priv = 'Y',
      Lock_tables_priv = 'Y',
      Create_view_priv = 'Y',
      Show_view_priv = 'Y',
      Create_routine_priv = 'Y',
      Alter_routine_priv = 'N',
      Execute_priv = 'N',
      Event_priv = 'Y',
      Trigger_priv = 'Y'
    WHERE db = 'test' and user = '';
  FLUSH PRIVILEGES;

  --eval DROP USER $_rpl_change_topology_user_created
   if ($rpl_privilege_checks_use_roles == 1)
   {
     --eval DROP ROLE 'sql_applier_thread'
   }
}

--eval SET @@global.read_only = $_read_only_saved
--eval SET @@global.super_read_only = $_super_read_only_saved
--eval SET @@sql_log_bin = $_sql_log_bin_saved
--enable_query_log

--let $include_filename= rpl/clear_priv_checks_user_configuration.inc
--source include/end_include_file.inc
