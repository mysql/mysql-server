## Create connection to clone instance optionally restarting instance with
## monitoring process.

# These variables can to be set before sourcing this file. Currently we used
# to test both with and without monitoring process.
#
# 1. Need to restart mysqld with monitoring process
# --let inst_monitor = 1
#
# 2. Number of connections to be dropped from the instance
# --let clone_connections = <number of connections> 1/2/3 ...
# Connections have the name as clone_conn_<number>
# e.g. clone_conn_1, clone_conn_2, clone_conn_3 etc.
#
# 3. This script should be used in pair with clone_connection_begin.inc
# --source clone_connection_begin.inc
# ...
# --source clone_connection_end.inc

if (!$clone_user) {
  --let $clone_user = 'root'
}

if (!$clone_connections) {
  --let $clone_connections = 1
}

# A. Drop the connections
--connection clone_conn_1
# In case server is restarted, we need to reconnect
--source include/wait_until_connected_again.inc

# Get Server ID
--let $SERVER_ID= `SELECT @@server_id`

--let $conn_nummber = 1

# If not the default instance, keep first connection.
if ($SERVER_ID != 1) {
 --let $conn_nummber =2

 --echo Uninstall clone plugin on recipient server
 UNINSTALL PLUGIN clone;
}

while ($conn_nummber <= $clone_connections)
{
  --let $conn_name = clone_conn_$conn_nummber
  --disconnect $conn_name
  --inc $conn_nummber
}

# B. Check if we need monitoring process. Taken from mysqld_safe.test
if ($inst_monitor) {

  # Reconnect default if we shut down default server.
  if ($SERVER_ID == 1) {
    --connection default
  }

  # 1. Shutdown the server
  --disable_query_log
  --enable_query_log
  --exec echo "wait" > $MYSQLTEST_VARDIR/tmp/mysqld.$SERVER_ID.expect
  --shutdown_server
  --source include/wait_until_disconnected.inc

  # 2. Restart mysqld having $SERVER_ID
  --exec echo "restart" > $MYSQLTEST_VARDIR/tmp/mysqld.$SERVER_ID.expect
  --source include/wait_until_disconnected.inc
  --source include/wait_until_connected_again.inc
}

# Now disconnect the skipped connection
if ($SERVER_ID != 1) {
  --disconnect clone_conn_1
}

# Switch to default connection
--connection default
