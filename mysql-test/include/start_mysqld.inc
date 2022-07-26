if (!$restart_parameters)
{
  let $restart_parameters = restart;
}

# We use --exec echo instead of simple "--echo" because
# replace_result works on --exec
--exec echo "# $restart_parameters"

# Include this script only after using shutdown_mysqld.inc
# or kill_mysqld.inc or expect_crash.inc
# where $_expect_file_name was initialized.
# Write file to make mysql-test-run.pl start up the server again
--exec echo "$restart_parameters" > $_expect_file_name

# --let $expect_crash_on_start = [0|1]
#    Whether or not a crash during start should be expected. If set to `1`,
#    will wait for the expect file containing the `restart` parameters to
#    be processed and will set it to "wait".
#
if ($expect_crash_on_start == 1) {
  --let $expect_file_name = $_expect_file_name
  --source include/wait_for_removed_expect_file.inc
  --exec echo "wait" > $_expect_file_name
}

if ($expect_crash_on_start != 1) {
  # Turn on reconnect
  --enable_reconnect

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

  # Turn off reconnect again
  --disable_reconnect
}
