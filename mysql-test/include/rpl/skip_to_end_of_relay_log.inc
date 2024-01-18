# ==== Purpose ====
#
# Execute CHANGE REPLICATION SOURCE to make the applier threads skip to the end of
# the relay log.
#
# WARNING! After using this file with auto_position=0, the applier
# thread's source_log_pos is not up to date. This can cause
# SOURCE_POS_WAIT to hang. The applier thread's source_log_pos is only
# updated when the applier thread executes events, not when CHANGE
# MASTER is used to fast-forward the position as this script does.
#
# When used in channels with SOURCE_AUTO_POSITION = 1, this include
# uses GTID_FROM_GTID_SET() which exists in include/rpl/gtid_utils.inc.
#
# ==== Usage ====
#
# [--let $rpl_channel_name= 'NAME']
# --source include/rpl/skip_to_end_of_relay_log.inc
#
# Parameters:
#   $rpl_channel_name
#     By default, uses the empty channel (channel name ''). Set this
#     if you want to operate on any other channel.

--let $include_filename= include/rpl/skip_to_end_of_relay_log.inc
--let $_for_channel_clause= FOR CHANNEL ''
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

--source include/rpl/save_relay_log_file_position.inc

--let $_rsteorl_is_auto_position= query_get_value(SHOW REPLICA STATUS, Auto_Position, 1)

if ($_rsteorl_is_auto_position)
{
  if (!`SELECT COUNT(*) FROM INFORMATION_SCHEMA.ROUTINES WHERE specific_name = 'GTID_FROM_GTID_SET' AND routine_schema = 'test'`)
  {
    --die include/rpl/skip_to_end_of_relay_log.inc uses GTID_FROM_GTID_SET() which exists in include/rpl/gtid_utils.inc. Please source it or use "$rpl_gtid_utils= 1" before sourcing include/rpl/init_source_replica.inc
  }

  --let $_rsteorl_received_gtid_set= query_get_value(SHOW REPLICA STATUS, Retrieved_Gtid_Set, 1)
  # Find transactions in relay log which are not in gtid_executed.
  --let $_rsteorl_skip_set= `SELECT GTID_SUBTRACT('$_rsteorl_received_gtid_set', @@GLOBAL.GTID_EXECUTED)`
  # Generate empty transactions.
  while ($_rsteorl_skip_set != '')
  {
    --let $_rsteorl_skip_gtid= `SELECT GTID_FROM_GTID_SET('$_rsteorl_skip_set')`
    eval SET GTID_NEXT = '$_rsteorl_skip_gtid';
    COMMIT;
    SET GTID_NEXT = 'AUTOMATIC';
    --let $_rsteorl_skip_set= `SELECT GTID_SUBTRACT('$_rsteorl_skip_set', '$_rsteorl_skip_gtid')`
  }
}

if (!$_rsteorl_is_auto_position)
{
  eval CHANGE REPLICATION SOURCE TO RELAY_LOG_FILE = '$relay_log_file',
                        RELAY_LOG_POS = $relay_log_size
                        $_for_channel_clause;
}

--source include/rpl/start_applier.inc

--let $include_filename= include/rpl/skip_to_end_of_relay_log.inc
--source include/end_include_file.inc
