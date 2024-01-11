#
# invoke commit that result in crash and then restart
#
--exec echo "restart " > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--error 2013
commit;
--echo "restarting after commit crash"
--source include/wait_until_connected_again.inc
