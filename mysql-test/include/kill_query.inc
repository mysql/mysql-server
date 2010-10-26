# ==== Purpose ====
#
# Kill a query in the master connection, and then try to reap the
# result of the killed query.
#
# ==== Usage ====
# 
# The following variables should be set before sourcing this file.
#
# $debug_lock: name of the debug user lock, if set, will release/lock
#              the specified debug lock accordingly, and before
#              sourcing this, connection 'master' should get the user
#              lock and run a query in another thread, which will
#              block before creating statement event.
#
# $connection_name: name of the connection that is waiting for the
#                   lock, this can not be 'master'
#
# $connection_id: id of the connection that is waiting for the lock
#
# Example:
# let $debug_lock=;
# connection master1;
# let $connection_name= master1;
# let $connection_id= `SELECT CONNECTION_ID()`;
# send CREATE TABLE t1;
# source kill_query.inc;
#
# let $debug_lock= "debug_lock.before_query_log_event";
# connection master;
# eval SELECT GET_LOCK($debug_lock, 10);
# connection master1;
# let $connection_name= master1;
# let $connection_id= `SELECT CONNECTION_ID()`;
# send CREATE TABLE t1;
# source kill_query.inc;


--echo source include/kill_query.inc;
disable_query_log;
disable_result_log;
connection master;

# kill the query that is waiting
eval kill query $connection_id;

if ($debug_lock)
{
  # release the lock to allow binlog continue
  eval SELECT RELEASE_LOCK($debug_lock);
}

# reap the result of the waiting query
connection $connection_name;
error 0, 1317, 1307, 1306, 1334, 1305, 1034;
reap;

connection master;

if ($debug_lock)
{
  # get lock again to make the next query wait
  eval SELECT GET_LOCK($debug_lock, 10);
}

connection $connection_name;
enable_query_log;
enable_result_log;
