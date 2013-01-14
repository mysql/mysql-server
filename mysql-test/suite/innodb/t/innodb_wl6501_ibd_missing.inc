# =====================  purpose  ============================================
# The test verifies that the TRUNCATE TABLE statement is rollbacked
# and the table is marked as corrupt when the .ibd file is missing.
#

--disable_warnings
drop table if exists tc;
--enable_warnings

create table tc(a int not null,
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
  eval insert into tc values($n,$n,$n);
  dec $n;
}
-- enable_result_log
-- enable_query_log
select count(*) from tc;

-- echo # Write file to make mysql-test-run.pl expect crash and restart
-- exec echo "wait" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
-- echo # Run the crashing query
#-- error 2013
--error ER_GET_ERRNO
truncate table tc;

-- enable_reconnect
-- echo # Restart the MySQL server
-- exec echo "restart" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
-- source include/wait_until_connected_again.inc
-- disable_reconnect

show create table tc;

drop table tc;

