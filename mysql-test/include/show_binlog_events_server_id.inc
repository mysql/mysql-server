# ==== Purpose ====
#
# Retrieves the server id of a certain event in the binlog.
#
# ==== Usage ====
#
# [--let $event_number= <binlog ordinal of the event> ]
# [--let $binlog_name= <binlog name> ]
# [--let $debug= 1]
# --source include/show_binlog_events_server_id.inc
#
# Parameters:
#   $event_number
#     The ordinal in the list of events returned by SHOW BINLOG EVENTS
#     It is 1-based. Default value is 1.
#
#   $binlog_name
#     The name of the binlog. Default is the one presented by
#     SHOW BINLOG EVENTS.
#
#   $debug
#     Enables debug information. Default is not enabled.
#
# Return Value
#   $event_server_id
#     The value from the server id of the requested event.
#
# Example:
#
# --let $event_number= 2
# --let $binlog_name= master-bin.000002
# --let $debug= 1
# --source include/show_binlog_events_server_id.inc
#
# --echo $event_server_id
#

--let $include_filename= show_binlog_events_server_id.inc
--source include/begin_include_file.inc

--let $_event_number= 1
if($event_number)
{
  --let $_event_number= $event_number;
}

--let $_query= SHOW BINLOG EVENTS
if($binlog_name)
{
  --let $_query= $_query IN '$binlog_name'
}

--let $event_server_id= query_get_value($_query, Server_id, $_event_number)

if($debug)
{
  --echo Server Id for Event is $event_server_id
}

--let $include_filename= show_binlog_events_server_id.inc
--source include/end_include_file.inc
