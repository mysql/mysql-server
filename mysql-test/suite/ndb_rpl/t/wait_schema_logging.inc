# suite/t/rpl_ndb/wait_schema_logging.inc
#
# SUMMARY
#   Waits until schema op performed on $source_server is
#   being logged by $dest_server
#   Assumes that source_server and dest_server are connected
#   to the same cluster
#   Assumes that $dest_server has binlogging enabled.
#
###############################################################
# Include file to wait for schema logging setup, where schema
# changes made on $source_server are logged on $dest_server
###############################################################

connection $dest_server;

#--echo Waiting for schema event logging to be active between $source_server and $dest_server
#--echo - First check that $dest_server has binary logging enabled

let $binlog= query_get_value(show variables like 'log_bin', Value, 1);
if (`SELECT "$binlog" != "ON"`)
{
  show variables like 'server_id';
  show variables like 'log_bin';
  --die ERROR: Binlogging not activated
}

--disable_query_log
RESET BINARY LOGS AND GTIDS;
--enable_query_log

# Following code 'inspired' by include/rpl/wait_for_binlog_event.inc

--let $wait_binlog_event=drop table

let $_loop_count= 10;
let $_event= ;
let $_event_pos= 1;

while (`SELECT INSTR("$_event","$wait_binlog_event") = 0`)
{
  #--echo loop_count is $_loop_count;
  dec $_loop_count;
  if (!$_loop_count)
  {
    SHOW BINLOG EVENTS;
    --die ERROR: failed while waiting local cluster schema event logging to start
  }

  connection $source_server;
  --disable_query_log
  # Supress warning for no replica update filter in NDB
  --disable_warnings
  create table wait_schema_logging (a int primary key) engine=ndb;
  --enable_warnings
  drop table wait_schema_logging;
  --enable_query_log
  connection $dest_server;

  let $_event= query_get_value(SHOW BINLOG EVENTS, Info, $_event_pos);
  #--echo Event is : $_event
  while (`SELECT ("$_event" != "No such row") and (INSTR("$_event", "$wait_binlog_event") = 0)`)
  {
    inc $_event_pos;
    let $_event= query_get_value(SHOW BINLOG EVENTS, Info, $_event_pos);
    #--echo Event did not match- next event is $_event;
  }
}

# Since we're attached to the same cluster, if we've seen the event
# then synchronous binlogging of changes is on (i.e. synchronous with
# the original schema change)
# Therefore it should now be safe to reset the master on the destination
# server as no more binlog changes are in-flight.
connection $dest_server;

--disable_query_log
RESET BINARY LOGS AND GTIDS;
--enable_query_log

#--echo - Schema event logging is active
