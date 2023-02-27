# ==== Purpose ====
#
# Shut down mysqlrouter, and then start it again.
#
# Note: this waits for the current connection to reconnect to mysqld,
# but does not wait for other connections. If your test uses multiple
# connections, you must also use include/wait_until_disconnected.inc
# on all other connections.
#
# ==== Usage ====
#
# [--let $instance_id = 1]
# [--let $connection_name = router.1]
# [--let $disable_echo = 1]
# --source include/mysqlrouter_mysql_connection.inc
#
# Parameters:
#
#   $instance_id
#     Use the TCP port that is bound to following router instance.
#     If not specified, then the default instance is used (instance 1).
#
#   $connection_name
#     Name of the new connection. If not specified, then its
#     constructed by concating "router" and $instance_id.
#

if (!$instance_id)
{
	let $instance_id=1;
}

if (!$connection_name)
{
	let $connection_name=router.$instance_id;
}

#TODO: Take the TCP PORT for the instance.
#      Not supported by MTRs scripts in current state. 
connect ($connection_name,127.0.0.1,root,,test,$ROUTER_PORT);
if (!$disable_echo)
{
  echo # Connection "$connection_name" active;
}

--let $instance_id=;
