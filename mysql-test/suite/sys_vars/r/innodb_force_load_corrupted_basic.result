'#---------------------BS_STVARS_031_01----------------------#'
SELECT COUNT(@@GLOBAL.innodb_force_load_corrupted);
COUNT(@@GLOBAL.innodb_force_load_corrupted)
1
1 Expected
'#---------------------BS_STVARS_031_02----------------------#'
SET @@GLOBAL.innodb_force_load_corrupted=1;
ERROR HY000: Variable 'innodb_force_load_corrupted' is a read only variable
Expected error 'Read only variable'
SELECT COUNT(@@GLOBAL.innodb_force_load_corrupted);
COUNT(@@GLOBAL.innodb_force_load_corrupted)
1
1 Expected
'#---------------------BS_STVARS_031_03----------------------#'
SELECT IF(@@GLOBAL.innodb_force_load_corrupted, "ON", "OFF") = VARIABLE_VALUE
FROM performance_schema.global_variables
WHERE VARIABLE_NAME='innodb_force_load_corrupted';
IF(@@GLOBAL.innodb_force_load_corrupted, "ON", "OFF") = VARIABLE_VALUE
1
1 Expected
SELECT COUNT(@@GLOBAL.innodb_force_load_corrupted);
COUNT(@@GLOBAL.innodb_force_load_corrupted)
1
1 Expected
SELECT COUNT(VARIABLE_VALUE)
FROM performance_schema.global_variables 
WHERE VARIABLE_NAME='innodb_force_load_corrupted';
COUNT(VARIABLE_VALUE)
1
1 Expected
'#---------------------BS_STVARS_031_04----------------------#'
SELECT @@innodb_force_load_corrupted = @@GLOBAL.innodb_force_load_corrupted;
@@innodb_force_load_corrupted = @@GLOBAL.innodb_force_load_corrupted
1
1 Expected
'#---------------------BS_STVARS_031_05----------------------#'
SELECT COUNT(@@innodb_force_load_corrupted);
COUNT(@@innodb_force_load_corrupted)
1
1 Expected
SELECT COUNT(@@local.innodb_force_load_corrupted);
ERROR HY000: Variable 'innodb_force_load_corrupted' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@SESSION.innodb_force_load_corrupted);
ERROR HY000: Variable 'innodb_force_load_corrupted' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@GLOBAL.innodb_force_load_corrupted);
COUNT(@@GLOBAL.innodb_force_load_corrupted)
1
1 Expected
SELECT innodb_force_load_corrupted;
ERROR 42S22: Unknown column 'innodb_force_load_corrupted' in 'field list'
Expected error 'Unknown column'
