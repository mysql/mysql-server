# ==== Purpose ====
#
# Receive a specified number of events on the slave, possibly pausing
# in the middle of a transaction.
#
# This script assumes that the receiver thread is stopped when the
# script starts.  The script does not involve the applier thread(s).
#
# ==== Usage ====
#
# --let $rpl_after_received_events_action= [stop|flush|hang]
# --let $rpl_event_count= N
# [--let $rpl_channel_name= NAME]
# [--let $rpl_count_header_events= 1]
# [--let $rpl_count_only_event_type= EVENT_TYPE]
# [--let $slave_timeout= N]
# [--let $rpl_debug= 1]
# [--let $rpl_continue_event_count_after_hanging= 1]
# [--let $rpl_skip_event_count_print_in_result_log= 1]
# --source include/rpl_receive_event_count.inc
#
# Parameters:
#
# $rpl_after_received_events_action
#   Specifies what to do after receiving the events:
#   stop  - Issue STOP SLAVE IO_THREAD and wait for it to stop.
#   flush - Issue FLUSH RELAY LOG and then continue to replicate.
#   hang  - Leave the receiver thread hanging on a debug sync point.
#           The caller must later release the receiver thread using:
#           SET DEBUG_SYNC= 'now SIGNAL continue_after_queue_event';
#
# $rpl_event_count
#   Replicate this number of events before stopping.
#
# $rpl_count_header_events
#   By default, the count starts after all
#   Format_description_log_events, Rotate_log_events, and
#   Previous_gtids_log_events.  If this parameter is set, these events
#   are counted too.
#
# $rpl_count_only_event_type
#   By default, counts events of all types (except those excluded by
#   $rpl_count_header_events).  If this variable is set, counts only
#   this type of event.
#
# $slave_timeout
#   Wait this number of seconds before receiving events (default 300).
#
# $rpl_debug
#   Print debug info.
#
# $rpl_channel_name
#   See include/wait_for_slave_param.inc
#
# $rpl_continue_event_count_after_hanging
#   The include should not start the I/O Thread because it is already
#   started.
#
# $rpl_skip_event_count_print_in_result_log
#   Set if you do not want to print sensible info (action and
#   event count) in the result log. This is useful if action or event
#   count varies for different configurations. For eg: event count
#   can be different for STS and MTS.

# Sanity check.
if (!$rpl_event_count)
{
  --die ERROR IN TEST: $rpl_event_count must be > 0.
}

if (!$rpl_after_received_events_action)
{
  --die ERROR IN TEST: $rpl_after_received_events_action must be set.
}
# IO thread should be stopped
--let $_for_channel=
if ($rpl_channel_name != '')
{
  --let $_for_channel= FOR CHANNEL '$rpl_channel_name'
}

--let $is_slave_io_running= query_get_value(SHOW SLAVE STATUS $_for_channel, Slave_IO_Running, 1)
if (!$rpl_continue_event_count_after_hanging)
{
  if ($is_slave_io_running != 'No' )
  {
    --die ERROR IN TEST: The I/O thread was expected to be stopped.
  }
}
if ($rpl_continue_event_count_after_hanging)
{
  if ($is_slave_io_running != 'Yes' )
  {
    --die ERROR IN TEST: The I/O thread was expected to be running.
  }
}
# Generate something sensible on the result log.
--let $_rrec_ev= events
if ($rpl_event_count == 1)
{
  --let $_rrec_ev= event
}
--let $_rrec_event_type_text= non-header $_rrec_ev
if ($rpl_count_header_events)
{
  --let $_rrec_event_type_text= $_rrec_ev
}
if ($rpl_count_only_event_type)
{
  --let $_rrec_event_type_text= $rpl_count_only_event_type $_rrec_ev
}
if (!$rpl_skip_event_count_print_in_result_log)
{
--let $include_filename= rpl_receive_event_count.inc [$rpl_after_received_events_action after $rpl_event_count $_rrec_event_type_text]
}
if ($rpl_skip_event_count_print_in_result_log)
{
--let $include_filename= rpl_receive_event_count.inc
}
--source include/begin_include_file.inc

if (!$rpl_debug)
{
  --disable_query_log
}

--let $_rrec_timeout_clause= TIMEOUT 300
if ($slave_timeout)
{
  --let $_rrec_timeout_clause= TIMEOUT $slave_timeout
}

# Make it pause on the debug point and start the receiver thread.
--let $debug_point= pause_after_queue_event
--let $debug_point_silent= 1
--source include/add_debug_point.inc

if (!$rpl_continue_event_count_after_hanging)
{
  --eval START SLAVE IO_THREAD $_for_channel;
}
if ($rpl_continue_event_count_after_hanging)
{
  SET DEBUG_SYNC= 'now SIGNAL continue_after_queue_event';
}

# Cleanup from previous runs
--let $_rrec_file=

# Determine if we need to check the event type.
--let $_rrec_need_event_type= 0
if ($rpl_count_event_type)
{
  --let $_rrec_need_event_type= 1
}
if (!$rpl_count_header_events)
{
  --let $_rrec_need_event_type= 1
}
if (!$_rrec_need_event_type)
{
  --let $_rrec_event_type= [unknown]
}

# Step through events.
# Loop invariant: at the beginning of each iteration, it has received
# and queued one more event.
--let $_rrec_counted_events= 0
while ($_rrec_counted_events < $rpl_event_count)
{
  eval SET DEBUG_SYNC= 'now WAIT_FOR reached_after_queue_event $_rrec_timeout_clause';

  if (`SHOW COUNT(*) WARNINGS`)
  {
    --source include/show_rpl_debug_info.inc
    --die Timeout in rpl_receive_event_count.inc while waiting for event to be queued.
  }

  # Get the event type.
  if ($_rrec_need_event_type)
  {
    # Get the position of the event.
    if ($_rrec_file != '')
    {
      # In non-first iteration, read the position of the next event.
      --let $_rrec_pos= query_get_value("SHOW RELAYLOG EVENTS IN '$_rrec_file' FROM $_rrec_pos LIMIT 1, 1", Pos, 1)
    }
    if ($_rrec_file == '')
    {
      # Find name of last relay log.
      --source include/rpl_get_end_of_relay_log.inc
      --let $_rrec_file= $relay_log_file
      # Find number of events in last relay log.
      --let $statement= SHOW RELAYLOG EVENTS IN '$_rrec_file'
      --let $column= Log_name
      --source include/get_row_count.inc
      # Get position for last event in relay log.
      --dec $row_count
      --let $_rrec_pos= query_get_value("SHOW RELAYLOG EVENTS IN '$_rrec_file' LIMIT $row_count, 1", Pos, 1)
    }
    # Get the event type.
    --let $_rrec_event_type= query_get_value(SHOW RELAYLOG EVENTS IN '$_rrec_file' FROM $_rrec_pos, Event_type, 1)
    if ($rpl_debug)
    {
      --echo Received event of type $_rrec_event_type at file $_rrec_file, position $_rrec_pos
    }
  }

  # Skip events that don't match $rpl_count_only_event_type.
  --let $_rrec_count_event= 1
  if ($rpl_count_only_event_type != '')
  {
    if ($_rrec_event_type != $rpl_count_only_event_type)
    {
      if ($rpl_debug)
      {
        --echo Skipping event of type $_rrec_event_type (is not a $rpl_count_only_event_type)
      }
      --let $_rrec_count_event= 0
    }
  }

  # Skip header events.
  if (!$rpl_count_header_events)
  {
    if ($_rrec_event_type == Format_desc)
    {
      --let $_rrec_count_event= 0
      if ($rpl_debug)
      {
        --echo Skipping event of type $_rrec_event_type (is a header event)
      }
    }
    if ($_rrec_event_type == Rotate)
    {
      --let $_rrec_count_event= 0
      if ($rpl_debug)
      {
        --echo Skipping event of type $_rrec_event_type (is a header event)
      }
    }
    if ($_rrec_event_type == Previous_gtids)
    {
      --let $_rrec_count_event= 0
      if ($rpl_debug)
      {
        --echo Skipping event of type $_rrec_event_type (is a header event)
      }
    }
  }

  # Increment counter if this event counts.
  if ($_rrec_count_event)
  {
    if ($rpl_debug)
    {
      --echo Counting event of type '$_rrec_event_type'
    }
    --inc $_rrec_counted_events
  }

  # Unpause the receiver thread.
  if ($_rrec_counted_events < $rpl_event_count)
  {
    SET DEBUG_SYNC= 'now SIGNAL continue_after_queue_event';
  }
}

# Execute the required action.
if ($rpl_after_received_events_action == 'flush')
{
  --source include/remove_debug_point.inc
  FLUSH LOCAL RELAY LOGS;
  SET DEBUG_SYNC= 'now SIGNAL continue_after_queue_event';
}

if ($rpl_after_received_events_action == 'stop')
{
  --let $_rrec_connection= $CURRENT_CONNECTION
  --let $_rrec_port= `SELECT @@PORT`
  --connect (_rrec_other_connection,127.0.0.1,root,,test,$_rrec_port,)

  --connection _rrec_other_connection
  --send_eval STOP SLAVE IO_THREAD $_for_channel;
  --connection $_rrec_connection

  SET DEBUG_SYNC= 'now WAIT_FOR reached_stopping_io_thread';
  --source include/remove_debug_point.inc
  SET DEBUG_SYNC= 'now SIGNAL continue_after_queue_event';

  --connection _rrec_other_connection
  --reap
  --connection $_rrec_connection

  --disconnect _rrec_other_connection
  --source include/wait_for_slave_io_to_stop.inc
}

if ($rpl_after_received_events_action != 'flush')
{
  if ($rpl_after_received_events_action != 'stop')
  {
    --source include/remove_debug_point.inc
  }
}

--let $include_filename= rpl_receive_event_count.inc
--source include/end_include_file.inc
