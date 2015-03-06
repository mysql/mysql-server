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
# --source extra/rpl_tests/rpl_trx_boundary_parser.inc
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
--source include/rpl_connection_slave.inc
--source include/stop_slave.inc
--disable_warnings
DROP TABLE IF EXISTS t1, t2;
DROP EVENT IF EXISTS ev1;
--enable_warnings
RESET MASTER;
RESET SLAVE;
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
START SLAVE IO_THREAD;
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
--source include/wait_for_slave_io_to_stop.inc
# Get the position of the last queued event
--let $master_pos= query_get_value(SHOW RELAYLOG EVENTS IN 'slave-relay-bin.000003' FROM $master_pos, Pos, 2)
###
### Display the last event queued by the IO thread
###
--echo # Stopped IO thread after queuing the following event (#$stop_after):
--let $binlog_start= $master_pos
--let $binlog_file= slave-relay-bin.000003
--let $binlog_limit=
--source include/show_relaylog_events.inc
--enable_query_log

###
### Restart the slave changing the master.info file
###
# Stop the slave server
--let $rpl_server_number= 2
--let $rpl_force_stop= 0
--source include/rpl_stop_server.inc
# Remove the current master.info
--remove_file $MYSQLD_SLAVE_DATADIR./master.info
--echo # Restoring a master.info.backup to start asking events at event #$restart_at
--copy_file $MYSQLD_SLAVE_DATADIR./master.backup.$_restart_at $MYSQLD_SLAVE_DATADIR./master.info
# Start the slave again
--source include/rpl_start_server.inc
# Start the IO thread to receive the events from the $restart_at point
--source include/start_slave_io.inc
--source include/rpl_connection_master.inc

--let $use_gtids= 0
--source include/sync_slave_io_with_master.inc
--echo # Restarted queuing the following event (#$restart_at):
--replace_result $master_uuid MASTER_UUID
--let $binlog_start=
--let $binlog_file= slave-relay-bin.000005
--let $binlog_limit= 3, 1
--source include/show_relaylog_events.inc

--let $rpl_connection_silent=
