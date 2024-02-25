--let $_server_id= `SELECT @@server_id`
--let $_expect_file_name= $MYSQLTEST_VARDIR/tmp/mysqld.$_server_id.expect

# There should be a debug crash after using this .inc file
--exec echo "wait" > $_expect_file_name
