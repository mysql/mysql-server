'#---------------------BS_STVARS_033_01----------------------#'
SELECT COUNT(@@GLOBAL.innodb_log_buffer_size);
COUNT(@@GLOBAL.innodb_log_buffer_size)
1
1 Expected
'#---------------------BS_STVARS_033_02----------------------#'
Invalid value
SET GLOBAL innodb_log_buffer_size = -1;
Warnings:
Warning	1292	Truncated incorrect innodb_log_buffer_size value: '-1'
SELECT @@innodb_log_buffer_size;
@@innodb_log_buffer_size
262144
Invalid value
SET GLOBAL innodb_log_buffer_size = 0;
Warnings:
Warning	1292	Truncated incorrect innodb_log_buffer_size value: '0'
SELECT @@innodb_log_buffer_size;
@@innodb_log_buffer_size
262144
Too small value
SET GLOBAL innodb_log_buffer_size = 1;
Warnings:
Warning	1292	Truncated incorrect innodb_log_buffer_size value: '1'
SELECT @@innodb_log_buffer_size;
@@innodb_log_buffer_size
262144
SELECT COUNT(@@GLOBAL.innodb_log_buffer_size);
COUNT(@@GLOBAL.innodb_log_buffer_size)
1
1 Expected
'#---------------------BS_STVARS_033_03----------------------#'
SELECT @@GLOBAL.innodb_log_buffer_size = VARIABLE_VALUE
FROM performance_schema.global_variables
WHERE VARIABLE_NAME='innodb_log_buffer_size';
@@GLOBAL.innodb_log_buffer_size = VARIABLE_VALUE
1
1 Expected
SELECT COUNT(@@GLOBAL.innodb_log_buffer_size);
COUNT(@@GLOBAL.innodb_log_buffer_size)
1
1 Expected
SELECT COUNT(VARIABLE_VALUE)
FROM performance_schema.global_variables 
WHERE VARIABLE_NAME='innodb_log_buffer_size';
COUNT(VARIABLE_VALUE)
1
1 Expected
'#---------------------BS_STVARS_033_04----------------------#'
SELECT @@innodb_log_buffer_size = @@GLOBAL.innodb_log_buffer_size;
@@innodb_log_buffer_size = @@GLOBAL.innodb_log_buffer_size
1
1 Expected
'#---------------------BS_STVARS_033_05----------------------#'
SELECT COUNT(@@innodb_log_buffer_size);
COUNT(@@innodb_log_buffer_size)
1
1 Expected
SELECT COUNT(@@local.innodb_log_buffer_size);
ERROR HY000: Variable 'innodb_log_buffer_size' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@SESSION.innodb_log_buffer_size);
ERROR HY000: Variable 'innodb_log_buffer_size' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@GLOBAL.innodb_log_buffer_size);
COUNT(@@GLOBAL.innodb_log_buffer_size)
1
1 Expected
SELECT innodb_log_buffer_size = @@SESSION.innodb_log_buffer_size;
ERROR 42S22: Unknown column 'innodb_log_buffer_size' in 'field list'
SELECT @@innodb_log_buffer_size;
@@innodb_log_buffer_size
262144
SET GLOBAL innodb_log_buffer_size = 1024*1024;
SELECT @@innodb_log_buffer_size;
@@innodb_log_buffer_size
1048576
SET GLOBAL innodb_log_buffer_size = 256*1024;
SELECT @@innodb_log_buffer_size;
@@innodb_log_buffer_size
262144
SET GLOBAL innodb_log_buffer_size = 2048*1024;
SELECT @@innodb_log_buffer_size;
@@innodb_log_buffer_size
2097152
SET @@GLOBAL.innodb_log_buffer_size=16777216;
SELECT @@innodb_log_buffer_size;
@@innodb_log_buffer_size
16777216
