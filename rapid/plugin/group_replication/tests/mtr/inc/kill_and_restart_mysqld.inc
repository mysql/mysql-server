# This is file is needed while it is not possible to mask logged
# output from include/kill_and_restart_mysqld.inc

if (!$restart_parameters)
{
  let $restart_parameters = restart;
}

--let $_server_id= `SELECT @@server_id`
--let $_expect_file_name= $MYSQLTEST_VARDIR/tmp/mysqld.$_server_id.expect

# We use '--exec echo' because it allows replace_result to mask its
# output.
--exec echo "# Kill and $restart_parameters"
--exec echo "$restart_parameters" > $_expect_file_name
--shutdown_server 0
--source include/wait_until_disconnected.inc
--enable_reconnect
--source include/wait_until_connected_again.inc
--disable_reconnect
