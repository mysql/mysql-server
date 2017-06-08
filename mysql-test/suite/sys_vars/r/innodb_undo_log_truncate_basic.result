SET @start_global_value = @@global.innodb_undo_log_truncate;
SELECT @start_global_value;
@start_global_value
1
'#---------------------BS_STVARS_028_01----------------------#'
SELECT COUNT(@@GLOBAL.innodb_undo_log_truncate);
COUNT(@@GLOBAL.innodb_undo_log_truncate)
1
1 Expected
'#---------------------BS_STVARS_028_02----------------------#'
SET @@global.innodb_undo_log_truncate = 0;
SELECT @@global.innodb_undo_log_truncate;
@@global.innodb_undo_log_truncate
0
SET @@global.innodb_undo_log_truncate ='On' ;
SELECT @@global.innodb_undo_log_truncate;
@@global.innodb_undo_log_truncate
1
SET @@global.innodb_undo_log_truncate ='Off' ;
SELECT @@global.innodb_undo_log_truncate;
@@global.innodb_undo_log_truncate
0
SET @@global.innodb_undo_log_truncate = 1;
SELECT @@global.innodb_undo_log_truncate;
@@global.innodb_undo_log_truncate
1
'#---------------------BS_STVARS_028_03----------------------#'
SELECT IF(@@GLOBAL.innodb_undo_log_truncate,'ON','OFF') = VARIABLE_VALUE
FROM performance_schema.global_variables
WHERE VARIABLE_NAME='innodb_undo_log_truncate';
IF(@@GLOBAL.innodb_undo_log_truncate,'ON','OFF') = VARIABLE_VALUE
1
1 Expected
SELECT COUNT(@@GLOBAL.innodb_undo_log_truncate);
COUNT(@@GLOBAL.innodb_undo_log_truncate)
1
1 Expected
SELECT COUNT(VARIABLE_VALUE)
FROM performance_schema.global_variables
WHERE VARIABLE_NAME='innodb_undo_log_truncate';
COUNT(VARIABLE_VALUE)
1
1 Expected
'#---------------------BS_STVARS_028_04----------------------#'
SELECT @@innodb_undo_log_truncate = @@GLOBAL.innodb_undo_log_truncate;
@@innodb_undo_log_truncate = @@GLOBAL.innodb_undo_log_truncate
1
1 Expected
'#---------------------BS_STVARS_028_05----------------------#'
SELECT COUNT(@@innodb_undo_log_truncate);
COUNT(@@innodb_undo_log_truncate)
1
1 Expected
SELECT COUNT(@@local.innodb_undo_log_truncate);
ERROR HY000: Variable 'innodb_undo_log_truncate' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@SESSION.innodb_undo_log_truncate);
ERROR HY000: Variable 'innodb_undo_log_truncate' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@GLOBAL.innodb_undo_log_truncate);
COUNT(@@GLOBAL.innodb_undo_log_truncate)
1
1 Expected
SELECT innodb_undo_log_truncate = @@SESSION.innodb_undo_log_truncate;
ERROR 42S22: Unknown column 'innodb_undo_log_truncate' in 'field list'
SET @@global.innodb_undo_log_truncate = @start_global_value;
SELECT @@global.innodb_undo_log_truncate;
@@global.innodb_undo_log_truncate
1
