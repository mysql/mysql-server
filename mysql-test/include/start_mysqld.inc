if (!$restart_parameters)
{
  let $restart_parameters = restart;
}

# We use --exec echo instead of simple "--echo" because
# replace_result works on --exec
--exec echo "# $restart_parameters"

# A common bug in our tests was that people tried to call start_mysqld.inc
# while the expect file already existed and contained something other than "wait",
# i.e "restart", "restart_abort" etc.
# This was a race condition between server being restarted due to the inclusion
# of this script, and restart because the test runner notices something other
# than "wait".
# The proper way of using start_mysqld.inc is to call it when you know the
# server is not running - this is tricky, so as a compromise you should at
# least ensure you are not connected. How? You've used a script which waits
# for the server to shut down or at least disconnect your connection, or
# you've executed a query which triggered a fatal error and your client
# got disconnected (--error CR_SERVER_LOST, 2013).
# And you don't want the test runner to ressurect the server, so the expect
# file should exists and contain "wait" in it.
# If it doesn't exist, then the test runner will panic, that server went
# away and you didn't expect it. If it exists and has "restart" or
# "restart_abort" in it, then the test runner will restart the server for
# you, so you shouldn't attempt to also start it in parallel by using this
# script.
# Another, more rare bug was to try to use start_mysqld.inc when the expect
# file doesn't even exit. This implies that the server is up and running, too,
# as otherwise the test runner would panic not seeing the expect file.
# Trying to start the server while it is already started indicates a logic bug.
# Thus, we assert that the expect file exists and contains "wait" in it.

--let $_sm_old_wait_timeout=$wait_timeout
--let $wait_timeout=0
--source include/wait_for_wait_in_expect_file.inc
--let $wait_timeout=$_sm_old_wait_timeout

# Include this script only after using shutdown_mysqld.inc
# or kill_mysqld.inc or expect_crash.inc
# where $_expect_file_name was initialized.
# Write file to make mysql-test-run.pl start up the server again
--exec echo "$restart_parameters" > $_expect_file_name

  # Call script that will poll the server waiting for it to be back online again
  # Allow for slow machines like Sparc
  --let $wait_counter= 6000
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

  --let $_xplugin_active= `SELECT plugin_status='ACTIVE' FROM information_schema.plugins WHERE plugin_name LIKE 'mysqlx'`
  if ($_xplugin_active)
  {
    --source include/xplugin_wait_for_interfaces.inc
  }
