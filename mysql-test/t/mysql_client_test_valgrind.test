--echo #
--echo # Bug#32372038: SSL MEMORY LEAK IN THE VIO STRUCT ON CLEANUP
--echo #

--source include/have_valgrind.inc
--source include/have_debug.inc
# need to have the dynamic loading turned on for the client plugin tests
--source include/have_plugin_auth.inc

--exec echo "$MYSQL_CLIENT_TEST" > $MYSQLTEST_VARDIR/log/mysql_client_test_valgrind.out.log 2>&1
--exec $MYSQL_CLIENT_TEST --getopt-ll-test=25600M $PLUGIN_AUTH_CLIENT_OPT test_bug32372038 >> $MYSQLTEST_VARDIR/log/mysql_client_test_valgrind.out.log 2>&1
