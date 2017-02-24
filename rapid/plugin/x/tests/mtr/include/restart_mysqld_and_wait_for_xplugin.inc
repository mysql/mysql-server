## Script restart_mysqld_and_wait_for_xplugin.inc
#
## Usage
#
# [--let $restart_parameters = restart: ...;]
# --let $wait_for_tcpsocket_status = valid_value|undefined_value|no_wait
# --let $wait_for_unixsocket_status = valid_value|undefined_value|no_wait
# --source ../include/restart_mysqld_and_wait_for_xplugin.inc
#

--source include/restart_mysqld.inc
--source ../include/xplugin_wait_for_interfaces.inc
