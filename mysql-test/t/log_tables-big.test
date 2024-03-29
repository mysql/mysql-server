# this test needs multithreaded mysqltest

# Test sleeps for long times
--source include/big_test.inc

set @@global.log_output = 'TABLE';

connect (con1,localhost,root,,);
connect (con2,localhost,root,,);

#
# Bug #27638: slow logging to CSV table inserts bad query_time and lock_time values
#
connection con1;
set session long_query_time=10;
select get_lock('bug27638', 1);
connection con2;
set session long_query_time=1;
select get_lock('bug27638', 2);
select if (query_time >= '00:00:01', 'OK', 'WRONG') as qt, sql_text from mysql.slow_log
       where sql_text = 'select get_lock(\'bug27638\', 2)';
select get_lock('bug27638', 60);
select if (query_time >= '00:00:59', 'OK', 'WRONG') as qt, sql_text from mysql.slow_log
       where sql_text = 'select get_lock(\'bug27638\', 60)';
select get_lock('bug27638', 101);
select if (query_time >= '00:01:40', 'OK', 'WRONG') as qt, sql_text from mysql.slow_log
       where sql_text = 'select get_lock(\'bug27638\', 101)';
connection con1;
select release_lock('bug27638');
connection default;

disconnect con1;
disconnect con2;

set @@global.log_output=default;

TRUNCATE mysql.general_log;
TRUNCATE mysql.slow_log;
