#
# Bug#28268680: REGRESSION: SLOW_QUERIES STATUS VARIABLE ISN'T
#               INCREMENTED IF SLOW QUERY IS OFF
#
SELECT @@global.slow_query_log INTO @save_sql;
# enable slow query log
SET @@global.slow_query_log=1;
# Should be none yet.
SHOW SESSION STATUS LIKE 'Slow_queries';
Variable_name	Value
Slow_queries	0
SELECT variable_name,variable_value
FROM performance_schema.session_status
WHERE variable_name = "Slow_queries";
variable_name	variable_value
Slow_queries	0
SET SESSION long_query_time=0;
# expect "1"
SELECT variable_name,variable_value
FROM performance_schema.session_status
WHERE variable_name = "Slow_queries";
variable_name	variable_value
Slow_queries	1
# expect "2"
SHOW SESSION STATUS LIKE 'Slow_queries';
Variable_name	Value
Slow_queries	2
# disable slow query log. slow queries should still be counted!
SET @@global.slow_query_log=0;
# expect "4"
SELECT variable_name,variable_value
FROM performance_schema.session_status
WHERE variable_name = "Slow_queries";
variable_name	variable_value
Slow_queries	4
# expect "5"
SHOW SESSION STATUS LIKE 'Slow_queries';
Variable_name	Value
Slow_queries	5
SET SESSION long_query_time=DEFAULT;
SET @@global.slow_query_log=@save_sql;
