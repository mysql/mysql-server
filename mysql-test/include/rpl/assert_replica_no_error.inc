# ==== Purpose ====
#
# Assert that Slave_SQL_Errno = Slave_IO_Errno = 0 in the output from
# SHOW REPLICA STATUS.
#
#
# ==== Usage ====
#
# [--let $rpl_channel_name= 'NAME']
# [--let $rpl_debug= 1]
# --source include/rpl/assert_replica_no_error.inc
#
# Parameters:
#   $rpl_channel_name
#     See include/rpl/assert_replica_status.inc
#
#   $rpl_debug
#     See include/rpl/init.inc


--let $include_filename= rpl/assert_replica_no_error.inc
if ($rpl_channel_name)
{
  --let $include_filename= $include_filename [FOR CHANNEL $rpl_channel_name]
}
--source include/begin_include_file.inc


--let $slave_param= Last_SQL_Errno
--let $slave_param_value= 0
--source include/rpl/assert_replica_status.inc

--let $slave_param= Last_IO_Errno
--let $slave_param_value= 0
--source include/rpl/assert_replica_status.inc


--let $include_filename= rpl/assert_replica_no_error.inc
--source include/end_include_file.inc
