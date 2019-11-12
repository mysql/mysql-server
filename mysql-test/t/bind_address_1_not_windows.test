
# WL#6301: Change default value for the 'bind-address' option
#
# 1. Check that by default the server accepts client connections on all
# server host IPv4 and IPv6 interfaces.
#
# Options: --skip-name-resolve (see corresponding opt file).
#

--source include/check_ipv6.inc

--source include/add_extra_root_users.inc

--let BIND_ADDRESS_LOG_FILE = $MYSQLTEST_VARDIR/log/bind_address_1_not_windows.debug.log

--source include/bind_address.inc

--source include/remove_extra_root_users.inc
