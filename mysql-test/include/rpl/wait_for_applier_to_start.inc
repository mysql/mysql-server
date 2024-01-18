# ==== Purpose ====
#
# Waits the SQL thread of the current connection has started, or until
# a timeout is reached.
#
# ==== Usage ====
#
# [--let $rpl_channel_name= 'NAME']
# [--let $slave_timeout= NUMBER]
# [--let $rpl_debug= 1]
# --source include/rpl/wait_for_applier_to_start.inc
#
# Parameters:
#   $rpl_channel_name
#     See include/rpl/wait_for_replica_status.inc
#
#   $slave_timeout
#     See include/rpl/wait_for_replica_status.inc
#
#   $rpl_debug
#     See include/rpl/init.inc


--let $include_filename= rpl/wait_for_applier_to_start.inc
if ($rpl_channel_name)
{
  --let $include_filename= $include_filename [FOR CHANNEL $rpl_channel_name]
}
--source include/begin_include_file.inc


let $slave_param= Replica_SQL_Running;
let $slave_param_value= Yes;

# Unfortunately, the slave sql thread sets Replica_SQL_Running=Yes
# *before* it clears Last_SQL_Errno.  So we have to allow errors in
# the SQL thread here.

--let $slave_error_param=
source include/rpl/wait_for_replica_status.inc;


--let $include_filename= rpl/wait_for_applier_to_start.inc
--source include/end_include_file.inc
