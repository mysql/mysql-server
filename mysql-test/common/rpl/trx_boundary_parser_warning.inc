# ==== Purpose ====
#
# This include will wait for the slave IO thread to queue a given amount of
# events, will stop the whole slave server, will restore a master.info file
# based on a desired point to restart queuing events and will restart the
# slave and the slave IO thread.
#
# ==== Usage ====
#
# --let $stop_after= <EVENT NUMBER TO STOP AFTER>
# --let $restart_at= <EVENT NUMBER TO RESTART AT>
# --source common/rpl/trx_boundary_parser.inc
#
# Parameters:
#   $stop_after
#     The number of events this include will let the slave IO thread to queue
#     before restarting the slave server.
#
#   $restart_at
#     The event that should be the first queued by the slave IO thread after
#     restarting the slave server.
#
#   $master_2nd_event_pos
#     We always start counting by the second event, skipping slave's FD, its exact position
#     is compated by this macro caller to be passed here as a parameter


if (!$stop_after)
{
  --die ERROR IN TEST: invalid value for mysqltest variable 'stop_after': $stop_after
}
if (!$restart_at)
{
  --die ERROR IN TEST: invalid value for mysqltest variable 'restart_at': $restart_at
}

--let $rpl_connection_silent= 1

###
### Cleanup and reset the slave
###
--disable_query_log
--echo # Cleaning up and reseting the slave
--source include/rpl/connection_replica.inc
--source include/rpl/stop_replica.inc
--disable_warnings
DROP TABLE IF EXISTS t1, t2;
DROP EVENT IF EXISTS ev1;
--enable_warnings
RESET BINARY LOGS AND GTIDS;
RESET REPLICA;
--enable_query_log

###
### Adjust the value of $restart_at to this include
###
--let $_stop_after= $stop_after
--let $_restart_at= $restart_at
# We will restart using the master_info file saved after
# queuing the event preceding the one we want.
--dec $_restart_at

###
### Prepare to count the events queued to the IO thread
###
--let $debug_point= pause_on_queuing_event
--source include/add_debug_point.inc
# Get the master.info file path (the $MYSQLD_SLAVE_DATADIR)
--let $MYSQLD_SLAVE_DATADIR= `select @@datadir`
# Start the IO thread to start receiving events from master
--source include/rpl/start_receiver.inc
--let $event_counter= 2
--let $master_pos= $master_2nd_event_pos
# Disabling query log during queue event counting
--disable_query_log
###
### Counting events
###
while ($event_counter <= $_stop_after)
{
  SET DEBUG_SYNC='now WAIT_FOR reached_queuing_event';
  # After the relay log PREVIOUS_GTIDS ($event_counter == 2) we save the event
  # position to display only the last event queued.
  if ($event_counter > 2)
  {
    --let $master_pos= query_get_value(SHOW RELAYLOG EVENTS IN 'slave-relay-bin.000003' FROM $master_pos, Pos, 2)
  }
  # If we reached the event to stop after, we can stop counting events
  if ($event_counter == $_stop_after)
  {
    --source include/remove_debug_point.inc
    --let $debug_point= stop_io_after_queuing_event
    --source include/add_debug_point.inc
  }
  # If we don't reached the event to stop after yet, make the IO thread to
  # queue the current event and retrieve the next one.
  if ($event_counter <= $_stop_after)
  {
    SET DEBUG_SYNC= 'now SIGNAL continue_queuing_event';
  }
  --inc $event_counter
}
--source include/rpl/wait_for_receiver_to_stop.inc
# Get the position of the last queued event
--let $master_pos= query_get_value(SHOW RELAYLOG EVENTS IN 'slave-relay-bin.000003' FROM $master_pos, Pos, 2)
###
### Display the last event queued by the IO thread
###
--echo # Stopped IO thread after queuing the following event (#$stop_after):
--let $binlog_start= $master_pos
--let $binlog_file= slave-relay-bin.000003
--let $binlog_limit=
--source include/rpl/deprecated/show_relaylog_events.inc
--enable_query_log

###
### Restart the replica changing the start point
###
# Stop the replica server
--let $rpl_server_number= 2
--let $rpl_force_stop= 0
--source include/rpl/stop_server.inc
# Start the replica again, with relay log sanitization disabled. Relay log
# sanitization process removes partially written transactions from
# relay log. Here, we want to disable it to prove a correct behavior of the
# Transaction Boundary Parser.
--let $rpl_server_parameters= --debug=d,dbug_disable_relay_log_truncation
--source include/rpl/start_server.inc
--let $start_after = $restart_at - 1
--let $relay_log_file= query_get_value(SHOW REPLICA STATUS, Relay_Log_File, 1)
--let $saved_pos = query_get_value(SELECT pos AS value FROM saved_positions WHERE id = $start_after, value, 1)
# Set relay log file to prevent server from purging relay logs, which would
# reset the state of the transaction boundary parser
--eval CHANGE REPLICATION SOURCE TO SOURCE_LOG_POS = $saved_pos, RELAY_LOG_FILE = '$relay_log_file'

# Start the IO thread to receive the events from the $restart_at point
--source include/rpl/start_receiver.inc
--source include/rpl/connection_source.inc

--let $use_gtids= 0
--source include/rpl/sync_to_replica_received.inc

--echo # Restarted queuing the following event (#$restart_at):
--replace_result $master_uuid MASTER_UUID
--let $binlog_start=
--let $binlog_file= slave-relay-bin.000005
--let $binlog_limit= 3, 1
--source include/rpl/deprecated/show_relaylog_events.inc

--let $rpl_connection_silent=
