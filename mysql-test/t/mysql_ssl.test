#Want to skip this test from daily Valgrind execution
--source include/no_valgrind_without_big.inc

# Save the initial number of concurrent sessions
--source include/count_sessions.inc

--source include/suppress_tls_off.inc

--echo #
--echo # WL#9091: Remove --ssl, --ssl-verify-server-cert client-side options
--echo #

--error 2
--exec $MYSQL --ssl
--error 2
--exec $MYSQL --skip-ssl
--error 2
--exec $MYSQL --ssl-verify-server-cert

--echo End of 8.0 tests

# Wait till all disconnects are completed
--source include/wait_until_count_sessions.inc

##  This test file is for testing encrypted communication only, not other
##  encryption routines that the SSL library happens to provide!
