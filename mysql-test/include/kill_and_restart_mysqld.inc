# Kills and restarts the current Mysql Server. Will wait for the server to
# disconnect the current session.
#
# Usage:
# --let $restart_parameters = restart_string
#    Value with Server parameters to be stored in the .expect file.
# --let $no_wait_for_reconnect = [0|1]
#    If not set or set to 0 it will cause the script to wait for the new Server
#    instance to start accepting connections and for the connection to be
#    connected again. If the flag is set, it will be reset to 0 when this script
#    exits, so the next calls to this script will by default wait for the
#    connection to be reconnected. If the flag is set, the
#    $do_not_wait_for_x_interface has no effect.

if (!$restart_parameters)
{
  let $restart_parameters = restart;
}

--let $_server_id= `SELECT @@server_id`
--let $_expect_file_name= $MYSQLTEST_VARDIR/tmp/mysqld.$_server_id.expect

--exec echo "# Kill and $restart_parameters"
--exec echo "$restart_parameters" > $_expect_file_name
--shutdown_server 0
--source include/wait_until_disconnected.inc

if (!$no_wait_for_reconnect)
{
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
}
--let $no_wait_for_reconnect = 0
