## Script xplugin_reset_and_wait.inc
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
# --let $wait_for_tcpsocket_status = valid_value|undefined_value|no_wait
# --let $wait_for_unixsocket_status = valid_value|undefined_value|no_wait
# --let $dont_reset_global_status_variables = 1|0
# --source include/xplugin_reset_and_wait.inc
#

source xplugin_wait_for_interfaces.inc;
source xplugin_reset_global_status.inc;
