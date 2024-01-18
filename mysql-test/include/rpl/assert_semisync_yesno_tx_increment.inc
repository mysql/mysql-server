# ==== Purpose ====
#
# Assert rpl_semi_sync_source_yes_tx and rpl_semi_sync_source_no_tx increase
# a certain value
#
# ==== Usage ====
# --source include/rpl/save_semisync_yesno_tx.inc
# INSERT INTO t1 VALUES(1);
# --let semi_sync_yes_tx_increment= 1
# --let semi_sync_no_tx_increment= 0
# --source include/rpl/assert_semisync_yesno_tx_increment.inc

if ($semi_sync_yes_tx_increment == '')
{
  --let $semi_sync_yes_tx_increment= 0
}

if ($semi_sync_no_tx_increment == '')
{
  --let $semi_sync_no_tx_increment= 0
}

if ($rpl_semi_sync_source_yes_tx == '')
{
  --die ERROR IN TEST:  the mysqltest variable rpl_semi_sync_source_yes_tx must be set
}

if ($rpl_semi_sync_source_no_tx == '')
{
  --die ERROR IN TEST:  the mysqltest variable rpl_semi_sync_source_no_tx must be set
}

--let $assert_status_name= rpl_semi_sync_source_yes_tx
--let $assert_status_value= $rpl_semi_sync_source_yes_tx + $semi_sync_yes_tx_increment
--source include/assert_status.inc

--let $assert_status_name= rpl_semi_sync_source_no_tx
--let $assert_status_value= $rpl_semi_sync_source_no_tx + $semi_sync_no_tx_increment
--source include/assert_status.inc

--let $semi_sync_yes_tx_increment=
--let $semi_sync_no_tx_increment=
--let $rpl_semi_sync_source_yes_tx=
--let $rpl_semi_sync_source_no_tx=
