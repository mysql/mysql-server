SELECT COUNT(@@GLOBAL.innodb_purge_threads);
COUNT(@@GLOBAL.innodb_purge_threads)
1
1 Expected
SELECT COUNT(@@innodb_purge_threads);
COUNT(@@innodb_purge_threads)
1
1 Expected
SET @@GLOBAL.innodb_purge_threads=1;
ERROR HY000: Variable 'innodb_purge_threads' is a read only variable
Expected error 'Read-only variable'
SELECT innodb_purge_threads;
ERROR 42S22: Unknown column 'innodb_purge_threads' in 'field list'
Expected error 'Unknown column'
SELECT @@SESSION.innodb_purge_threads;
ERROR HY000: Variable 'innodb_purge_threads' is a GLOBAL variable
SELECT @@GLOBAL.innodb_purge_threads = VARIABLE_VALUE
FROM performance_schema.global_variables
WHERE VARIABLE_NAME='innodb_purge_threads';
@@GLOBAL.innodb_purge_threads = VARIABLE_VALUE
1
1 Expected
SELECT COUNT(VARIABLE_VALUE)
FROM performance_schema.global_variables 
WHERE VARIABLE_NAME='innodb_purge_threads';
COUNT(VARIABLE_VALUE)
1
1 Expected
SELECT @@innodb_purge_threads = @@GLOBAL.innodb_purge_threads;
@@innodb_purge_threads = @@GLOBAL.innodb_purge_threads
1
1 Expected
SELECT COUNT(@@local.innodb_purge_threads);
ERROR HY000: Variable 'innodb_purge_threads' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@SESSION.innodb_purge_threads);
ERROR HY000: Variable 'innodb_purge_threads' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT VARIABLE_NAME, VARIABLE_VALUE
FROM performance_schema.global_variables
WHERE VARIABLE_NAME = 'innodb_purge_threads';
VARIABLE_NAME	VARIABLE_VALUE
innodb_purge_threads	4
