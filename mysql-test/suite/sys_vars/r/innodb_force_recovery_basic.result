'#---------------------BS_STVARS_030_01----------------------#'
SELECT COUNT(@@GLOBAL.innodb_force_recovery);
COUNT(@@GLOBAL.innodb_force_recovery)
1
1 Expected
'#---------------------BS_STVARS_030_02----------------------#'
SET @@GLOBAL.innodb_force_recovery=1;
ERROR HY000: Variable 'innodb_force_recovery' is a read only variable
Expected error 'Read only variable'
SELECT COUNT(@@GLOBAL.innodb_force_recovery);
COUNT(@@GLOBAL.innodb_force_recovery)
1
1 Expected
'#---------------------BS_STVARS_030_03----------------------#'
SELECT @@GLOBAL.innodb_force_recovery = VARIABLE_VALUE
FROM performance_schema.global_variables
WHERE VARIABLE_NAME='innodb_force_recovery';
@@GLOBAL.innodb_force_recovery = VARIABLE_VALUE
1
1 Expected
SELECT COUNT(@@GLOBAL.innodb_force_recovery);
COUNT(@@GLOBAL.innodb_force_recovery)
1
1 Expected
SELECT COUNT(VARIABLE_VALUE)
FROM performance_schema.global_variables 
WHERE VARIABLE_NAME='innodb_force_recovery';
COUNT(VARIABLE_VALUE)
1
1 Expected
'#---------------------BS_STVARS_030_04----------------------#'
SELECT @@innodb_force_recovery = @@GLOBAL.innodb_force_recovery;
@@innodb_force_recovery = @@GLOBAL.innodb_force_recovery
1
1 Expected
'#---------------------BS_STVARS_030_05----------------------#'
SELECT COUNT(@@innodb_force_recovery);
COUNT(@@innodb_force_recovery)
1
1 Expected
SELECT COUNT(@@local.innodb_force_recovery);
ERROR HY000: Variable 'innodb_force_recovery' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@SESSION.innodb_force_recovery);
ERROR HY000: Variable 'innodb_force_recovery' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@GLOBAL.innodb_force_recovery);
COUNT(@@GLOBAL.innodb_force_recovery)
1
1 Expected
SELECT innodb_force_recovery = @@SESSION.innodb_force_recovery;
ERROR 42S22: Unknown column 'innodb_force_recovery' in 'field list'
Expected error 'Readonly variable'
