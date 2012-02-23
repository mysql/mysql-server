#
# few innodb tests depend on innodb tablespace being completelty clean and new
#
# this file deletes old innodb files and restarts mysqld
#
source include/not_embedded.inc;

let $_server_id= `SELECT @@server_id`;
let $_expect_file_name= $MYSQLTEST_VARDIR/tmp/mysqld.$_server_id.expect;
let $datadir= `SELECT @@datadir`;
exec echo "wait" > $_expect_file_name;
shutdown_server;
remove_file $datadir/ib_logfile0;
remove_file $datadir/ib_logfile1;
remove_file $datadir/ibdata1;
exec echo "restart" > $_expect_file_name;
enable_reconnect;
source include/wait_until_connected_again.inc;
disable_reconnect;

