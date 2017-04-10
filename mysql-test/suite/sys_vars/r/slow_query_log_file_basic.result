SET @start_value = @@global.slow_query_log_file;
'#---------------------FN_DYNVARS_004_01-------------------------#'
SET @@global.slow_query_log_file = DEFAULT;
SET @a=concat(left(@@hostname, instr(concat(@@hostname, '.'), '.')-1), '-slow.log');
SELECT RIGHT(@@global.slow_query_log_file, length(@a)) = @a;
RIGHT(@@global.slow_query_log_file, length(@a)) = @a
1
'#--------------------FN_DYNVARS_004_02------------------------#'
SET @@global.slow_query_log_file = mytest.log;
ERROR 42000: Incorrect argument type to variable 'slow_query_log_file'
SET @@global.slow_query_log_file = 12;
ERROR 42000: Incorrect argument type to variable 'slow_query_log_file'
'#----------------------FN_DYNVARS_004_03------------------------#'
SELECT @@global.slow_query_log_file = VARIABLE_VALUE 
FROM performance_schema.global_variables 
WHERE VARIABLE_NAME='slow_query_log_file';
@@global.slow_query_log_file = VARIABLE_VALUE
1
SET @@global.slow_query_log_file= @start_value;
