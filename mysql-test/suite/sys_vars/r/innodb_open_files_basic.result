'#---------------------BS_STVARS_038_01----------------------#'
SELECT COUNT(@@GLOBAL.innodb_open_files);
COUNT(@@GLOBAL.innodb_open_files)
1
1 Expected
'#---------------------BS_STVARS_038_02----------------------#'
SET @@GLOBAL.innodb_open_files=1;
ERROR HY000: Variable 'innodb_open_files' is a read only variable
Expected error 'Read only variable'
SELECT COUNT(@@GLOBAL.innodb_open_files);
COUNT(@@GLOBAL.innodb_open_files)
1
1 Expected
'#---------------------BS_STVARS_038_03----------------------#'
SELECT @@GLOBAL.innodb_open_files = VARIABLE_VALUE
FROM performance_schema.global_variables
WHERE VARIABLE_NAME='innodb_open_files';
@@GLOBAL.innodb_open_files = VARIABLE_VALUE
1
1 Expected
SELECT COUNT(@@GLOBAL.innodb_open_files);
COUNT(@@GLOBAL.innodb_open_files)
1
1 Expected
SELECT COUNT(VARIABLE_VALUE)
FROM performance_schema.global_variables 
WHERE VARIABLE_NAME='innodb_open_files';
COUNT(VARIABLE_VALUE)
1
1 Expected
'#---------------------BS_STVARS_038_04----------------------#'
SELECT @@innodb_open_files = @@GLOBAL.innodb_open_files;
@@innodb_open_files = @@GLOBAL.innodb_open_files
1
1 Expected
'#---------------------BS_STVARS_038_05----------------------#'
SELECT COUNT(@@innodb_open_files);
COUNT(@@innodb_open_files)
1
1 Expected
SELECT COUNT(@@local.innodb_open_files);
ERROR HY000: Variable 'innodb_open_files' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@SESSION.innodb_open_files);
ERROR HY000: Variable 'innodb_open_files' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@GLOBAL.innodb_open_files);
COUNT(@@GLOBAL.innodb_open_files)
1
1 Expected
SELECT innodb_open_files = @@SESSION.innodb_open_files;
ERROR 42S22: Unknown column 'innodb_open_files' in 'field list'
Expected error 'Readonly variable'
