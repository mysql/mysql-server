# Supported on platforms with UNIX sockets only
--source include/not_windows.inc

--echo #
--echo # Bug#28466137 MYSQLADMIN SHUTDOWN DOES NOT WAIT FOR MYSQL TO SHUT DOWN ANYMORE
--echo #

--let $_pid_file_location= `SELECT @@pid_file`

# Write file to make mtr expect the shutdown and avoid implicit restart
--let $_server_id= `SELECT @@server_id`
--let $_expect_file_name= $MYSQLTEST_VARDIR/tmp/mysqld.$_server_id.expect
--exec echo "wait" > $_expect_file_name

--exec $MYSQLADMIN -uroot --password="" -S $MASTER_MYSOCK shutdown 2>&1

# pid file should be gone
--error 1
--file_exists $_pid_file_location

--source include/wait_until_disconnected.inc

--exec echo "restart" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--source include/wait_until_connected_again.inc
