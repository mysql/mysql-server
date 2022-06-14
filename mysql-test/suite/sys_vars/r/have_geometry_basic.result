'#---------------------BS_STVARS_010_01----------------------#'
SELECT COUNT(@@GLOBAL.have_geometry);
COUNT(@@GLOBAL.have_geometry)
1
1 Expected
'#---------------------BS_STVARS_010_02----------------------#'
SET @@GLOBAL.have_geometry=1;
ERROR HY000: Variable 'have_geometry' is a read only variable
Expected error 'Read only variable'
SELECT COUNT(@@GLOBAL.have_geometry);
COUNT(@@GLOBAL.have_geometry)
1
1 Expected
'#---------------------BS_STVARS_010_03----------------------#'
SELECT @@GLOBAL.have_geometry = VARIABLE_VALUE
FROM performance_schema.global_variables
WHERE VARIABLE_NAME='have_geometry';
@@GLOBAL.have_geometry = VARIABLE_VALUE
1
1 Expected
SELECT COUNT(@@GLOBAL.have_geometry);
COUNT(@@GLOBAL.have_geometry)
1
1 Expected
SELECT COUNT(VARIABLE_VALUE)
FROM performance_schema.global_variables 
WHERE VARIABLE_NAME='have_geometry';
COUNT(VARIABLE_VALUE)
1
1 Expected
'#---------------------BS_STVARS_010_04----------------------#'
SELECT @@have_geometry = @@GLOBAL.have_geometry;
@@have_geometry = @@GLOBAL.have_geometry
1
1 Expected
'#---------------------BS_STVARS_010_05----------------------#'
SELECT COUNT(@@have_geometry);
COUNT(@@have_geometry)
1
1 Expected
SELECT COUNT(@@local.have_geometry);
ERROR HY000: Variable 'have_geometry' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@SESSION.have_geometry);
ERROR HY000: Variable 'have_geometry' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@GLOBAL.have_geometry);
COUNT(@@GLOBAL.have_geometry)
1
1 Expected
SELECT have_geometry;
ERROR 42S22: Unknown column 'have_geometry' in 'field list'
Expected error 'Unknown column'
