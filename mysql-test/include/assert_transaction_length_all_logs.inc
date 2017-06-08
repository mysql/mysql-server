# ==== Purpose ====
#
# Assert that all transactions in all server's binary and relay logs
# have the correct transaction length information.
#
# ==== Usage ====
#
# [--let $skip_output= 1]
# --source include/assert_transaction_length_all_logs.inc
#
# Parameters:
#
# $skip_output
#      When this option is set, the include will not display the results
#      with the summary per good log file evaluation.
#

--let $_atlal_data_dir= `SELECT @@datadir`

#
# For each binlog file
#

# Get last binary log file
--let $_atlal_current_binlog_file= query_get_value(SHOW MASTER STATUS, File, 1)
--let $_atlal_binlog_prefix=`SELECT CONCAT(SUBSTRING_INDEX('$_atlal_current_binlog_file', '.', 1), '.')`
--let $_atlal_binlog_number=`SELECT CAST(SUBSTRING_INDEX('$_atlal_current_binlog_file', '.', -1) AS UNSIGNED)`
# Iterate backwards for each existing binary log file
while ($_atlal_binlog_number)
{
  --let $binlog_file= `SELECT CONCAT('$_atlal_binlog_prefix', LPAD($_atlal_binlog_number, 6, '0'))`
  --let $is_relay_log= 0

  # check if the file exists
  --disable_query_log
  --disable_result_log
  --error 0, ER_ERROR_WHEN_EXECUTING_COMMAND
  --eval SHOW BINLOG EVENTS IN '$binlog_file' LIMIT 1
  --let $_atlal_error= query_get_value(SHOW ERRORS, Code, 1)
  if ($_atlal_error == 1220)
  {
    --let $_atlal_binlog_number= 0
  }
  --enable_result_log
  --enable_query_log

  if ($_atlal_binlog_number)
  {
    --source include/assert_transaction_length.inc
    --dec $_atlal_binlog_number
  }
}

--let $_atlal_slave_prefix= `SELECT @@relay_log`
#
# For each channel
#
--let $_atlal_channel_counter=`SELECT COUNT(*) FROM performance_schema.replication_connection_configuration`
while ($_atlal_channel_counter)
{
  # Get channel name
  --let $_atlal_channel_name=query_get_value(SELECT CHANNEL_NAME FROM performance_schema.replication_connection_configuration, CHANNEL_NAME, $_atlal_channel_counter)
  # Check if it is a GR channel
  --error 0, ER_SLAVE_CHANNEL_OPERATION_NOT_ALLOWED
  --let $_atlal_current_relaylog_file= query_get_value(SHOW SLAVE STATUS FOR CHANNEL '$_atlal_channel_name', Relay_Log_File, 1)
  --let $_atlal_error= query_get_value(SHOW ERRORS, Code, 1)
  # Skip GR channels assertion
  if ($_atlal_error == 3139)
  {
    --let $_atlal_relaylog_number= 0
  }
  # Get the last relay log file applied
  if ($_atlal_error != 3139)
  {
    --let $_atlal_relaylog_prefix=`SELECT CONCAT(SUBSTRING_INDEX('$_atlal_current_relaylog_file', '.', 1), '.')`
    --let $_atlal_relaylog_number=`SELECT CAST(SUBSTRING_INDEX('$_atlal_current_relaylog_file', '.', -1) AS UNSIGNED)`
    --let $_atlal_current_relaylog_pos= query_get_value(SHOW SLAVE STATUS FOR CHANNEL '$_atlal_channel_name', Relay_Log_Pos, 1)
    # This channel was just RESET and probably has no relay log file yet.
    if ($_atlal_relaylog_number == 2)
    {
      if ($_atlal_current_relaylog_pos == 4)
      {
        --let $_atlal_relaylog_number= 0
      }
    }
  }
  # Iterate backwards for each existing relay log file
  while ($_atlal_relaylog_number)
  {
    --let $binlog_file= `SELECT CONCAT('$_atlal_relaylog_prefix', LPAD($_atlal_relaylog_number, 6, '0'))`
    --let $is_relay_log= 1

    # check if the file exists
    --disable_query_log
    --disable_result_log
    --error 0, ER_ERROR_WHEN_EXECUTING_COMMAND
    --eval SHOW RELAYLOG EVENTS IN '$binlog_file' LIMIT 1 FOR CHANNEL '$_atlal_channel_name'
    --let $_atlal_error= query_get_value(SHOW ERRORS, Code, 1)
    if ($_atlal_error == 1220)
    {
      --let $_atlal_relaylog_number= 0
    }
    --enable_result_log
    --enable_query_log

    if ($_atlal_relaylog_number)
    {
      --source include/assert_transaction_length.inc
      --dec $_atlal_relaylog_number
    }
  }

  --dec $_atlal_channel_counter
}
#--die

