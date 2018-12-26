--let $_server_id= `SELECT @@server_id`
--let $_expect_file_name= $MYSQLTEST_VARDIR/tmp/mysqld.$_server_id.expect

--echo # Kill the server
--exec echo "wait" > $_expect_file_name
--shutdown_server 0
--source include/wait_until_disconnected.inc
