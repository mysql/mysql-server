
# Write file to make mysql-test-run.pl expect crash and restart
--exec echo "restart" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect

--exec $MYSQLADMIN --defaults-file=$MYSQLTEST_VARDIR/my.cnf --shutdown-timeout=60 shutdown

# Call script that will poll the server waiting for it to be gone
--source include/wait_until_disconnected.inc

# Turn on reconnect
--enable_reconnect

# Call script that will poll the server waiting for it to be back online again
--source include/wait_until_connected_again.inc

