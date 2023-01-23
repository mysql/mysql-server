--echo #
--echo # WL#13470: Add X509 certificate sanity checks in server
--echo #


--disable_query_log
call mtr.add_suppression("Unable to get certificate");
call mtr.add_suppression('Failed to initialize TLS for channel: mysql_main');
call mtr.add_suppression("Server certificate .* verification has failed. Check logs for more details");
call mtr.add_suppression("CA certificate/certficates is invalid. Please check logs for more details.");
call mtr.add_suppression("Failed to validate certificate .*");
--enable_query_log

--let $assert_text = "Check for TLS warning in the server log"
--let $assert_file = $MYSQLTEST_VARDIR/log/mysqld.1.err
--let $assert_select =  Failed to validate certificate
--let $assert_count = 3
--source include/assert_grep.inc

--echo # Restart server with defaults
--let $restart_parameters = restart:
--source include/restart_mysqld.inc

