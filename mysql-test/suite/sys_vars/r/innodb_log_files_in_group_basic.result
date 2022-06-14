'#---------------------BS_STVARS_034_01----------------------#'
SELECT COUNT(@@GLOBAL.innodb_log_files_in_group);
COUNT(@@GLOBAL.innodb_log_files_in_group)
1
1 Expected
'#---------------------BS_STVARS_034_02----------------------#'
SET @@GLOBAL.innodb_log_files_in_group=1;
ERROR HY000: Variable 'innodb_log_files_in_group' is a read only variable
Expected error 'Read only variable'
SELECT COUNT(@@GLOBAL.innodb_log_files_in_group);
COUNT(@@GLOBAL.innodb_log_files_in_group)
1
1 Expected
'#---------------------BS_STVARS_034_03----------------------#'
SELECT @@GLOBAL.innodb_log_files_in_group = VARIABLE_VALUE
FROM performance_schema.global_variables
WHERE VARIABLE_NAME='innodb_log_files_in_group';
@@GLOBAL.innodb_log_files_in_group = VARIABLE_VALUE
1
1 Expected
SELECT COUNT(@@GLOBAL.innodb_log_files_in_group);
COUNT(@@GLOBAL.innodb_log_files_in_group)
1
1 Expected
SELECT COUNT(VARIABLE_VALUE)
FROM performance_schema.global_variables 
WHERE VARIABLE_NAME='innodb_log_files_in_group';
COUNT(VARIABLE_VALUE)
1
1 Expected
'#---------------------BS_STVARS_034_04----------------------#'
SELECT @@innodb_log_files_in_group = @@GLOBAL.innodb_log_files_in_group;
@@innodb_log_files_in_group = @@GLOBAL.innodb_log_files_in_group
1
1 Expected
'#---------------------BS_STVARS_034_05----------------------#'
SELECT COUNT(@@innodb_log_files_in_group);
COUNT(@@innodb_log_files_in_group)
1
1 Expected
SELECT COUNT(@@local.innodb_log_files_in_group);
ERROR HY000: Variable 'innodb_log_files_in_group' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@SESSION.innodb_log_files_in_group);
ERROR HY000: Variable 'innodb_log_files_in_group' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@GLOBAL.innodb_log_files_in_group);
COUNT(@@GLOBAL.innodb_log_files_in_group)
1
1 Expected
SELECT innodb_log_files_in_group;
ERROR 42S22: Unknown column 'innodb_log_files_in_group' in 'field list'
Expected error 'Unknown column'
