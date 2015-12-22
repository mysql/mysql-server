'#---------------------BS_STVARS_050_01----------------------#'
SELECT COUNT(@@GLOBAL.system_time_zone);
COUNT(@@GLOBAL.system_time_zone)
1
1 Expected
'#---------------------BS_STVARS_050_02----------------------#'
SET @@GLOBAL.system_time_zone=1;
ERROR HY000: Variable 'system_time_zone' is a read only variable
Expected error 'Read only variable'
SELECT COUNT(@@GLOBAL.system_time_zone);
COUNT(@@GLOBAL.system_time_zone)
1
1 Expected
'#---------------------BS_STVARS_050_03----------------------#'
SELECT @@GLOBAL.system_time_zone = VARIABLE_VALUE
FROM performance_schema.global_variables
WHERE VARIABLE_NAME='system_time_zone';
@@GLOBAL.system_time_zone = VARIABLE_VALUE
1
1 Expected
SELECT COUNT(@@GLOBAL.system_time_zone);
COUNT(@@GLOBAL.system_time_zone)
1
1 Expected
SELECT COUNT(VARIABLE_VALUE)
FROM performance_schema.global_variables 
WHERE VARIABLE_NAME='system_time_zone';
COUNT(VARIABLE_VALUE)
1
1 Expected
'#---------------------BS_STVARS_050_04----------------------#'
SELECT @@system_time_zone = @@GLOBAL.system_time_zone;
@@system_time_zone = @@GLOBAL.system_time_zone
1
1 Expected
'#---------------------BS_STVARS_050_05----------------------#'
SELECT COUNT(@@system_time_zone);
COUNT(@@system_time_zone)
1
1 Expected
SELECT COUNT(@@local.system_time_zone);
ERROR HY000: Variable 'system_time_zone' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@SESSION.system_time_zone);
ERROR HY000: Variable 'system_time_zone' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@GLOBAL.system_time_zone);
COUNT(@@GLOBAL.system_time_zone)
1
1 Expected
SELECT system_time_zone = @@SESSION.system_time_zone;
ERROR 42S22: Unknown column 'system_time_zone' in 'field list'
Expected error 'Readonly variable'
