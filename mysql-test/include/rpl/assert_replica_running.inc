# ==== Purpose ====
#
# Assert that the slave threads are running and don't have any errors.
#
#
# ==== Usage ====
#
# [--let $rpl_channel_name= 'NAME']
# [--let $rpl_debug= 1]
# --source include/rpl/assert_replica_running.inc
#
# Parameters:
#   $rpl_channel_name
#     See include/rpl/assert_replica_status.inc
#
#   $rpl_debug
#     See include/rpl/init.inc


--let $include_filename= rpl/assert_replica_running.inc
if ($rpl_channel_name)
{
  --let $include_filename= $include_filename [FOR CHANNEL $rpl_channel_name]
}
--source include/begin_include_file.inc


--let $slave_param= Replica_IO_Running
--let $slave_param_value= Yes
--source include/rpl/assert_replica_status.inc

--let $slave_param= Replica_SQL_Running
--let $slave_param_value= Yes
--source include/rpl/assert_replica_status.inc


--let $include_filename= rpl/assert_replica_running.inc
--source include/end_include_file.inc
