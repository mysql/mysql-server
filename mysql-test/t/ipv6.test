
# Test of ipv6 format
# Options: --skip-name-resolve (see corresponding opt file).
#
--source include/check_ipv6.inc
--source include/add_extra_root_users.inc

# Save the initial number of concurrent sessions
--source include/count_sessions.inc

echo =============Test of '::1' ========================================;
let $IPv6= ::1;
--source include/ipv6_clients.inc
--source include/ipv6.inc
--source include/ipv6_func.inc

echo =============Test of '127.0.0.1' (IPv4) ===========================;
let $IPv6= 127.0.0.1;
--source include/ipv6_clients.inc
--source include/ipv6.inc
--source include/ipv6_func.inc

echo =============Test of '::1/128' ====================================;
let $IPv6= ::1/128;
#--source include/ipv6_clients.inc
#--source include/ipv6.inc

echo =============Test of '0000:0000:0000:0000:0000:0000:0000:0001' ====;
let $IPv6= 0000:0000:0000:0000:0000:0000:0000:0001;
--source include/ipv6_clients.inc
--source include/ipv6.inc
--source include/ipv6_func.inc

echo =============Test of '0:0:0:0:0:0:0:1' ============================;
let $IPv6= 0:0:0:0:0:0:0:1;
--source include/ipv6_clients.inc
--source include/ipv6.inc
--source include/ipv6_func.inc

--source include/remove_extra_root_users.inc
# Wait till all disconnects are completed
--source include/wait_until_count_sessions.inc
