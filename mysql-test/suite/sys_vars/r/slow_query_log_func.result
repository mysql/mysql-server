SET @global_slow_query_log = @@global.slow_query_log;
SET @global_log_output = @@global.log_output;
SET @@session.long_query_time=1;
SET @@global.log_output = 'TABLE';
'----When slow_query_log = OFF----'
SET @@global.slow_query_log = OFF;
TRUNCATE mysql.slow_log;
SELECT sleep(2);
sleep(2)
0
SELECT count(*) FROM mysql.slow_log;
count(*)
0
'----When slow_query_log = ON-----'
SET @@global.slow_query_log = ON;
TRUNCATE mysql.slow_log;
SELECT sleep(2) AS bug53630;
bug53630
0
SELECT count(*) > 0 FROM mysql.slow_log WHERE sql_text LIKE '% bug53630';
count(*) > 0
1
SELECT thread_id - connection_id() FROM mysql.slow_log
WHERE sql_text LIKE '% bug53630'
                                   ORDER BY start_time DESC LIMIT 1;
thread_id - connection_id()
0
'Bug#47905 stored procedures not logged correctly to slow query log'
TRUNCATE mysql.slow_log;
CREATE PROCEDURE p_test() 
BEGIN
select sleep(2);
select 1;
END//
CALL p_test();
sleep(2)
0
1
1
SELECT count(*) > 0 FROM mysql.slow_log;
count(*) > 0
1
DROP PROCEDURE p_test;
SET sql_mode = default;
SET @@global.log_output = @global_log_output;
SET @global.slow_query_log = @global_slow_query_log;
TRUNCATE TABLE mysql.general_log;
TRUNCATE TABLE mysql.slow_log;
