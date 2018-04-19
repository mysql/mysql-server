'#---------------------BS_STVARS_036_01----------------------#'
SELECT COUNT(@@GLOBAL.innodb_log_group_home_dir);
COUNT(@@GLOBAL.innodb_log_group_home_dir)
1
1 Expected
'#---------------------BS_STVARS_036_02----------------------#'
SET @@GLOBAL.innodb_log_group_home_dir=1;
ERROR HY000: Variable 'innodb_log_group_home_dir' is a read only variable
Expected error 'Read only variable'
SELECT COUNT(@@GLOBAL.innodb_log_group_home_dir);
COUNT(@@GLOBAL.innodb_log_group_home_dir)
1
1 Expected
'#---------------------BS_STVARS_036_03----------------------#'
SELECT @@GLOBAL.innodb_log_group_home_dir = VARIABLE_VALUE
FROM performance_schema.global_variables
WHERE VARIABLE_NAME='innodb_log_group_home_dir';
@@GLOBAL.innodb_log_group_home_dir = VARIABLE_VALUE
1
1 Expected
SELECT COUNT(@@GLOBAL.innodb_log_group_home_dir);
COUNT(@@GLOBAL.innodb_log_group_home_dir)
1
1 Expected
SELECT COUNT(VARIABLE_VALUE)
FROM performance_schema.global_variables 
WHERE VARIABLE_NAME='innodb_log_group_home_dir';
COUNT(VARIABLE_VALUE)
1
1 Expected
'#---------------------BS_STVARS_036_04----------------------#'
SELECT @@innodb_log_group_home_dir = @@GLOBAL.innodb_log_group_home_dir;
@@innodb_log_group_home_dir = @@GLOBAL.innodb_log_group_home_dir
1
1 Expected
'#---------------------BS_STVARS_036_05----------------------#'
SELECT COUNT(@@innodb_log_group_home_dir);
COUNT(@@innodb_log_group_home_dir)
1
1 Expected
SELECT COUNT(@@local.innodb_log_group_home_dir);
ERROR HY000: Variable 'innodb_log_group_home_dir' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@SESSION.innodb_log_group_home_dir);
ERROR HY000: Variable 'innodb_log_group_home_dir' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@GLOBAL.innodb_log_group_home_dir);
COUNT(@@GLOBAL.innodb_log_group_home_dir)
1
1 Expected
SELECT innodb_log_group_home_dir = @@SESSION.innodb_log_group_home_dir;
ERROR 42S22: Unknown column 'innodb_log_group_home_dir' in 'field list'
Expected error 'Readonly variable'
