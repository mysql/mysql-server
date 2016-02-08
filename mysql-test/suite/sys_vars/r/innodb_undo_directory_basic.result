SELECT COUNT(@@GLOBAL.innodb_undo_directory);
COUNT(@@GLOBAL.innodb_undo_directory)
1
1 Expected
SET @@GLOBAL.innodb_undo_directory="/tmp";
ERROR HY000: Variable 'innodb_undo_directory' is a read only variable
Expected error 'Read only variable'
SELECT COUNT(@@GLOBAL.innodb_undo_directory);
COUNT(@@GLOBAL.innodb_undo_directory)
1
1 Expected
SELECT @@GLOBAL.innodb_undo_directory = VARIABLE_VALUE
FROM performance_schema.global_variables
WHERE VARIABLE_NAME='innodb_undo_directory';
@@GLOBAL.innodb_undo_directory = VARIABLE_VALUE
1
1 Expected
SELECT COUNT(@@GLOBAL.innodb_undo_directory);
COUNT(@@GLOBAL.innodb_undo_directory)
1
1 Expected
SELECT COUNT(VARIABLE_VALUE)
FROM performance_schema.global_variables 
WHERE VARIABLE_NAME='innodb_undo_directory';
COUNT(VARIABLE_VALUE)
1
1 Expected
SELECT @@innodb_undo_directory = @@GLOBAL.innodb_undo_directory;
@@innodb_undo_directory = @@GLOBAL.innodb_undo_directory
1
1 Expected
SELECT COUNT(@@innodb_undo_directory);
COUNT(@@innodb_undo_directory)
1
1 Expected
SELECT COUNT(@@local.innodb_undo_directory);
ERROR HY000: Variable 'innodb_undo_directory' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@SESSION.innodb_undo_directory);
ERROR HY000: Variable 'innodb_undo_directory' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@GLOBAL.innodb_undo_directory);
COUNT(@@GLOBAL.innodb_undo_directory)
1
1 Expected
SELECT innodb_undo_directory = @@SESSION.innodb_undo_directory;
ERROR 42S22: Unknown column 'innodb_undo_directory' in 'field list'
Expected error 'Readonly variable'
