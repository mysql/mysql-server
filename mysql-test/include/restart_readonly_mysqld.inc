# Change buffer must be empty before restarting server as read-only
# If the innodb_fast_shutdown=0, InnoDB does a full purge and an insert buffer
# merge before a shutdown
--disable_query_log
SET GLOBAL innodb_fast_shutdown=0;
--enable_query_log

# Write file to make mysql-test-run.pl expect the "crash", but don't start
# it until it's told to
--let $_server_id= `SELECT @@server_id`
--let $_expect_file_name= $MYSQLTEST_VARDIR/tmp/mysqld.$_server_id.expect
--exec echo "wait" > $_expect_file_name

# Send shutdown to the connected server and give
# it 10 seconds to die before zapping it
shutdown_server 10;

# Write file to make mysql-test-run.pl start up the server again
--exec echo "restart: --innodb-read-only " > $_expect_file_name

# Turn on reconnect
--enable_reconnect

# Call script that will poll the server waiting for it to be back online again
--source include/wait_until_connected_again.inc

# Turn off reconnect again
--disable_reconnect
