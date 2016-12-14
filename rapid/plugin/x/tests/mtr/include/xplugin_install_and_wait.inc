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
# --let $dont_load_plugin = 1|0
# --let $wait_for_tcpsocket_status = valid_value|undefined_value|no_wait
# --let $wait_for_unixsocket_status = valid_value|undefined_value|no_wait
# --source ../include/xplugin_install_and_wait.inc
#

if ( $dont_load_plugin != 1)
{
    --replace_regex /\.dll/.so/
    --eval install plugin mysqlx soname "$MYSQLXPLUGIN"
}

--source xplugin_wait_for_interfaces.inc

--let $dont_load_plugin=0
