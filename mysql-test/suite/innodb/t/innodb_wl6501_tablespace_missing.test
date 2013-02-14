# =====================  purpose  ============================================
# The test verifies that the TRUNCATE TABLE statement works fine if
# the tablespace is missing.
#

CREATE DATABASE wl6501;
create table wl6501.t1(a int not null,
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
  eval insert into wl6501.t1 values($n,$n,$n);
  dec $n;
}
-- enable_result_log
-- enable_query_log
select count(*) from wl6501.t1;

-- echo # Write file to make mysql-test-run.pl expect crash and restart
-- exec echo "wait" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
-- echo # Run the crashing query
-- error 2013
truncate table wl6501.t1;

-- enable_reconnect
-- echo # Restart the MySQL server
-- exec echo "restart" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
-- source include/wait_until_connected_again.inc
-- disable_reconnect

-- disable_query_log
-- disable_result_log
let $n=100;
while ($n)
{
  eval insert into wl6501.t1 values($n,$n,$n);
  dec $n;
}
-- enable_result_log
-- enable_query_log
select count(*) from wl6501.t1;

drop table wl6501.t1;
drop database wl6501;

