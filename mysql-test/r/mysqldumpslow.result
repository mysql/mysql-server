SET @old_log_output=          @@global.log_output;
SET @old_slow_query_log=      @@global.slow_query_log;
SET @old_slow_query_log_file= @@global.slow_query_log_file;
SET GLOBAL slow_query_log_file= '.../log/slow_query_temp.log';
SET GLOBAL log_output=       'FILE';
SET GLOBAL slow_query_log=   'ON';
SET SESSION long_query_time=  0;
SHOW VARIABLES LIKE "%long_query_time%";
Variable_name	Value
long_query_time	0.000000
SHOW VARIABLES LIKE "%long_query_time%";
Variable_name	Value
long_query_time	0.000000
SHOW VARIABLES LIKE "%long_query_time%";
Variable_name	Value
long_query_time	0.000000
SHOW VARIABLES LIKE "%long_query_time%";
Variable_name	Value
long_query_time	0.000000
SHOW VARIABLES LIKE "%long_query_time%";
Variable_name	Value
long_query_time	0.000000
SELECT 1;
1
1
SELECT 1;
1
1
SELECT 1;
1
1
SELECT 1;
1
1
"Running mysqldumpslow on the slow-query-log"
Count: 5    SHOW VARIABLES LIKE "%long_query_time%"

Count: 4    SELECT 1

Count: 1    SET SESSION long_query_time=  0

SET GLOBAL  log_output=         @old_log_output;
SET GLOBAL  slow_query_log=     @old_slow_query_log;
SET GLOBAL  slow_query_log_file=@old_slow_query_log_file;
End of test!
