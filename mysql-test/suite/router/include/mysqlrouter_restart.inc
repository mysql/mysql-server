# ==== Purpose ====
#
# Shut down mysqlrouter, and then start it again.
#
# Note: this waits for the current connection to reconnect to router,
# but does not wait for other connections. If your test uses multiple
# connections, you must also use include/wait_until_disconnected.inc
# on all other connections.
#
# ==== Usage ====
#
# [--let $restart_parameters = COMMAND-LINE-OPTIONS]
# [--let $shutdown_server_timeout = N]
# [--let $do_not_echo_parameters = 1]
# --source include/mysqlrouter_restart.inc
#
# Parameters:
#
#   $restart_parameters
#     Command line arguments to give to mysqlrouter.
#
#
#   $shutdown_server_timeout
#     Fail the test if shutdown takes more than this number of seconds.
#     (6 times more if Valgrind is enabled). The default is 60.
#
#   $do_not_echo_parameters
#     By default, $restart_parameters is echoed to the result log.
#     Suppress that by setting this variable to 1.
#     (You can also use "replace_result" to mask parameters.)


if (!$restart_parameters)
{
  let $restart_parameters = restart;
}

# We use --exec echo instead of simple "--echo" because
# replace_result works on --exec
if (!$do_not_echo_parameters) {
  --exec echo "# $restart_parameters"
}
if ($do_not_echo_parameters) {
  --exec echo "# restart"
}

# Write file to make mysql-test-run.pl expect the "crash", but don't start
# it until it's told to
--let $_expect_file_name= $MYSQLTEST_VARDIR/tmp/mysqlrouter.expect

--exec echo "wait" > $_expect_file_name

# Send shutdown to the connected server.

--let $ss_timeout= 60

if ($shutdown_server_timeout)
{
  --let $ss_timeout= $shutdown_server_timeout
}

--let $timeout_multiplier= 6

if ($VALGRIND_TEST)
{
  --expr $ss_timeout= $ss_timeout * $timeout_multiplier
}

let $have_sparc = `select convert(@@version_compile_machine using latin1) IN ('sparc')`;
if ($have_sparc)
{
  --expr $ss_timeout= $ss_timeout * $timeout_multiplier
}
--let $pid_file=$MYSQLROUTER_PIDFILE
--source mysqlrouter_shutdown.inc

--source include/wait_until_disconnected.inc

# Write file to make mysql-test-run.pl start up the router again
--exec echo "$restart_parameters" > $_expect_file_name

# Call script that will poll the router waiting for it to be back online again
--source include/wait_until_connected_again.inc
