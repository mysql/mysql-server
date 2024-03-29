'#---------------------BS_STVARS_018_01----------------------#'
SELECT COUNT(@@GLOBAL.have_symlink);
COUNT(@@GLOBAL.have_symlink)
1
1 Expected
'#---------------------BS_STVARS_018_02----------------------#'
SET @@GLOBAL.have_symlink=1;
ERROR HY000: Variable 'have_symlink' is a read only variable
Expected error 'Read only variable'
SELECT COUNT(@@GLOBAL.have_symlink);
COUNT(@@GLOBAL.have_symlink)
1
1 Expected
'#---------------------BS_STVARS_018_03----------------------#'
SELECT @@GLOBAL.have_symlink = VARIABLE_VALUE
FROM performance_schema.global_variables
WHERE VARIABLE_NAME='have_symlink';
@@GLOBAL.have_symlink = VARIABLE_VALUE
1
1 Expected
SELECT COUNT(@@GLOBAL.have_symlink);
COUNT(@@GLOBAL.have_symlink)
1
1 Expected
SELECT COUNT(VARIABLE_VALUE)
FROM performance_schema.global_variables 
WHERE VARIABLE_NAME='have_symlink';
COUNT(VARIABLE_VALUE)
1
1 Expected
'#---------------------BS_STVARS_018_04----------------------#'
SELECT @@have_symlink = @@GLOBAL.have_symlink;
@@have_symlink = @@GLOBAL.have_symlink
1
1 Expected
'#---------------------BS_STVARS_018_05----------------------#'
SELECT COUNT(@@have_symlink);
COUNT(@@have_symlink)
1
1 Expected
SELECT COUNT(@@local.have_symlink);
ERROR HY000: Variable 'have_symlink' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@SESSION.have_symlink);
ERROR HY000: Variable 'have_symlink' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@GLOBAL.have_symlink);
COUNT(@@GLOBAL.have_symlink)
1
1 Expected
SELECT have_symlink;
ERROR 42S22: Unknown column 'have_symlink' in 'field list'
Expected error 'Unknown column'
