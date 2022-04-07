SELECT  @@GLOBAL.default_authentication_plugin;
@@GLOBAL.default_authentication_plugin
caching_sha2_password
SELECT COUNT(@@GLOBAL.default_authentication_plugin);
COUNT(@@GLOBAL.default_authentication_plugin)
1
1 Expected
SET @@GLOBAL.default_authentication_plugin=1;
ERROR HY000: Variable 'default_authentication_plugin' is a read only variable
Expected error 'Read only variable'
SELECT COUNT(@@GLOBAL.default_authentication_plugin);
COUNT(@@GLOBAL.default_authentication_plugin)
1
1 Expected
SELECT @@GLOBAL.default_authentication_plugin = VARIABLE_VALUE
FROM performance_schema.global_variables
WHERE VARIABLE_NAME='default_authentication_plugin';
@@GLOBAL.default_authentication_plugin = VARIABLE_VALUE
1
1 Expected
SELECT COUNT(@@GLOBAL.default_authentication_plugin);
COUNT(@@GLOBAL.default_authentication_plugin)
1
1 Expected
SELECT COUNT(VARIABLE_VALUE)
FROM performance_schema.global_variables
WHERE VARIABLE_NAME='default_authentication_plugin';
COUNT(VARIABLE_VALUE)
1
1 Expected
SELECT @@default_authentication_plugin = @@GLOBAL.default_authentication_plugin;
@@default_authentication_plugin = @@GLOBAL.default_authentication_plugin
1
1 Expected
SELECT COUNT(@@default_authentication_plugin);
COUNT(@@default_authentication_plugin)
1
1 Expected
SELECT COUNT(@@local.default_authentication_plugin);
ERROR HY000: Variable 'default_authentication_plugin' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@SESSION.default_authentication_plugin);
ERROR HY000: Variable 'default_authentication_plugin' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@GLOBAL.default_authentication_plugin);
COUNT(@@GLOBAL.default_authentication_plugin)
1
1 Expected
SELECT default_authentication_plugin = @@SESSION.default_authentication_plugin;
ERROR 42S22: Unknown column 'default_authentication_plugin' in 'field list'
Expected error 'Readonly variable'
