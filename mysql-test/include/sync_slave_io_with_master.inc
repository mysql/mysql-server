# ==== Purpose ====
#
# Waits until the slave IO thread has been synced, i.e., all events
# have been copied over to slave.  Does not care if the SQL thread is
# in sync (or even running).
#
#
# ==== Usage ====
#
# [--let $sync_slave_connection= <connection_name>]
# [--let $slave_timeout= NUMBER]
# [--let $rpl_debug= 1]
# --source include/sync_slave_io_with_master.inc
#
# Syncs slave to the current position on master, as found by SHOW
# MASTER STATUS.
#
# Must be called on the master. Will change connection to the slave.
#
# Parameters:
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

--let $include_filename= sync_slave_io_with_master.inc
--source include/begin_include_file.inc


--source include/save_master_pos.inc

--let $rpl_connection_name= slave
if (`SELECT '$sync_slave_connection' != ''`)
{
  --let $rpl_connection_name= $sync_slave_connection
}
--source include/rpl_connection.inc

--source include/sync_io_with_master.inc


--let $include_filename= sync_slave_io_with_master.inc
--let $skip_restore_connection= 1
--source include/end_include_file.inc
