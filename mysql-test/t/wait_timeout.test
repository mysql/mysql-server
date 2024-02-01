
# Last modification:
# 2009-01-19 H.Hunger Fix Bug#39108 main.wait_timeout fails sporadically
#                       - Increase wait timeout to 2 seconds
#                       - Eliminated the corresponding opt file,
#                         set global wait timeout within the test.
#                       - Replaced sleeps by wait condition
#                       - Minor improvements
###############################################################################
-- source include/one_thread_per_connection.inc

# Save the initial number of concurrent sessions
--source include/count_sessions.inc
SET @@global.log_error_verbosity= 3;

#
# Bug#8731 wait_timeout does not work on Mac OS X
#

let $start_value= `SELECT @@global.wait_timeout`;
SET @@global.wait_timeout= 2;
--echo disconnect default;
disconnect default;

# Connect with another connection and reset counters
--disable_query_log
connect (wait_con,localhost,root,,test,,);
--echo connection wait_con;
connection wait_con;
SET SESSION wait_timeout=100;
let $retries=300;
SET @aborted_clients= 0;
--enable_query_log

# Do the query
connect (default,localhost,root,,test,,);
--echo connection default;
connection default;
SELECT 1;

# Switch to wait_con and wait until server has aborted the connection
--disable_query_log
--echo connection wait_con;
connection wait_con;
while (!`select @aborted_clients`)
{
  sleep 0.1;
  let $aborted_clients = `SHOW STATUS LIKE 'aborted_clients'`;
  eval SET @aborted_clients= SUBSTRING('$aborted_clients', 16)+0;

  dec $retries;
  if (!$retries)
  {
    die Failed to detect that client has been aborted;
  }
}
--enable_query_log
# The server has disconnected, add small sleep to make sure
# the disconnect has reached client
let $wait_condition= SELECT COUNT(*)=2 FROM information_schema.processlist;
--source include/wait_condition.inc
--echo connection default;
connection default;
# When the connection is closed in this way, the error code should
# be consistent see Bug#2845 for an explanation
# depending on platform/client, either errno 2006 or 2013 can occur below
--error ER_CLIENT_INTERACTION_TIMEOUT,CR_SERVER_LOST
SELECT 2;
connect;
SELECT 3;
# Disconnect so that we will not be confused by a future abort from this
# connection.
--echo disconnection default;
disconnect default;

#
# Do the same test as above on a TCP connection
# (which we get by specifying an ip adress)

# Connect with another connection and reset counters
--disable_query_log
--echo connection wait_con;
connection wait_con;
FLUSH STATUS; # Reset counters
let $retries=300;
SET @aborted_clients= 0;
--enable_query_log

--echo connection con1;
connect (con1,127.0.0.1,root,,test,$MASTER_MYPORT,);
SELECT 1;

# Switch to wait_con and wait until server has aborted the connection
--disable_query_log
--echo connection wait_con;
connection wait_con;
while (!`select @aborted_clients`)
{
  sleep 0.1;
  let $aborted_clients = `SHOW STATUS LIKE 'aborted_clients'`;
  eval SET @aborted_clients= SUBSTRING('$aborted_clients', 16)+0;

  dec $retries;
  if (!$retries)
  {
    die Failed to detect that client has been aborted;
  }
}
--enable_query_log
# The server has disconnected, add small sleep to make sure
# the disconnect has reached client
let $wait_condition= SELECT COUNT(*)=2 FROM information_schema.processlist;
--source include/wait_condition.inc
disconnect wait_con;

--echo connection con1;
connection con1;
# When the connection is closed in this way, the error code should
# be consistent see Bug#2845 for an explanation
# depending on platform/client, either errno 2006 or 2013 can occur below
--error ER_CLIENT_INTERACTION_TIMEOUT,CR_SERVER_LOST
SELECT 2;
connect;
SELECT 3;
--replace_result $start_value <start_value>
eval SET @@global.wait_timeout= $start_value;
--echo disconnection con1;
disconnect con1;


# The last connect is to keep tools checking the current test happy.
connect (default,localhost,root,,test,,);

--echo #
--echo # Bug#54790: Use of non-blocking mode for sockets limits performance
--echo #

--echo #
--echo # Test UNIX domain sockets timeout.
--echo #

--echo # Open con1 and set a timeout.
connect(con1,localhost,root,,);

LET $ID= `SELECT connection_id()`;
SET @@SESSION.wait_timeout = 2;

--echo # Wait for con1 to be disconnected.
connection default;
let $wait_condition=
  SELECT COUNT(*) = 0 FROM INFORMATION_SCHEMA.PROCESSLIST
  WHERE ID = $ID;
--source include/wait_condition.inc

--echo # Check that con1 has been disconnected.
connection con1;
--error ER_CLIENT_INTERACTION_TIMEOUT,CR_SERVER_LOST
SELECT 1;

disconnect con1;
connection default;

--echo #
--echo # Test TCP/IP sockets timeout.
--echo #

--echo # Open con1 and set a timeout.
connect(con1,127.0.0.1,root,,,,,TCP,,);

LET $ID= `SELECT connection_id()`;
SET @@SESSION.wait_timeout = 2;

--echo # Wait for con1 to be disconnected.
connection default;
let $wait_condition=
  SELECT COUNT(*) = 0 FROM INFORMATION_SCHEMA.PROCESSLIST
  WHERE ID = $ID;
--source include/wait_condition.inc

--echo # Check that con1 has been disconnected.
connection con1;
--echo # Client interaction timeout
--error ER_CLIENT_INTERACTION_TIMEOUT,CR_SERVER_LOST
SELECT 1;

--error ER_CLIENT_INTERACTION_TIMEOUT,CR_SERVER_LOST
SELECT "Check that we don't reconnect with reconnection disabled.";

connection default;
disconnect con1;
# Wait till all disconnects are completed
--source include/wait_until_count_sessions.inc
#
# Bug #28940167 WAIT_TIMEOUT ERROR NOT CLEAR AND NOT SENT TO CLIENT BEFORE CLOSING CONNECTION
#
--echo # Must find the expected message in the error log
SELECT COUNT(*) > 0 AS EXPECTED_LOG_MESSAGE_FOUND
FROM performance_schema.error_log
WHERE ERROR_CODE = 'MY-015123';
