#
## Test cases for accounts created on local-scope IPv6
## addresses. If the account address doesn't exactly match
## the clients IP address (with scope specifier), such
## connection attempt must be rejected
#
# Usage
#
# --let $ip = IPv6 ADDRESS
# --let $interface = scope name/interface name of the IP
# --let $execute_command = program that is going to connect to MySQLd
# --source ../include/connection_local_scop.inc
#

--disable_query_log
--disable_result_log
#
# Please note that '%' is going to be interpreted as wildcard character.
# anyway it doesn't matter for the test-case
#
--eval CREATE USER user1@`$ip%$interface` identified with 'mysql_native_password' by 'p1'
--eval CREATE USER user2@`$ip` identified with 'mysql_native_password' by 'p2'
--enable_result_log
--enable_query_log

--let $accounts_host=/$ip%$interface/localhost/ /user1@[a-zA-Z0-9:._%-]+/user1@localhost/ /'user2'@'[^']+'/'user2'@'localhost'/

--echo # Account user1 was created with interface name.
--echo # The authentication must be succesful
--replace_regex $accounts_host
exec $execute_command -uuser1 --password='p1'
 -h$ip%$interface -e "SELECT USER()" 2>&1;

--echo # Account user2 was created without interface name.
--echo # The authentication must fail because account IP
--echo # and client IP doesn't match
--replace_regex $accounts_host
error 1;
exec $execute_command
 -uuser2 --password='p2'
 -h$ip%$interface -e "SELECT USER()" 2>&1;

--disable_query_log
--disable_result_log
--eval DROP USER user1@`$ip%$interface`
--eval DROP USER user2@`$ip`
--enable_result_log
--enable_query_log
