# =====================  purpose  ============================================
# The test verifies that the TRUNCATE TABLE statement is rollbacked
# and the table is marked as corrupt when the .ibd file is missing.
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
engine=innodb row_format=compact;

-- disable_query_log
-- disable_result_log
let $n=100;
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
#-- error 2013
--error ER_GET_ERRNO
truncate table t1;

-- enable_reconnect
-- echo # Restart the MySQL server
-- exec echo "restart" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
-- source include/wait_until_connected_again.inc
-- disable_reconnect

show create table t1;

drop table t1;

