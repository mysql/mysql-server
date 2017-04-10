'#---------------------BS_STVARS_009_01----------------------#'
SELECT COUNT(@@GLOBAL.have_dynamic_loading);
COUNT(@@GLOBAL.have_dynamic_loading)
1
1 Expected
'#---------------------BS_STVARS_009_02----------------------#'
SET @@GLOBAL.have_dynamic_loading=1;
ERROR HY000: Variable 'have_dynamic_loading' is a read only variable
Expected error 'Read only variable'
SELECT COUNT(@@GLOBAL.have_dynamic_loading);
COUNT(@@GLOBAL.have_dynamic_loading)
1
1 Expected
'#---------------------BS_STVARS_009_03----------------------#'
SELECT @@GLOBAL.have_dynamic_loading = VARIABLE_VALUE
FROM performance_schema.global_variables
WHERE VARIABLE_NAME='have_dynamic_loading';
@@GLOBAL.have_dynamic_loading = VARIABLE_VALUE
1
1 Expected
SELECT COUNT(@@GLOBAL.have_dynamic_loading);
COUNT(@@GLOBAL.have_dynamic_loading)
1
1 Expected
SELECT COUNT(VARIABLE_VALUE)
FROM performance_schema.global_variables 
WHERE VARIABLE_NAME='have_dynamic_loading';
COUNT(VARIABLE_VALUE)
1
1 Expected
'#---------------------BS_STVARS_009_04----------------------#'
SELECT @@have_dynamic_loading = @@GLOBAL.have_dynamic_loading;
@@have_dynamic_loading = @@GLOBAL.have_dynamic_loading
1
1 Expected
'#---------------------BS_STVARS_009_05----------------------#'
SELECT COUNT(@@have_dynamic_loading);
COUNT(@@have_dynamic_loading)
1
1 Expected
SELECT COUNT(@@local.have_dynamic_loading);
ERROR HY000: Variable 'have_dynamic_loading' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@SESSION.have_dynamic_loading);
ERROR HY000: Variable 'have_dynamic_loading' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@GLOBAL.have_dynamic_loading);
COUNT(@@GLOBAL.have_dynamic_loading)
1
1 Expected
SELECT have_dynamic_loading = @@SESSION.have_dynamic_loading;
ERROR 42S22: Unknown column 'have_dynamic_loading' in 'field list'
Expected error 'Readonly variable'
