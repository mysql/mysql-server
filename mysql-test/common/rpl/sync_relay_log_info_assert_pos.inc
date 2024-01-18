#
# This include is a sub routine of common/rpl/sync_relay_log_info.inc
#

# Save current master position
--source include/rpl/connection_source.inc
--let $master_file= query_get_value(SHOW BINARY LOG STATUS, File, 1)
--let $master_pos= query_get_value(SHOW BINARY LOG STATUS, Position, 1)

# Inc the sub test counter
--inc $_rrli_counter

# Add sub test counter to the header
--let $_rrli_subheader= $_rrli_header, step=$_rrli_counter

# Get the expected status of the SQL thread positions
--let $_rrli_expected= `SELECT SUBSTR("$rpl_relay_log_info_expected_update", $_rrli_counter, 1)`

if ($_rrli_expected != "N")
{
  if ($_rrli_expected != "U")
  {
    --echo ERROR IN TEST: rpl_relay_log_info_expected_update parameter: U or N is expected at position $_rrli_counter
    --die Please fix your test case!
  }
}
--source include/rpl/connection_replica.inc

# Ensure that the SQL thread is waiting for more events if started
--let $is_replica_sql_running= query_get_value(SHOW REPLICA STATUS $_for_channel, Replica_SQL_Running, 1)
if ($is_replica_sql_running == 'Yes' )
{
  --let $slave_param= Replica_SQL_Running_State
  --let $slave_param_value= Replica has read all relay log; waiting for more updates
  --source include/rpl/wait_for_replica_status.inc
}

# If it was expected to not be updated
if ( $_rrli_expected == "N" )
{
  --let $assert_text= ($_rrli_subheader) SQL thread did not synced master binlog file
  --let $assert_cond= [ SELECT Master_log_name = "$last_synced_master_file" FROM mysql.slave_relay_log_info WHERE Channel_name = "" ]
  --source include/assert.inc

  --let $assert_text= ($_rrli_subheader) SQL thread did not synced master binlog position
  --let $assert_cond= [ SELECT Master_log_pos = "$last_synced_master_pos" FROM mysql.slave_relay_log_info WHERE Channel_name = "" ]
  --source include/assert.inc
}

# If it was expected to be updated
if ( $_rrli_expected == "U" )
{
  --echo Wait until ($_rrli_subheader) SQL thread synced master binlog file
  --let $wait_condition= SELECT Master_log_name = "$master_file" FROM mysql.slave_relay_log_info WHERE Channel_name = ""
  --source include/wait_condition.inc
  --let $last_synced_master_file= $master_file

  --echo Wait until ($_rrli_subheader) SQL thread synced master binlog position
  --let $wait_condition= SELECT Master_log_pos = "$master_pos" FROM mysql.slave_relay_log_info WHERE Channel_name = ""
  --source include/wait_condition.inc
  --let $last_synced_master_pos= $master_pos
}

--source include/rpl/gtid_step_assert.inc
