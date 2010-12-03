
# Write file to make mysql-test-run.pl expect the "crash", but don't start
# it until it's told to
--exec echo "wait" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect

# Send shutdown to the connected server and give
# it 10 seconds to die before zapping it
shutdown_server 10;

# Write file to make mysql-test-run.pl start up the server again
--exec echo "restart" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect

# Turn on reconnect
--enable_reconnect

# Call script that will poll the server waiting for it to be back online again
--source include/wait_until_connected_again.inc

# Turn off reconnect again
--disable_reconnect

