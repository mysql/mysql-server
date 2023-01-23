--echo #
--echo # WL#13470: Add X509 certificate sanity checks in server
--echo #


--disable_query_log
call mtr.add_suppression("Server certificate .* verification has failed. Check logs for more details");
call mtr.add_suppression("Failed to validate certificate .*");
--enable_query_log

--let $assert_text = "Check for TLS warning in the server log"
--let $assert_file = $MYSQLTEST_VARDIR/log/mysqld.1.err
--let $assert_select = certificate revoked
--let $assert_count = 1
--source include/assert_grep.inc

--echo # Restart server with defaults
--let $restart_parameters = restart:
--source include/restart_mysqld.inc

