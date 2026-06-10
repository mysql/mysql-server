# Test that covers basic requirements for memory authentication.

# Create and configure xuser_* accounts that use native, sha256 and
# cache_sha2 authentication plugins.
--source ../include/create_mysql_accounts_for_xplugin_tests.inc

GRANT ALL ON *.* TO xuser_cache2@localhost;

#
## Test starts here
#

--echo
--echo #######################################################################
--echo ## $test_param_run.1. Memory mechanism ok after filling the cache, fails after flush
--echo ##
--echo ## when:
--echo ## * The cache is empty, the memory request is going to fail
--echo ## * plain auth is going to fill the cache (mysqlx_cache_cleaner installed)
--echo ## * The cache is filled, the memory auth request is going to be successful
--echo ## * FLUSH PRIVILEGES is going to clear the cache,
--echo ##   the memory auth request is going to fail
--echo ##
--echo ## Requirements: F2, F4, F5, CI6
--echo #

--echo
--echo #######################################################################
--echo # Cache is empty, the sha256_memory request must fail
exec $MYSQLXTEST $memory_login_fail 2>&1;

--echo
--echo #######################################################################
--echo # Plain auth work as expected and it fill the cache
exec $MYSQLXTEST $plain_login_ok 2>&1;

--echo
--echo #######################################################################
--echo # Plain filled the cache, thus auth-mech sha256_memory must be successful
exec $MYSQLXTEST $memory_login_ok 2>&1;

--echo
--echo #######################################################################
--echo # Cache is cleared by FLUSH SQL, thus auth-mech sha256_memory must fail
FLUSH PRIVILEGES;
exec $MYSQLXTEST $memory_login_fail 2>&1;

# Drop xuser_* account.
--source ../include/remove_mysql_accounts_for_xplugin_tests.inc
