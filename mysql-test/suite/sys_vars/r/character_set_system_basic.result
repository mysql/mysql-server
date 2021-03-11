'#---------------------BS_STVARS_003_01----------------------#'
SELECT COUNT(@@GLOBAL.character_set_system);
COUNT(@@GLOBAL.character_set_system)
1
1 Expected
'#---------------------BS_STVARS_003_02----------------------#'
SET @@GLOBAL.character_set_system=1;
ERROR HY000: Variable 'character_set_system' is a read only variable
Expected error 'Read only variable'
SELECT COUNT(@@GLOBAL.character_set_system);
COUNT(@@GLOBAL.character_set_system)
1
1 Expected
'#---------------------BS_STVARS_003_03----------------------#'
SELECT @@GLOBAL.character_set_system = VARIABLE_VALUE
FROM performance_schema.global_variables
WHERE VARIABLE_NAME='character_set_system';
@@GLOBAL.character_set_system = VARIABLE_VALUE
1
1 Expected
SELECT COUNT(@@GLOBAL.character_set_system);
COUNT(@@GLOBAL.character_set_system)
1
1 Expected
SELECT COUNT(VARIABLE_VALUE)
FROM performance_schema.global_variables 
WHERE VARIABLE_NAME='character_set_system';
COUNT(VARIABLE_VALUE)
1
1 Expected
'#---------------------BS_STVARS_003_04----------------------#'
SELECT @@character_set_system = @@GLOBAL.character_set_system;
@@character_set_system = @@GLOBAL.character_set_system
1
1 Expected
'#---------------------BS_STVARS_003_05----------------------#'
SELECT COUNT(@@character_set_system);
COUNT(@@character_set_system)
1
1 Expected
SELECT COUNT(@@local.character_set_system);
ERROR HY000: Variable 'character_set_system' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@SESSION.character_set_system);
ERROR HY000: Variable 'character_set_system' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@GLOBAL.character_set_system);
COUNT(@@GLOBAL.character_set_system)
1
1 Expected
SELECT character_set_system;
ERROR 42S22: Unknown column 'character_set_system' in 'field list'
Expected error 'Unknown column'
