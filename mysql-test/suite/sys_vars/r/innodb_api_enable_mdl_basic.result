'#---------------------BS_STVARS_035_01----------------------#'
SELECT COUNT(@@GLOBAL.innodb_api_enable_mdl);
COUNT(@@GLOBAL.innodb_api_enable_mdl)
1
1 Expected
'#---------------------BS_STVARS_035_02----------------------#'
SET @@GLOBAL.innodb_api_enable_mdl=1;
ERROR HY000: Variable 'innodb_api_enable_mdl' is a read only variable
Expected error 'Read only variable'
SELECT COUNT(@@GLOBAL.innodb_api_enable_mdl);
COUNT(@@GLOBAL.innodb_api_enable_mdl)
1
1 Expected
'#---------------------BS_STVARS_035_03----------------------#'
SELECT IF(@@GLOBAL.innodb_api_enable_mdl, 'ON', 'OFF') = VARIABLE_VALUE
FROM performance_schema.global_variables
WHERE VARIABLE_NAME='innodb_api_enable_mdl';
IF(@@GLOBAL.innodb_api_enable_mdl, 'ON', 'OFF') = VARIABLE_VALUE
1
1 Expected
SELECT COUNT(@@GLOBAL.innodb_api_enable_mdl);
COUNT(@@GLOBAL.innodb_api_enable_mdl)
1
1 Expected
SELECT COUNT(VARIABLE_VALUE)
FROM performance_schema.global_variables 
WHERE VARIABLE_NAME='innodb_api_enable_mdl';
COUNT(VARIABLE_VALUE)
1
1 Expected
'#---------------------BS_STVARS_035_04----------------------#'
SELECT @@innodb_api_enable_mdl = @@GLOBAL.innodb_api_enable_mdl;
@@innodb_api_enable_mdl = @@GLOBAL.innodb_api_enable_mdl
1
1 Expected
'#---------------------BS_STVARS_035_05----------------------#'
SELECT COUNT(@@innodb_api_enable_mdl);
COUNT(@@innodb_api_enable_mdl)
1
1 Expected
SELECT COUNT(@@local.innodb_api_enable_mdl);
ERROR HY000: Variable 'innodb_api_enable_mdl' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@SESSION.innodb_api_enable_mdl);
ERROR HY000: Variable 'innodb_api_enable_mdl' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@GLOBAL.innodb_api_enable_mdl);
COUNT(@@GLOBAL.innodb_api_enable_mdl)
1
1 Expected
SELECT innodb_api_enable_mdl;
ERROR 42S22: Unknown column 'innodb_api_enable_mdl' in 'field list'
Expected error 'Unknown column'
