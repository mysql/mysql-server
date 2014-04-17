# ==== Purpose ====
#
# Waits until the slave SQL thread has been synced, i.e., all events
# have been copied over to slave.  This is like mtr's built-in command
# sync_slave_with_master, but more flexible (e.g., you can set a
# custom timeout and you can force it to use GTIDs instead of filename
# and offset).
#
#
# ==== Usage ====
#
# [--let $sync_slave_connection= <connection_name>]
# [--let $use_gtids= 1]
# [--let $slave_timeout= NUMBER]
# [--let $rpl_debug= 1]
# --source include/sync_slave_io_with_master.inc
#
# Must be called on the master. Will change connection to the slave.
#
# Parameters:
#
#   $use_gtids
#     If set, uses GTIDs instead of filename and offset for positions.
#
#   $sync_slave_connection
#     By default, this script switches connection to 'slave'. If
#     $sync_slave_connection is set, then '$sync_slave_connection' is
#     used instead of 'slave'.
#
#   $slave_timeout
#      See include/wait_for_slave_param.inc.
#
#   $rpl_debug
#      See include/rpl_init.inc


--let $include_filename= sync_slave_sql_with_master.inc
--source include/begin_include_file.inc

--source include/wait_for_ndb_committed_to_binlog.inc
--source include/save_master_pos.inc

--let $rpl_connection_name= slave
if ($sync_slave_connection)
{
  --let $rpl_connection_name= $sync_slave_connection
}
--source include/rpl_connection.inc

--source include/sync_slave_sql.inc

--let $include_filename= sync_slave_sql_with_master.inc
--let $skip_restore_connection= 1
--source include/end_include_file.inc
