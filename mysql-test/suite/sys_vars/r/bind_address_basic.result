'#---------------------BS_STVARS_001_01----------------------#'
SELECT COUNT(@@GLOBAL.bind_address);
COUNT(@@GLOBAL.bind_address)
1
1 Expected
'#---------------------BS_STVARS_001_02----------------------#'
SET @@GLOBAL.bind_address=1;
ERROR HY000: Variable 'bind_address' is a read only variable
Expected error 'Read only variable'
SELECT COUNT(@@GLOBAL.bind_address);
COUNT(@@GLOBAL.bind_address)
1
1 Expected
'#---------------------BS_STVARS_001_03----------------------#'
SELECT VARIABLE_VALUE = ''
FROM performance_schema.global_variables
WHERE VARIABLE_NAME='bind_address';
VARIABLE_VALUE = ''
0
0 Expected
SELECT COUNT(@@GLOBAL.bind_address);
COUNT(@@GLOBAL.bind_address)
1
1 Expected
SELECT COUNT(VARIABLE_VALUE)
FROM performance_schema.global_variables 
WHERE VARIABLE_NAME='bind_address';
COUNT(VARIABLE_VALUE)
1
1 Expected
'#---------------------BS_STVARS_001_04----------------------#'
SELECT @@bind_address is NULL AND @@GLOBAL.bind_address is NULL;
@@bind_address is NULL AND @@GLOBAL.bind_address is NULL
0
0 Expected
SELECT @@bind_address is NOT NULL AND @@GLOBAL.bind_address is NOT NULL;
@@bind_address is NOT NULL AND @@GLOBAL.bind_address is NOT NULL
1
1 Expected
'#---------------------BS_STVARS_001_05----------------------#'
SELECT COUNT(@@bind_address);
COUNT(@@bind_address)
1
1 Expected
SELECT COUNT(@@local.bind_address);
ERROR HY000: Variable 'bind_address' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@SESSION.bind_address);
ERROR HY000: Variable 'bind_address' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@GLOBAL.bind_address);
COUNT(@@GLOBAL.bind_address)
1
1 Expected
SELECT bind_address = @@SESSION.bind_address;
ERROR 42S22: Unknown column 'bind_address' in 'field list'
Expected error 'Readonly variable'
