################################################################################
# This file aims to check if a log, either binary or relay log, has correctly
# stored the Previous_gtids_log_event and Gtid_log_event events.
#
# Options:
# (Type of the log: binlog or relaylog)
# --let $binlog= <type_log>
# (Check in the current log has <set_ini>-<set_end> in Previous GTIDs)
# $gtid_set_ini= <set_ini>
# $gtid_set_end= <set_end>
# (Check in the current log has GTIDs from <gtid_ini> to <gtid_end>)
# $gtid_ini= <gtid_ini>
# $gtid_end= <gtid_end>
# (Enable or disable debug)
# --let $debug= <enabled>
#
# Usage:
# --let $binlog= <type_log>
# --let $gtid_set_ini= <set_ini>
# --let $gtid_set_end= <set_end>
# --let $gtid_ini= <gtid_ini>
# --let $gtid_end= <gtid_end>
# --source extra/rpl_tests/rpl_check_gtid.inc
#
# Example (Checking both Previous GTIDs and current Gtids in the master's
#          current binary log):
# --connection master
# --let $binlog= binlog
# --let $gtid_set_ini= <set_ini>
# --let $gtid_set_end= <set_end>
# --let $gtid_ini= <gtid_ini>
# --let $gtid_end= <gtid_end>
# --source extra/rpl_tests/rpl_check_gtid.inc
#
# Example (Checking Previous GTIDs in the slave's current binary log):
# --connection slave
# --let $binlog= binlog
# --let $gtid_set_ini= <set_ini>
# --let $gtid_set_end= <set_end>
# --let $gtid_ini=
# --let $gtid_end=
# --source extra/rpl_tests/rpl_check_gtid.inc
#
# Example (Checking GTIDs in the slave's current relay log):
# --connection slave
# --let $binlog= relaylog
# --let $gtid_set_ini=
# --let $gtid_set_end=
# --let $gtid_ini= <gtid_ini>
# --let $gtid_end= <gtid_end>
# --source extra/rpl_tests/rpl_check_gtid.inc
################################################################################

--echo extra/rpl_tests/rpl_check_gtid.inc

if (`SELECT "$binlog" = "binlog"`)
{
  --let $command_set= SHOW MASTER STATUS
  --let $command_set_info= File
  --let $command_show= SHOW BINLOG EVENTS
}

if (`SELECT "$binlog" = "relaylog"`)
{
  --let $command_set= SHOW SLAVE STATUS
  --let $command_set_info= Relay_Log_File
  --let $command_show= SHOW RELAYLOG EVENTS
}


if (`SELECT "$binlog" <> "" and "$gtid_set_end" <> "" and "$gtid_set_ini" <> ""`)
{
  --let $server_log= query_get_value($command_set, $command_set_info, 1)
  if (`SELECT "$debug" <> ""`)
  {
    --eval $command_show in "$server_log"
  }
  --let $type= query_get_value($command_show in "$server_log", Event_type, 2)
  --let $info= query_get_value($command_show in "$server_log", Info, 2)
  if ($type != Previous_gtids)
  {
    --source include/show_rpl_debug_info.inc
    --echo $type $info --- Type should be Previous_gtids.
    --echo log='$server_log'
    --eval $command_show in "$server_log"
    --die
  }
  if ($gtid_set_end == 0 && $gtid_set_ini == 0)
  {
    if (`SELECT "$info" <> ""`)
    {
      --source include/show_rpl_debug_info.inc
      --echo $type $info -- Info should be empty.
      --let $datadir= `select @@datadir`
      --echo log='$server_log'
      --exec $MYSQL_BINLOG $datadir/$server_log
      --eval $command_show in "$server_log"
      --die
    }
  }
  if (`SELECT $gtid_set_end <> 0 and $gtid_set_ini <> 0 and $gtid_set_end <> $gtid_set_ini`)
  {
    if (`SELECT NOT ("$info" REGEXP "$server_uuid:$gtid_set_ini-$gtid_set_end")`)
    {
      --source include/show_rpl_debug_info.inc
      --echo $type $info -- Info should be $server_uuid:$gtid_set_ini-$gtid_set_end.
      --let $datadir= `select @@datadir`
      --echo log='$server_log'
      --exec $MYSQL_BINLOG $datadir/$server_log
      --eval $command_show in "$server_log"
      --die
    }
  }
  if ($gtid_set_end != 0 && $gtid_set_ini != 0 && $gtid_set_end == $gtid_set_ini)
  {
    if (`SELECT NOT ("$info" REGEXP "$server_uuid:$gtid_set_ini")`)
    {
      --source include/show_rpl_debug_info.inc
      --echo $type $info -- Info should be $server_uuid:$gtid_set_ini.
      --let $datadir= `select @@datadir`
      --echo log='$server_log'
      --exec $MYSQL_BINLOG $datadir/$server_log
      --eval $command_show in "$server_log"
      --die
    }
  }
}

if (`SELECT "$binlog" <> "" and "$gtid_end" <> "" and "$gtid_ini" <> ""`)
{
  --let $type= unknown
  --let $trans= `SELECT $gtid_ini`
  --let $pos= 1
  --let $info= unknown
  --let $server_log= query_get_value($command_set, $command_set_info, 1)
  while (`SELECT $trans <= $gtid_end AND "$type" <> "No such row"`)
  {
    --let $type= query_get_value($command_show in "$server_log", Event_type, $pos)
    if ($type == Gtid)
    {
      --let $info= query_get_value($command_show in "$server_log", Info, $pos)
      if (`SELECT "$info" REGEXP "^SET @@SESSION.GTID_NEXT= '$server_uuid:$trans'"`)
      {
        --let $trans= `SELECT $trans + 1`
      }
    }
    --inc $pos
  }
  if ($trans <=  $gtid_end)
  {
    --source include/show_rpl_debug_info.inc
    --echo $type $info -- We should retrieve from $gtid_ini to $gtid_end.
    --echo log='$server_log'
    --eval $command_show in "$server_log"
    --die
  }
}
