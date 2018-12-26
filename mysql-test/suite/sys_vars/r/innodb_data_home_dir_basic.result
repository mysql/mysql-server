'#---------------------BS_STVARS_025_01----------------------#'
SELECT COUNT(@@GLOBAL.innodb_data_home_dir);
COUNT(@@GLOBAL.innodb_data_home_dir)
0
0 Expected
'#---------------------BS_STVARS_025_02----------------------#'
SET @@GLOBAL.innodb_data_home_dir=1;
ERROR HY000: Variable 'innodb_data_home_dir' is a read only variable
Expected error 'Read only variable'
SELECT COUNT(@@GLOBAL.innodb_data_home_dir);
COUNT(@@GLOBAL.innodb_data_home_dir)
0
0 Expected
'#---------------------BS_STVARS_025_03----------------------#'
SELECT @@GLOBAL.innodb_data_home_dir = VARIABLE_VALUE
FROM performance_schema.global_variables
WHERE VARIABLE_NAME='innodb_data_home_dir';
@@GLOBAL.innodb_data_home_dir = VARIABLE_VALUE
NULL
1 Expected
SELECT COUNT(@@GLOBAL.innodb_data_home_dir);
COUNT(@@GLOBAL.innodb_data_home_dir)
0
0 Expected
SELECT COUNT(VARIABLE_VALUE)
FROM performance_schema.global_variables 
WHERE VARIABLE_NAME='innodb_data_home_dir';
COUNT(VARIABLE_VALUE)
1
1 Expected
'#---------------------BS_STVARS_025_04----------------------#'
SELECT @@innodb_data_home_dir = @@GLOBAL.innodb_data_home_dir;
@@innodb_data_home_dir = @@GLOBAL.innodb_data_home_dir
NULL
1 Expected
'#---------------------BS_STVARS_025_05----------------------#'
SELECT COUNT(@@innodb_data_home_dir);
COUNT(@@innodb_data_home_dir)
0
0 Expected
SELECT COUNT(@@local.innodb_data_home_dir);
ERROR HY000: Variable 'innodb_data_home_dir' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@SESSION.innodb_data_home_dir);
ERROR HY000: Variable 'innodb_data_home_dir' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@GLOBAL.innodb_data_home_dir);
COUNT(@@GLOBAL.innodb_data_home_dir)
0
0 Expected
SELECT innodb_data_home_dir = @@SESSION.innodb_data_home_dir;
ERROR 42S22: Unknown column 'innodb_data_home_dir' in 'field list'
Expected error 'Readonly variable'
