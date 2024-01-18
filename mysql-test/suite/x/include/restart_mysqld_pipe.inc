
if ($rpl_inited)
{
  if (!$allow_rpl_inited)
  {
    --die ERROR IN TEST: When using the replication test framework (rpl/init_source_replica.inc, rpl/init.inc etc), use rpl/restart_server.inc instead of restart_mysqld.inc. If you know what you are doing and you really have to use restart_mysqld.inc, set allow_rpl_inited=1 before you source restart_mysqld.inc
  }
}

if (!$restart_parameters)
{
  let $restart_parameters = restart;
}

# We use --exec echo instead of simple "--echo" because
# replace_result works on --exec
--exec echo "# $restart_parameters"

# Write file to make mysql-test-run.pl expect the "crash", but don't start
# it until it's told to
--let $_server_id= `SELECT @@server_id`
--let $_expect_file_name= $MYSQLTEST_VARDIR/tmp/mysqld.$_server_id.expect

# Allow restarting different server than the one currently
# connected to, actually one where suffix is not .serverid but
# .serverid.clusterid
if ($mysqld_name)
{
  let $_expect_file_name= $MYSQLTEST_VARDIR/tmp/$mysqld_name.expect;
  # echo expect_file_name: $_expect_file_name;
}
--exec echo "wait" > $_expect_file_name

# Send shutdown to the connected server.
--error 0,1,175
shutdown_server;

--source include/wait_until_disconnected.inc

# Write file to make mysql-test-run.pl start up the server again
--exec echo "$restart_parameters" > $_expect_file_name

# Call script that will poll the server waiting for it to be back online again
--source ../include/wait_until_connected_again_pipe.inc
