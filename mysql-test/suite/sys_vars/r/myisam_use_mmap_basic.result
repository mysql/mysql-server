'#---------------------BS_STVARS_042_01----------------------#'
SELECT COUNT(@@GLOBAL.myisam_use_mmap);
COUNT(@@GLOBAL.myisam_use_mmap)
1
1 Expected
'#---------------------BS_STVARS_042_02----------------------#'
"BUG:It should give error on setting this variable as it is readonly variable"
Expected error 'Read only variable'
SELECT COUNT(@@GLOBAL.myisam_use_mmap);
COUNT(@@GLOBAL.myisam_use_mmap)
1
1 Expected
'#---------------------BS_STVARS_042_03----------------------#'
SELECT IF(@@GLOBAL.myisam_use_mmap, "ON", "OFF") = VARIABLE_VALUE
FROM performance_schema.global_variables
WHERE VARIABLE_NAME='myisam_use_mmap';
IF(@@GLOBAL.myisam_use_mmap, "ON", "OFF") = VARIABLE_VALUE
1
1 Expected
SELECT COUNT(@@GLOBAL.myisam_use_mmap);
COUNT(@@GLOBAL.myisam_use_mmap)
1
1 Expected
SELECT COUNT(VARIABLE_VALUE)
FROM performance_schema.global_variables 
WHERE VARIABLE_NAME='myisam_use_mmap';
COUNT(VARIABLE_VALUE)
1
1 Expected
'#---------------------BS_STVARS_042_04----------------------#'
SELECT @@myisam_use_mmap = @@GLOBAL.myisam_use_mmap;
@@myisam_use_mmap = @@GLOBAL.myisam_use_mmap
1
1 Expected
'#---------------------BS_STVARS_042_05----------------------#'
SELECT COUNT(@@myisam_use_mmap);
COUNT(@@myisam_use_mmap)
1
1 Expected
SELECT COUNT(@@local.myisam_use_mmap);
ERROR HY000: Variable 'myisam_use_mmap' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@SESSION.myisam_use_mmap);
ERROR HY000: Variable 'myisam_use_mmap' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@GLOBAL.myisam_use_mmap);
COUNT(@@GLOBAL.myisam_use_mmap)
1
1 Expected
SELECT myisam_use_mmap = @@SESSION.myisam_use_mmap;
ERROR 42S22: Unknown column 'myisam_use_mmap' in 'field list'
Expected error 'Readonly variable'
