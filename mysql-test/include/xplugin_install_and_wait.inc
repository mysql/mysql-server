## Script xplugin_install_and_wait.inc
#
#  $wait_for_tcpsocket_status -
#    set if the application should wait for
#    X Plugin - tcp interface. If so then define which
#    value is expected: success - valid_value
#    or failure - undefined_value
#
#  $wait_for_unixsocket_status -
#    set if the application should wait for
#    X Plugin - UNIX socket interface. If so then define which
#    value is expected: success - valid_value
#    or failure - undefined_value
#
## Usage
#
# TODO(lkotula): The default value for dont_load_mysqlx_cleaner_plugin
#                must be changed or removed after resolving the problem
#                with mysqlx as static or dynamic and installing be default
# --let $dont_load_mysqlx_plugin = 1|0 (default 0)
# --let $dont_load_mysqlx_cleaner_plugin = 1|0 (default 1)
# --let $error_load_plugin = error_code
# --let $wait_for_tcpsocket_status = valid_value|undefined_value|no_wait
# --let $wait_for_unixsocket_status = valid_value|undefined_value|no_wait
# --source include/xplugin_install_and_wait.inc
#

if ($dont_load_mysqlx_cleaner_plugin == '')
{
  --let $dont_load_mysqlx_cleaner_plugin=1
}

if ( $dont_load_mysqlx_plugin == 1)
{
  --let $wait_for_tcpsocket_status = no_wait
  --let $wait_for_unixsocket_status = no_wait
}

if ( $dont_load_mysqlx_plugin != 1)
{
    --replace_regex /\.dll/.so/
    if ($error_load_plugin != "")
    {
        --echo # Expecting error: $error_load_plugin at X Plugin install.
        --error $error_load_plugin,0
    }
    --eval install plugin mysqlx soname "$MYSQLXPLUGIN"
}

if ($dont_load_mysqlx_cleaner_plugin != 1)
{
  --replace_result $MYSQLXPLUGIN MYSQLXPLUGIN
  --eval install plugin mysqlx_cache_cleaner soname "$MYSQLXPLUGIN"
}

--source xplugin_wait_for_interfaces.inc

--let $dont_load_mysqlx_plugin=
--let $dont_load_mysqlx_cleaner_plugin=
--let $error_load_plugin=
