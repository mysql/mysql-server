SELECT COUNT(@@GLOBAL.innodb_page_cleaners);
COUNT(@@GLOBAL.innodb_page_cleaners)
1
1 Expected
SELECT COUNT(@@innodb_page_cleaners);
COUNT(@@innodb_page_cleaners)
1
1 Expected
SET @@GLOBAL.innodb_page_cleaners=1;
ERROR HY000: Variable 'innodb_page_cleaners' is a read only variable
Expected error 'Read-only variable'
SELECT innodb_page_cleaners;
ERROR 42S22: Unknown column 'innodb_page_cleaners' in 'field list'
Expected error 'Unknown column'
SELECT @@SESSION.innodb_page_cleaners;
ERROR HY000: Variable 'innodb_page_cleaners' is a GLOBAL variable
SELECT @@GLOBAL.innodb_page_cleaners = VARIABLE_VALUE
FROM performance_schema.global_variables
WHERE VARIABLE_NAME='innodb_page_cleaners';
@@GLOBAL.innodb_page_cleaners = VARIABLE_VALUE
1
1 Expected
SELECT COUNT(VARIABLE_VALUE)
FROM performance_schema.global_variables
WHERE VARIABLE_NAME='innodb_page_cleaners';
COUNT(VARIABLE_VALUE)
1
1 Expected
SELECT @@innodb_page_cleaners = @@GLOBAL.innodb_page_cleaners;
@@innodb_page_cleaners = @@GLOBAL.innodb_page_cleaners
1
1 Expected
SELECT COUNT(@@local.innodb_page_cleaners);
ERROR HY000: Variable 'innodb_page_cleaners' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@SESSION.innodb_page_cleaners);
ERROR HY000: Variable 'innodb_page_cleaners' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT VARIABLE_NAME, VARIABLE_VALUE
FROM performance_schema.global_variables
WHERE VARIABLE_NAME = 'innodb_page_cleaners';
VARIABLE_NAME	VARIABLE_VALUE
innodb_page_cleaners	1
