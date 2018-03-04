###############################################################################
# Use this .inc to corrupt a channel and test basic commands
# on all channels (good channels and the corrupted channel)
#
# Usage:
# --let $rpl_number_of_servers=4
# --let $rpl_corrupt_channel_name='channel_2'
# --source extra/rpl_tests/rpl_multi_source_corrupt_repository.inc
#
# $rpl_number_of_servers
#    Specify total number of servers in the MSR setup.
#
# $rpl_corrupt_channel_name
#    Specify the channel name which needs to be corrupted.
#
# Note:
#    This .inc assumes that the MSR setup is done
#    as 1->$rpl_number_of_servers, 2->$rpl_number_of_servers,...,
#    $rpl_number_of_servers -1 -> $rpl_number_of_servers.
#    It also assumes that server_$rpl_number_of_servers
#    is the server where we have to create the channels and
#    there are $rpl_number_of_servers -1 of channels created
#    on the server which connects all the remaining servers in the setup.
# Hint:
#    Use extra/rpl_tests/rpl_create_msr_channels.inc if you want
#    create the channels on a MSR slave server.
###############################################################################

if (!$rpl_number_of_servers)
{
  --die "Please specify number of servers in the MSR setup."
}

if (!$rpl_corrupt_channel_name)
{
  --die "Please specify channel name that needs to be corrupted."
}

###############################################################################
# Step-1: Corrupt channel '$rpl_corrupt_channel_name'
#         1.1: Connect to the MSR slave server where we want corrupt the channel
#              and get basic information before shutting down.
#         1.2: Stop the server
#         1.3: Remove the channel's index file (one form of corrupting the
#                                               channel)
#         1.4: Restart the server with --skip-slave-start option.
###############################################################################

--let $number_of_channels= `SELECT $rpl_number_of_servers -1`
--let $number_of_good_channels= `SELECT $rpl_number_of_servers -2`
--let $rpl_msr_server_number=$rpl_number_of_servers

--echo ===== Corrupting Channel $rpl_corrupt_channel_name.

###############################################################################
# Step-1.1: Connect to the MSR slave server where we want corrupt the channel
#           and get basic information before shutting down.
###############################################################################
--let $rpl_connection_name=server_$rpl_msr_server_number
--source include/rpl_connection.inc
--let $slave_datadir= `select @@datadir`
--let $relaylog_name= `select @@relay_log_basename`
--let $rpl_channel_name_without_single_quotes=`SELECT LTRIM(RTRIM($rpl_corrupt_channel_name))`

###############################################################################
# Step-1.2: Stop the server
###############################################################################

--echo ===== Stop the server $rpl_msr_server_number.
--let $rpl_server_number=$rpl_msr_server_number
--source include/rpl_stop_server.inc

###############################################################################
# Step-1.3: Remove the channel's index file (one form of corrupting the
#                                            channel)
###############################################################################

--echo ===== Remove $rpl_corrupt_channel_name channel's relaylog file.
if ($rpl_corrupt_channel_name == "''")
{
  --remove_file $relaylog_name.index
}
if ($rpl_corrupt_channel_name != "''")
{
  --remove_file $relaylog_name-$rpl_channel_name_without_single_quotes.index
}

###############################################################################
# Step-1.4: Restart the server with --skip-slave-start option.
###############################################################################

--echo ===== Start the server $rpl_msr_server_number with --skip-slave-start=0 option.
--let $rpl_server_parameters=--skip-slave-start=0
--source include/rpl_start_server.inc

###############################################################################
# Step-2: After server started with (--skip-slave-start=0) option,
#         check the state of the channels. Only corrupted channel
#         should be in OFF state and remaining channels IO and SQL
#         thread should be in ON state.
###############################################################################

--echo ===== Assert that all channels IO and SQL threads are in expected state.
--let $assert_text= Corrupted channel's IO thread should be in OFF state.
--let $assert_cond= "[SELECT CHANNEL_NAME FROM performance_schema.replication_connection_status WHERE SERVICE_STATE= "OFF"]" = "$rpl_channel_name_without_single_quotes"
--source include/assert.inc

--let $assert_text= Corrupted channel's SQL thread should be in OFF state.
--let $assert_cond= "[SELECT CHANNEL_NAME FROM performance_schema.replication_applier_status WHERE SERVICE_STATE= "OFF"]" = "$rpl_channel_name_without_single_quotes"
--source include/assert.inc

--echo ===== ONLY one channel is corrupted. Waiting until started channels be $number_of_good_channels.
--let $wait_condition= SELECT COUNT(*) = $number_of_good_channels FROM performance_schema.replication_connection_status WHERE SERVICE_STATE="ON"
--source include/wait_condition_or_abort.inc

--echo ===== ONLY one channel is corrupted. Waiting until started channels be $number_of_good_channels.
--let $wait_condition= SELECT COUNT(*) = $number_of_good_channels FROM performance_schema.replication_applier_status WHERE SERVICE_STATE="ON"
--source include/wait_condition_or_abort.inc


###############################################################################
# Step-3: Check basic commands without 'FOR CHANNEL' clause.
###############################################################################

--echo ===== Executing few basic commands without 'FOR CHANNEL' clause.

###############################################################################
# Step-3.1: STOP SLAVE should be able to bring all the channels's IO and SQL
# thread down (corrupted channel's IO and SQL thread down already before this
# step).
###############################################################################

--echo ===== STOP SLAVE without 'FOR CHANNEL' clause.
--let $rpl_channel_name=
--source include/stop_slave.inc

--echo === Assert that all channels IO and SQL threads are in OFF state ===
--let $assert_text= All the channel's IO thread should be in OFF state.
--let $assert_cond= "[SELECT COUNT(*) FROM performance_schema.replication_connection_status WHERE SERVICE_STATE="OFF"]" = $number_of_channels
--source include/assert.inc

--let $assert_text= All the channel's SQL thread should be in OFF state.
--let $assert_cond= "[SELECT COUNT(*) FROM performance_schema.replication_applier_status WHERE SERVICE_STATE="OFF"]" = $number_of_channels
--source include/assert.inc

################################################################################
# Step-3.2: START SLAVE should be able to bring all the *good* channels's IO and SQL
# thread up except the corrupted channel. START SLAVE should throw error
# ER_SLAVE_RLI_INIT_REPOSITORY because it is unable to bring up corrupted
# channel's IO and SQL thread.
################################################################################
--echo ===== START SLAVE without 'FOR CHANNEL' clause.
--error ER_SLAVE_RLI_INIT_REPOSITORY
START SLAVE;

--echo === Assert that all channels IO and SQL threads are in expected state ===
--let $assert_text= Corrupted channel's IO thread should be in OFF state.
--let $assert_cond= "[SELECT CHANNEL_NAME FROM performance_schema.replication_connection_status WHERE SERVICE_STATE= "OFF"]" = "$rpl_channel_name_without_single_quotes"
--source include/assert.inc

--let $assert_text= Corrupted channel's SQL thread should be in OFF state.
--let $assert_cond= "[SELECT CHANNEL_NAME FROM performance_schema.replication_applier_status WHERE SERVICE_STATE= "OFF"]" = "$rpl_channel_name_without_single_quotes"
--source include/assert.inc

--echo ===== ONLY one channel is corrupted. Waiting until started channels be $number_of_good_channels.
--let $wait_condition= SELECT COUNT(*) = $number_of_good_channels FROM performance_schema.replication_connection_status WHERE SERVICE_STATE="ON"
--source include/wait_condition_or_abort.inc

--echo ===== ONLY one channel is corrupted. Waiting until started channels be $number_of_good_channels.
--let $wait_condition= SELECT COUNT(*) = $number_of_good_channels FROM performance_schema.replication_applier_status WHERE SERVICE_STATE="ON"
--source include/wait_condition_or_abort.inc

###############################################################################
# Step-3.3: SHOW RELAYLOG EVENTS without 'FOR CHANNEL' clause when we have
#           multiple channels, should throw error
#           ER_SLAVE_MULTIPLE_CHANNELS_CMD.
###############################################################################

--echo ===== SHOW RELAYLOG EVENTS without 'FOR CHANNEL' clause should throw error.
--error ER_SLAVE_MULTIPLE_CHANNELS_CMD
SHOW RELAYLOG EVENTS;

###############################################################################
# Step-3.4: MASTER_POS_WAIT function without 4th argument (channel name
#           argument) when we have multiple channels, should throw error
#           ER_SLAVE_MULTIPLE_CHANNELS_CMD.
###############################################################################

--echo ===== SELECT MASTER_POS_WAIT(...) without 'FOR CHANNEL' argument should throw error.
--error ER_SLAVE_MULTIPLE_CHANNELS_CMD
SELECT MASTER_POS_WAIT('server1-bin.000001', 120, 5);

###############################################################################
# Step-3.5: WAIT_UNTIL_SQL_THREAD_AFTER_GTIDS function without 3rd argument
#           (channel name argument) when we have multiple channels, should
#           throw error ER_SLAVE_MULTIPLE_CHANNELS_CMD.
###############################################################################

--echo ===== SELECT WAIT_UNTIL_SQL_THREAD_AFTER_GTIDS(...) without 'FOR CHANNEL' argument should throw error.
--error ER_SLAVE_MULTIPLE_CHANNELS_CMD
SELECT WAIT_UNTIL_SQL_THREAD_AFTER_GTIDS('01010101-0101-0101-0101-010101010101:1', 1);

###############################################################################
# Step-3.6: CHANGE MASTER TO without 'FOR CHANNEL' clause when we have multiple
#           channels, should throw error ER_SLAVE_MULTIPLE_CHANNELS_CMD.
###############################################################################

--echo ===== CHANGE MASTER without 'FOR CHANNEL' clause should throw error.
--error ER_SLAVE_MULTIPLE_CHANNELS_CMD
CHANGE MASTER TO MASTER_HEARTBEAT_PERIOD=10,  MASTER_CONNECT_RETRY=10, MASTER_RETRY_COUNT=10;

###############################################################################
# Step-3.7: Flush relay logs without empty channel acts on all replication
#           channels.
###############################################################################

--echo ===== FLUSH RELAY LOGS without 'FOR CHANNEL' clause.
FLUSH RELAY LOGS;

###############################################################################
# Step-4: Check basic commands with 'FOR CHANNEL' clause.
#         The following while loop will iterate through all the channels
#         Order: default channel (empty name), channel_2, channel_3,...
###############################################################################

--let $i=1
while ($i <= $number_of_channels)
{
  --let $channel_name='channel_$i'
  if ($i == 1)
  {
    --let $channel_name=''
  }

  ##############################################################################
  # Step-4.1: Check MASTER_POS_WAIT and WAIT_UNTIL_SQL_THREAD_AFTER_GTIDS
  #           functions. We need two iterations here.
  #           > j ==1 : Test with past position/gtid (already executed)
  #                     Return value for corrupted channel: NULL
  #                     Return value for good channel: 0
  #           > j ==2 : Test with future position/gtid
  #                     Return value for corrupted channel: NULL
  #                     Return value for good channel: -1
  ##############################################################################

  --echo ===== Executing few basic commands on channel $channel_name.
  --echo ===== Executing MASTER_POS_WAIT() and WAIT_UNTIL_SQL_THREAD_AFTER_GTIDS().
  --let $j=1
  while ($j <=2)
  {
    --let $rpl_connection_name= server_$i
    --source include/rpl_connection.inc
    --let $master_executed_gtid=`SELECT @@GLOBAL.gtid_executed`
    --let $position=query_get_value("SHOW MASTER STATUS", Position, 1)
    --let $masked_position= POSITION
    --let $query_expected_result= 0
    if ($j == 2)
    {
      --let $master_executed_gtid= `SELECT concat(@@GLOBAL.gtid_executed, ':100')`
      --let $position= `SELECT $position + 1`
      --let $masked_position= POSITION+1
      --let $query_expected_result= -1
    }
    if ($rpl_corrupt_channel_name == $channel_name)
    {
      --let $query_expected_result= NULL
    }

    --let $rpl_connection_name= server_$rpl_number_of_servers
    --source include/rpl_connection.inc
    --let $query_result= query_get_value("SELECT MASTER_POS_WAIT('server$i-bin.000001', $position, 1, $channel_name) AS VALUE", VALUE, 1)
    --let $assert_text= MASTER_POS_WAIT should return $query_expected_result for channel $channel_name and position $masked_position.
    --let $assert_cond= "$query_result" = "$query_expected_result"
    --source include/assert.inc

    if ( `SELECT @@GLOBAL.GTID_MODE = "ON"` )
    {
      --let $query_result= query_get_value("SELECT WAIT_UNTIL_SQL_THREAD_AFTER_GTIDS('$master_executed_gtid', 1, $channel_name) AS VALUE", VALUE, 1)
      --let $assert_text= WAIT_UNTIL_SQL_THREAD_AFTER_GTIDS should return $query_expected_result for channel $channel_name and position $masked_position.
      --let $assert_cond= "$query_result" = "$query_expected_result"
      # GTID_MODE OFF/ON, .result file should be same. Hence disabling the query
      # log for the following test.
      --let $include_silent= 1
      --source include/assert.inc
      --let $include_silent=
    }
    --inc $j
  }

  #############################################################################
  # Step 4.2: FLUSH RELAY LOGS FOR CHANNEL 'channel_name'. For a corrupted
  # channel it is equal to No-op operation, no error in that case.
  #############################################################################

  --echo ===== Executing FLUSH RELAY LOGS FOR CHANNEL on channel $channel_name.
  --eval FLUSH RELAY LOGS FOR CHANNEL $channel_name

  #############################################################################
  # Step 4.3: SHOW RELAYLOG EVENTS FOR CHANNEL 'channel_name'
  #############################################################################

  --echo ===== Executing SHOW RELAYLOG EVENTS FOR CHANNEL on channel $channel_name.
  --let $rpl_channel_name=$channel_name
  # It doesn't care the result, just make sure the query can be executed successfully
  --disable_result_log
  eval SHOW RELAYLOG EVENTS FOR CHANNEL $channel_name;
  --enable_result_log

  #############################################################################
  # Step 4.4: SHOW SLAVE STATUS FOR CHANNEL 'channel_name'
  #############################################################################

  --echo ===== Executing SHOW SLAVE FOR CHANNEL on channel $channel_name.

    # Wait until slave threads are synced with master
    --let $wait_condition=SELECT COUNT(*)=$number_of_good_channels FROM performance_schema.threads WHERE NAME="thread/sql/slave_sql" AND PROCESSLIST_STATE="Slave has read all relay log; waiting for more updates";
  --source include/wait_condition_or_abort.inc
  --let $wait_condition=SELECT COUNT(*)=$number_of_good_channels FROM performance_schema.threads WHERE NAME="thread/sql/slave_io" AND PROCESSLIST_STATE="Waiting for master to send event";
  --source include/wait_condition_or_abort.inc

  --let $status_items=Slave_IO_State,Slave_SQL_Running_State,Last_IO_Errno,Last_IO_Error,Last_SQL_Errno,Last_SQL_Error,Channel_Name
  --let $rpl_channel_name=$channel_name
  --source include/show_slave_status.inc

  #############################################################################
  # Step 5: If this channel is a good channel, replication should work fine
  #         without any issues.
  # Note: Before executing 'STOP SLAVE FOR CHANNEL' command, execute this sync
  #       step to avoid stopping and staring these slave threads.
  #############################################################################

  if ($channel_name != $rpl_corrupt_channel_name)
  {
    --echo ===== Check that replication is working fine on channel $channel_name.
    --let $rpl_connection_name= server_$i
    --source include/rpl_connection.inc
    CREATE TABLE t1(i INT);
    INSERT INTO t1 VALUES (12);
    DROP TABLE t1;
    --let $rpl_channel_name=$channel_name
    --let $sync_slave_connection=server_$rpl_number_of_servers
    --source include/sync_slave_sql_with_master.inc
    --dec $number_of_good_channels
  }

  #############################################################################
  # Step 4.5: STOP SLAVE FOR CHANNEL 'channel_name'
  #############################################################################

  --echo ===== Executing STOP SLAVE FOR CHANNEL on channel $channel_name.
  --let $rpl_channel_name=$channel_name
  --source include/stop_slave.inc

  #############################################################################
  # Step 4.6: CHANGE MASTER TO ... FOR CHANNEL 'channel_name'
  #############################################################################

  --echo ===== Executing CHANGE MASTER FOR CHANNEL on channel $channel_name.
  --eval CHANGE MASTER TO MASTER_CONNECT_RETRY=100, MASTER_RETRY_COUNT=100 FOR CHANNEL $channel_name
  --let $status_items=Connect_Retry, Master_Retry_Count, Slave_IO_State,Slave_SQL_Running_State, Channel_Name
  --let $rpl_channel_name=$channel_name
  --source include/show_slave_status.inc

  #############################################################################
  # Step 4.7: RESET SLAVE FOR CHANNEL 'channel_name'
  #############################################################################

  --echo ===== Executing RESET SLAVE FOR CHANNEL on channel $channel_name.
  --eval RESET SLAVE FOR CHANNEL $channel_name

  #############################################################################
  # Step 4.8: RESET SLAVE ALL FOR CHANNEL 'channel_name'
  #############################################################################

  --echo ===== Executing RESET SLAVE ALL FOR CHANNEL on channel $channel_name.
  --eval RESET SLAVE ALL FOR CHANNEL $channel_name

  --inc $i
}
