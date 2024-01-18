# ==== Purpose ====
#
# Issues START REPLICA SQL_THREAD on the current connection. Then waits
# until the SQL thread has started, or until a timeout is reached.
#
# Please use this instead of 'START RELICA SQL_THREAD', to reduce the
# risk of races in test cases.
#
#
# ==== Usage ====
#
# [--let $rpl_channel_name= 'NAME']
# [--let $slave_timeout= NUMBER]
# [--let $rpl_debug= 1]
# --source include/rpl/start_applier.inc
#
# Parameters:
#   $rpl_channel_name
#     If set, a FOR CHANNEL $rpl_channel_name is added.
#
#   $slave_timeout
#     See include/rpl/wait_for_replica_status.inc
#
#   $rpl_debug
#     See include/rpl/init.inc


--let $include_filename= rpl/start_applier.inc
--let $_for_channel_clause=
if ($rpl_channel_name)
{
  --let $_for_channel_clause= FOR CHANNEL $rpl_channel_name
  --let $include_filename= $include_filename [FOR CHANNEL $rpl_channel_name]
}
--source include/begin_include_file.inc


if (!$rpl_debug)
{
  --disable_query_log
}


--eval START REPLICA SQL_THREAD $_for_channel_clause

--source include/rpl/wait_for_applier_to_start.inc


--let $include_filename= rpl/start_applier.inc
--source include/end_include_file.inc
