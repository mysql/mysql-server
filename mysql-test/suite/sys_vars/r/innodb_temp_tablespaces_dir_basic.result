SELECT COUNT(@@GLOBAL.innodb_temp_tablespaces_dir);
COUNT(@@GLOBAL.innodb_temp_tablespaces_dir)
1
1 Expected
SET @@GLOBAL.innodb_temp_tablespaces_dir="/tmp";
ERROR HY000: Variable 'innodb_temp_tablespaces_dir' is a read only variable
Expected error 'Read only variable'
SELECT COUNT(@@GLOBAL.innodb_temp_tablespaces_dir);
COUNT(@@GLOBAL.innodb_temp_tablespaces_dir)
1
1 Expected
SELECT @@GLOBAL.innodb_temp_tablespaces_dir = VARIABLE_VALUE
FROM performance_schema.global_variables
WHERE VARIABLE_NAME='innodb_temp_tablespaces_dir';
@@GLOBAL.innodb_temp_tablespaces_dir = VARIABLE_VALUE
1
1 Expected
SELECT COUNT(@@GLOBAL.innodb_temp_tablespaces_dir);
COUNT(@@GLOBAL.innodb_temp_tablespaces_dir)
1
1 Expected
SELECT COUNT(VARIABLE_VALUE)
FROM performance_schema.global_variables
WHERE VARIABLE_NAME='innodb_temp_tablespaces_dir';
COUNT(VARIABLE_VALUE)
1
1 Expected
SELECT @@innodb_temp_tablespaces_dir = @@GLOBAL.innodb_temp_tablespaces_dir;
@@innodb_temp_tablespaces_dir = @@GLOBAL.innodb_temp_tablespaces_dir
1
1 Expected
SELECT @@innodb_temp_tablespaces_dir = VARIABLE_VALUE
FROM performance_schema.session_variables
WHERE VARIABLE_NAME='innodb_temp_tablespaces_dir';
@@innodb_temp_tablespaces_dir = VARIABLE_VALUE
1
1 Expected
SET PERSIST_ONLY innodb_temp_tablespaces_dir = "/tmp";
ERROR HY000: Variable 'innodb_temp_tablespaces_dir' is a non persistent read only variable
Expected error 'Non persistent read only variable'
SELECT COUNT(@@innodb_temp_tablespaces_dir);
COUNT(@@innodb_temp_tablespaces_dir)
1
1 Expected
SELECT COUNT(@@local.innodb_temp_tablespaces_dir);
ERROR HY000: Variable 'innodb_temp_tablespaces_dir' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@SESSION.innodb_temp_tablespaces_dir);
ERROR HY000: Variable 'innodb_temp_tablespaces_dir' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@GLOBAL.innodb_temp_tablespaces_dir);
COUNT(@@GLOBAL.innodb_temp_tablespaces_dir)
1
1 Expected
SELECT innodb_temp_tablespaces_dir = @@SESSION.innodb_temp_tablespaces_dir;
ERROR 42S22: Unknown column 'innodb_temp_tablespaces_dir' in 'field list'
Expected error 'Readonly variable'
SET innodb_temp_tablespaces_dir = "/tmp";
ERROR HY000: Variable 'innodb_temp_tablespaces_dir' is a read only variable
Expected error 'Read only variable'
SET local.innodb_temp_tablespaces_dir = 1;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'local.innodb_temp_tablespaces_dir = 1' at line 1
Expected error 'Parse error'
SELECT local.innodb_temp_tablespaces_dir;
ERROR 42S02: Unknown table 'local' in field list
Expected error 'Unknown table local in field list'
SET global.innodb_temp_tablespaces_dir = 1;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'global.innodb_temp_tablespaces_dir = 1' at line 1
Expected error 'Parse error'
SELECT global.innodb_temp_tablespaces_dir;
ERROR 42S02: Unknown table 'global' in field list
Expected error 'Unknown table global in field list'
SELECT innodb_temp_tablespaces_dir = @@session.innodb_temp_tablespaces_dir;
ERROR 42S22: Unknown column 'innodb_temp_tablespaces_dir' in 'field list'
SELECT innodb_temp_tablespaces_dir;
ERROR 42S22: Unknown column 'innodb_temp_tablespaces_dir' in 'field list'
Expected error 'Unknown column innodb_temp_tablespaces_dir in field list'
