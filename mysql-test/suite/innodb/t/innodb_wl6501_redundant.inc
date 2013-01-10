# =====================  purpose  =======================================
# The test verifies that the internal InnoDB 'TRUNCATE TABLE' statement
# is atomic. To a general table with redundant format, recovery works
# fine from a crash during the truncation.
#

--disable_warnings
drop table if exists t1;
--enable_warnings

create table t1(a int not null,
 b int not null,
 c int not null,
 primary key (a),
 index (b),
 index (c))
engine=innodb row_format=redundant;

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
let $n=34000;
while ($n > 17000)
{
  eval insert into t1 values($n,$n,$n);
  dec $n;
}
-- enable_result_log
-- enable_query_log
select count(*) from t1;

-- echo # Verify that 'TRUNCATE TABLE' statement can empty a table normally.
truncate table t1;
select count(*) from t1;

-- echo # Verify that we can insert data into the truncated table normally.
-- disable_query_log
-- disable_result_log
let $n=17000;
while ($n > 0)
{
  eval insert into t1 values($n,$n,$n);
  dec $n;
}
-- enable_result_log
-- enable_query_log
select count(*) from t1;

drop table t1;

