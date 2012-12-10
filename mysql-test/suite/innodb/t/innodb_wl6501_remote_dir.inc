# =====================  purpose  =======================================
# The test verifies that the internal InnoDB 'TRUNCATE TABLE' statement
# is atomic. To table with a remote data directory, recovery works fine
# from a crash during the truncation.
#

--disable_warnings
drop table if exists t1;
--enable_warnings
set global innodb_file_format='Barracuda';
--echo #
--echo # Create and insert records into a table
--echo # that uses a remote DATA DIRECTORY.
--echo #
--mkdir $MYSQL_TMP_DIR/alt_dir
--replace_result $MYSQL_TMP_DIR mysql_tmp_dir
eval create table t1(a int not null,
 b int not null,
 c int not null,
 primary key (a),
 index (b),
 index (c))
row_format=dynamic engine=innodb $data_directory;
-- disable_query_log
-- disable_result_log

let $n=17000;
while ($n)
{
  eval insert into t1 values($n,$n,$n);
  dec $n;
}
-- enable_result_log
-- enable_query_log
select count(*) from t1;

-- echo # Write file to make mysql-test-run.pl expect crash and restart
-- exec echo "wait" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
-- echo # Run the crashing query
-- error 2013
truncate table t1;

-- source include/wait_until_disconnected.inc
-- enable_reconnect
-- echo # Restart the MySQL server
-- exec echo "restart" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
-- source include/wait_until_connected_again.inc
-- disable_reconnect
select count(*) from t1;

-- disable_query_log
-- disable_result_log
let $n=20000;
while ($n)
{
  eval insert into t1 values($n,$n,$n);
  dec $n;
}
-- enable_result_log
-- enable_query_log
select count(*) from t1;

drop table t1;

--rmdir $MYSQL_TMP_DIR/alt_dir/test
--rmdir $MYSQL_TMP_DIR/alt_dir

