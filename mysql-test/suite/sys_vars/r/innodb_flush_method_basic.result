'#---------------------BS_STVARS_029_01----------------------#'
SELECT @@GLOBAL.innodb_flush_method IN ('fsync', 'unbuffered');
@@GLOBAL.innodb_flush_method IN ('fsync', 'unbuffered')
1
1 Expected
'#---------------------BS_STVARS_029_02----------------------#'
SET @@GLOBAL.innodb_flush_method=1;
ERROR HY000: Variable 'innodb_flush_method' is a read only variable
Expected error 'Read only variable'
SELECT @@GLOBAL.innodb_flush_method IN ('fsync', 'unbuffered');
@@GLOBAL.innodb_flush_method IN ('fsync', 'unbuffered')
1
1 Expected
'#---------------------BS_STVARS_029_03----------------------#'
SELECT @@GLOBAL.innodb_flush_method = VARIABLE_VALUE
FROM performance_schema.global_variables
WHERE VARIABLE_NAME='innodb_flush_method';
@@GLOBAL.innodb_flush_method = VARIABLE_VALUE
1
1 Expected
SELECT @@GLOBAL.innodb_flush_method IN ('fsync', 'unbuffered');
@@GLOBAL.innodb_flush_method IN ('fsync', 'unbuffered')
1
1 Expected
SELECT COUNT(VARIABLE_VALUE)
FROM performance_schema.global_variables 
WHERE VARIABLE_NAME='innodb_flush_method';
COUNT(VARIABLE_VALUE)
1
1 Expected
'#---------------------BS_STVARS_029_04----------------------#'
SELECT @@innodb_flush_method = @@GLOBAL.innodb_flush_method;
@@innodb_flush_method = @@GLOBAL.innodb_flush_method
1
1 Expected
'#---------------------BS_STVARS_029_05----------------------#'
SELECT COUNT(@@innodb_flush_method);
COUNT(@@innodb_flush_method)
1
1 Expected
SELECT COUNT(@@local.innodb_flush_method);
ERROR HY000: Variable 'innodb_flush_method' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@SESSION.innodb_flush_method);
ERROR HY000: Variable 'innodb_flush_method' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@GLOBAL.innodb_flush_method);
COUNT(@@GLOBAL.innodb_flush_method)
1
1 Expected
SELECT innodb_flush_method = @@SESSION.innodb_flush_method;
ERROR 42S22: Unknown column 'innodb_flush_method' in 'field list'
Expected error 'Readonly variable'
