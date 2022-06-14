# ==== Purpose ====
#
# Shuts down the current server in a way that can be started again within
# an MTR test script.
#
# ==== Usage ====
#
#   [--let $ss_timeout = <seconds>]
#
# Parameters:
#   $ss_timeout
#     Amount of seconds to wait for the server to shutdown before erroring
#     out. This value is passed on to `--shutdown-server`. Default value is
#     60 seconds.
#

if ($rpl_inited)
{
  if (!$allow_rpl_inited)
  {
    --die ERROR IN TEST: When using the replication test framework (master-slave.inc, rpl_init.inc etc), use rpl_restart_server.inc instead of restart_mysqld.inc. If you know what you are doing and you really have to use restart_mysqld.inc, set allow_rpl_inited=1 before you source restart_mysqld.inc
  }
}

--let $_server_id = `SELECT @@server_id`
--echo include/stop_mysqld.inc [server $_server_id]
--let $_expect_file_name = $MYSQLTEST_VARDIR/tmp/mysqld.$_server_id.expect
--exec echo "wait" > $_expect_file_name

--let $ss_timeout = 60
if ($shutdown_server_timeout)
{
  --let $ss_timeout = $shutdown_server_timeout
}

--let $timeout_multiplier = 6

if ($VALGRIND_TEST)
{
  --expr $ss_timeout = $ss_timeout * $timeout_multiplier
}

let $have_sparc = `select convert(@@version_compile_machine using latin1) IN ('sparc')`;
if ($have_sparc)
{
  --expr $ss_timeout = $ss_timeout * $timeout_multiplier
}
--shutdown_server $ss_timeout

--source include/wait_until_disconnected.inc
