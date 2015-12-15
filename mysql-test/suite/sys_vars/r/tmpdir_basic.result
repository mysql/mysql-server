'#---------------------BS_STVARS_052_01----------------------#'
SELECT COUNT(@@GLOBAL.tmpdir);
COUNT(@@GLOBAL.tmpdir)
1
1 Expected
'#---------------------BS_STVARS_052_02----------------------#'
SET @@GLOBAL.tmpdir=1;
ERROR HY000: Variable 'tmpdir' is a read only variable
Expected error 'Read only variable'
SELECT COUNT(@@GLOBAL.tmpdir);
COUNT(@@GLOBAL.tmpdir)
1
1 Expected
'#---------------------BS_STVARS_052_03----------------------#'
SELECT @@GLOBAL.tmpdir = VARIABLE_VALUE
FROM performance_schema.global_variables
WHERE VARIABLE_NAME='tmpdir';
@@GLOBAL.tmpdir = VARIABLE_VALUE
1
1 Expected
SELECT COUNT(@@GLOBAL.tmpdir);
COUNT(@@GLOBAL.tmpdir)
1
1 Expected
SELECT COUNT(VARIABLE_VALUE)
FROM performance_schema.global_variables 
WHERE VARIABLE_NAME='tmpdir';
COUNT(VARIABLE_VALUE)
1
1 Expected
'#---------------------BS_STVARS_052_04----------------------#'
SELECT @@tmpdir = @@GLOBAL.tmpdir;
@@tmpdir = @@GLOBAL.tmpdir
1
1 Expected
'#---------------------BS_STVARS_052_05----------------------#'
SELECT COUNT(@@tmpdir);
COUNT(@@tmpdir)
1
1 Expected
SELECT COUNT(@@local.tmpdir);
ERROR HY000: Variable 'tmpdir' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@SESSION.tmpdir);
ERROR HY000: Variable 'tmpdir' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@GLOBAL.tmpdir);
COUNT(@@GLOBAL.tmpdir)
1
1 Expected
SELECT tmpdir = @@SESSION.tmpdir;
ERROR 42S22: Unknown column 'tmpdir' in 'field list'
Expected error 'Readonly variable'
