'#________________________VAR_09_INNODB_LOG_FILE_SIZE__________________#'
echo '##'
--echo '#---------------------WL6372_VAR_9_01----------------------#'
####################################################################
#   Checking default value                                         #
####################################################################
SELECT COUNT(@@GLOBAL.innodb_log_file_size)
1 Expected
SELECT @@GLOBAL.innodb_log_file_size;
@@GLOBAL.innodb_log_file_size
5242880
5242880 Expected
'#---------------------WL6372_VAR_9_02----------------------#'
SET @@local.innodb_log_file_size=1;
ERROR HY000: Variable 'innodb_log_file_size' is a read only variable
Expected error 'Read only variable'
SET @@session.innodb_log_file_size=1;
ERROR HY000: Variable 'innodb_log_file_size' is a read only variable
Expected error 'Read only variable'
SET @@GLOBAL.innodb_log_file_size=1;
ERROR HY000: Variable 'innodb_log_file_size' is a read only variable
Expected error 'Read only variable'
SET @@GLOBAL.innodb_log_file_size=DEFAULT;
ERROR HY000: Variable 'innodb_log_file_size' is a read only variable
Expected error 'Read only variable'
SELECT COUNT(@@GLOBAL.innodb_log_file_size);
COUNT(@@GLOBAL.innodb_log_file_size)
1
1 Expected
'#---------------------WL6372_VAR_9_03----------------------#'
SELECT @@GLOBAL.innodb_log_file_size = VARIABLE_VALUE
FROM performance_schema.global_variables
WHERE VARIABLE_NAME='innodb_log_file_size';
@@GLOBAL.innodb_log_file_size = VARIABLE_VALUE
1
1 Expected
SELECT COUNT(@@GLOBAL.innodb_log_file_size);
COUNT(@@GLOBAL.innodb_log_file_size)
1
1 Expected
SELECT COUNT(VARIABLE_VALUE)
FROM performance_schema.global_variables 
WHERE VARIABLE_NAME='innodb_log_file_size';
COUNT(VARIABLE_VALUE)
1
1 Expected
'#---------------------WL6372_VAR_9_04----------------------#'
SELECT @@innodb_log_file_size = @@GLOBAL.innodb_log_file_size;
@@innodb_log_file_size = @@GLOBAL.innodb_log_file_size
1
1 Expected
SELECT COUNT(@@local.innodb_log_file_size);
ERROR HY000: Variable 'innodb_log_file_size' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@SESSION.innodb_log_file_size);
ERROR HY000: Variable 'innodb_log_file_size' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@GLOBAL.innodb_log_file_size);
COUNT(@@GLOBAL.innodb_log_file_size)
1
1 Expected
SELECT innodb_log_file_size = @@SESSION.innodb_log_file_size;
ERROR 42S22: Unknown column 'innodb_log_file_size' in 'field list'
Expected error 'Unknown column innodb_log_file_size in field list'
'#---------------------WL6372_VAR_9_05----------------------#'
Check the size of the vardir
The output size is in unit blocks
TRUE
TRUE Expected
'#---------------------WL6372_VAR_9_06----------------------#'
The size of the ib_logfile(0/1): 5242880
The size of the ib_logfile(0/1): 5242880
5242880 expected
