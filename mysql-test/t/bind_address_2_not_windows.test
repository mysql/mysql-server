
# WL#6301: Change default value for the 'bind-address' option
#
# 2. Check that if the server is started with bind-address=0.0.0.0,
# the server accepts client connections on all server host IPv4,
# but not IPv6 interfaces.
#
# Options: --skip-name-resolve --bind-address=0.0.0.0 (see corresponding opt file).
#

--let BIND_ADDRESS_LOG_FILE = $MYSQLTEST_VARDIR/log/bind_address_2_not_windows.debug.log

--source include/add_extra_root_users.inc

--source include/bind_address.inc

--source include/remove_extra_root_users.inc
