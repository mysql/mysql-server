# ==== Purpose ====
#
# Clear variables that were persisted with SET PERSIST[_ONLY]
# For now it only resets single primary variables
#  - group_replication_single_primary_mode
#  - group_replication_enforce_update_everywhere_checks
#
# ==== Usage ====
#
# --source include/gr_clear_persistent_variables.inc
#

--let $include_filename= gr_clear_persistent_variables.inc
--source include/begin_include_file.inc

# Save current connection.
--let $_old_connection= $CURRENT_CONNECTION

# Clean persisted primary mode options on all servers to avoid contamination
# to other testcases.
--let $_rpl_server_max= 10
--let $_rpl_server= 1
while ($_rpl_server <= $_rpl_server_max)
{
  --let $_rpl_server_port= \$SERVER_MYPORT_$_rpl_server
  if (!$_rpl_server_port)
  {
    # Break loop.
    --let $_rpl_server= $_rpl_server_max
  }

  if ($_rpl_server_port)
  {
    --let $rpl_connection_name= server_aux_$_rpl_server
    --let $rpl_server_number= $_rpl_server
    --source include/rpl/connect.inc
    --let $rpl_connection_name= server_aux_$_rpl_server
    --source include/connection.inc

    --disable_query_log
    # variables might not be there for some connections
    --disable_warnings
    RESET PERSIST IF EXISTS group_replication_single_primary_mode;
    RESET PERSIST IF EXISTS group_replication_enforce_update_everywhere_checks;
    --enable_warnings
    --enable_query_log

    --connection default
    --disconnect server_aux_$_rpl_server
  }

  --inc $_rpl_server
}

# Revert to old connection.
--connection $_old_connection

--let $include_filename= gr_clear_persistent_variables.inc
--source include/end_include_file.inc
