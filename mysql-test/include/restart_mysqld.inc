# ==== Purpose ====
#
# Shut down mysqld, and then start it again.
#
# Note: this waits for the current connection to reconnect to mysqld,
# but does not wait for other connections. If your test uses multiple
# connections, you must also use include/wait_until_disconnected.inc
# on all other connections.
#
# In replication tests, which use include/master-slave.inc or
# include/rpl_init.inc, it is recommended to use
# rpl_restart_server.inc instead.
#
# ==== Usage ====
#
# [--let $restart_parameters = COMMAND-LINE-OPTIONS]
# [--let $mysqld_name = mysqld.N]
# [--let $shutdown_server_timeout = N]
# [--let $do_not_echo_parameters = 1]
# --source include/restart_mysqld.inc
#
# Parameters:
#
#   $restart_parameters
#     Command line arguments to give to mysqld.
#
#   $mysqld_name
#     By default, uses the mysqld of the current connection.
#     To shut down another one, set $mysqld_name.
#     Note: in replication tests is is usually better to use
#     rpl_restart_server.inc instead, since it waits for all client
#     sessions to reconnect.
#
#   $shutdown_server_timeout
#     Fail the test if shutdown takes more than this number of seconds.
#     (6 times more if Valgrind is enabled). The default is 60.
#
#   $do_not_echo_parameters
#     By default, $restart_parameters is echoed to the result log.
#     Suppress that by setting this variable to 1.
#     (You can also use "replace_result" to mask parameters.)

if ($rpl_inited)
{
  if (!$allow_rpl_inited)
  {
    --die ERROR IN TEST: When using the replication test framework (master-slave.inc, rpl_init.inc etc), use rpl_restart_server.inc instead of restart_mysqld.inc. If you know what you are doing and you really have to use restart_mysqld.inc, set allow_rpl_inited=1 before you source restart_mysqld.inc
  }
}

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
--shutdown_server $ss_timeout

--source include/wait_until_disconnected.inc

# Write file to make mysql-test-run.pl start up the server again
--exec echo "$restart_parameters" > $_expect_file_name

# Turn on reconnect
--enable_reconnect

# Call script that will poll the server waiting for it to be back online again
--source include/wait_until_connected_again.inc

## Script xplugin_wait_for_interfaces.inc
#
#  $do_not_wait_for_x_interface -
#    set for which interfaces the script shouldn't wait
#    In default the script wait for both socket and tcp
#    on *NIX OS-es and only for TCP on windows
#
## Usage
#
# --let $wait_for_tcpsocket_status = valid_value|undefined_value|no_wait
# --let $wait_for_unixsocket_status = valid_value|undefined_value|no_wait
# --source include/xplugin_wait_for_interfaces.inc
#

--let $_xplugin_active= `SELECT /*+SET_VAR(use_secondary_engine=OFF)*/ plugin_status='ACTIVE' FROM information_schema.plugins WHERE plugin_name LIKE 'mysqlx'`
if ($_xplugin_active)
{
  --source include/xplugin_wait_for_interfaces.inc
}
# Turn off reconnect again
--disable_reconnect

