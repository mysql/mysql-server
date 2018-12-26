SET @global_start_value = @@global.innodb_autoinc_lock_mode;
SELECT @global_start_value;
@global_start_value
2
'#--------------------FN_DYNVARS_046_01------------------------#'
SET @@global.innodb_autoinc_lock_mode = 0;
ERROR HY000: Variable 'innodb_autoinc_lock_mode' is a read only variable
SET @@global.innodb_autoinc_lock_mode = DEFAULT;
ERROR HY000: Variable 'innodb_autoinc_lock_mode' is a read only variable
SELECT @@global.innodb_autoinc_lock_mode;
@@global.innodb_autoinc_lock_mode
2
'#---------------------FN_DYNVARS_046_02-------------------------#'
SELECT @@innodb_autoinc_lock_mode;
@@innodb_autoinc_lock_mode
2
SELECT local.innodb_autoinc_lock_mode;
ERROR 42S02: Unknown table 'local' in field list
SELECT @@global.innodb_autoinc_lock_mode;
@@global.innodb_autoinc_lock_mode
2
'#----------------------FN_DYNVARS_046_03------------------------#'
SELECT @@global.innodb_autoinc_lock_mode = VARIABLE_VALUE 
FROM performance_schema.global_variables 
WHERE VARIABLE_NAME='innodb_autoinc_lock_mode';
@@global.innodb_autoinc_lock_mode = VARIABLE_VALUE
1
SELECT @@global.innodb_autoinc_lock_mode;
@@global.innodb_autoinc_lock_mode
2
SELECT VARIABLE_VALUE 
FROM performance_schema.global_variables 
WHERE VARIABLE_NAME='innodb_autoinc_lock_mode';
VARIABLE_VALUE
2
