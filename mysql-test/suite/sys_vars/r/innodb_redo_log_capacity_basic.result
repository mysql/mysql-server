SET @start_log_capacity = @@GLOBAL.innodb_redo_log_capacity;
'#---------------------BS_STVARS_022_01----------------------#'
SELECT COUNT(@@GLOBAL.innodb_redo_log_capacity);
COUNT(@@GLOBAL.innodb_redo_log_capacity)
1
1 Expected
'#---------------------BS_STVARS_022_02----------------------#'
SET @@GLOBAL.innodb_redo_log_capacity=15728640;
Expected succeeded
SELECT COUNT(@@GLOBAL.innodb_redo_log_capacity);
COUNT(@@GLOBAL.innodb_redo_log_capacity)
1
1 Expected
'#---------------------BS_STVARS_022_03----------------------#'
SELECT @@GLOBAL.innodb_redo_log_capacity = VARIABLE_VALUE
FROM performance_schema.global_variables
WHERE VARIABLE_NAME='innodb_redo_log_capacity';
@@GLOBAL.innodb_redo_log_capacity = VARIABLE_VALUE
1
1 Expected
SELECT COUNT(@@GLOBAL.innodb_redo_log_capacity);
COUNT(@@GLOBAL.innodb_redo_log_capacity)
1
1 Expected
SELECT COUNT(VARIABLE_VALUE)
FROM performance_schema.global_variables
WHERE VARIABLE_NAME='innodb_redo_log_capacity';
COUNT(VARIABLE_VALUE)
1
1 Expected
'#---------------------BS_STVARS_022_04----------------------#'
SELECT @@innodb_redo_log_capacity = @@GLOBAL.innodb_redo_log_capacity;
@@innodb_redo_log_capacity = @@GLOBAL.innodb_redo_log_capacity
1
1 Expected
'#---------------------BS_STVARS_022_05----------------------#'
SELECT COUNT(@@innodb_redo_log_capacity);
COUNT(@@innodb_redo_log_capacity)
1
1 Expected
SELECT COUNT(@@local.innodb_redo_log_capacity);
ERROR HY000: Variable 'innodb_redo_log_capacity' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@SESSION.innodb_redo_log_capacity);
ERROR HY000: Variable 'innodb_redo_log_capacity' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@GLOBAL.innodb_redo_log_capacity);
COUNT(@@GLOBAL.innodb_redo_log_capacity)
1
1 Expected
SELECT innodb_redo_log_capacity;
ERROR 42S22: Unknown column 'innodb_redo_log_capacity' in 'field list'
Expected error 'Readonly variable'
SET @@GLOBAL.innodb_redo_log_capacity = @start_log_capacity;
Checking if total size of redo log files does not exceed 10485760 bytes
