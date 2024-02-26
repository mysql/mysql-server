SELECT COUNT(@@GLOBAL.innodb_doublewrite_pages);
COUNT(@@GLOBAL.innodb_doublewrite_pages)
1
1 Expected
SET @@GLOBAL.innodb_doublewrite_pages=1;
ERROR HY000: Variable 'innodb_doublewrite_pages' is a read only variable
Expected error 'Read only variable'
SELECT COUNT(@@GLOBAL.innodb_doublewrite_pages);
COUNT(@@GLOBAL.innodb_doublewrite_pages)
1
1 Expected
SELECT @@GLOBAL.innodb_doublewrite_pages = VARIABLE_VALUE
FROM performance_schema.global_variables
WHERE VARIABLE_NAME='innodb_doublewrite_pages';
@@GLOBAL.innodb_doublewrite_pages = VARIABLE_VALUE
1
1 Expected
SELECT COUNT(@@GLOBAL.innodb_doublewrite_pages);
COUNT(@@GLOBAL.innodb_doublewrite_pages)
1
1 Expected
SELECT COUNT(VARIABLE_VALUE)
FROM performance_schema.global_variables 
WHERE VARIABLE_NAME='innodb_doublewrite_pages';
COUNT(VARIABLE_VALUE)
1
1 Expected
SELECT @@innodb_doublewrite_pages = @@GLOBAL.innodb_doublewrite_pages;
@@innodb_doublewrite_pages = @@GLOBAL.innodb_doublewrite_pages
1
1 Expected
SELECT COUNT(@@innodb_doublewrite_pages);
COUNT(@@innodb_doublewrite_pages)
1
1 Expected
SELECT COUNT(@@local.innodb_doublewrite_pages);
ERROR HY000: Variable 'innodb_doublewrite_pages' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@SESSION.innodb_doublewrite_pages);
ERROR HY000: Variable 'innodb_doublewrite_pages' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@GLOBAL.innodb_doublewrite_pages);
COUNT(@@GLOBAL.innodb_doublewrite_pages)
1
1 Expected
SELECT innodb_doublewrite_pages;
ERROR 42S22: Unknown column 'innodb_doublewrite_pages' in 'field list'
Expected error 'Unknown column'
