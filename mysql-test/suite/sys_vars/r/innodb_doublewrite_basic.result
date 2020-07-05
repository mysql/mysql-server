SELECT COUNT(@@GLOBAL.innodb_doublewrite);
COUNT(@@GLOBAL.innodb_doublewrite)
1
1 Expected
SET @@GLOBAL.innodb_doublewrite=off;
ERROR HY000: Variable 'innodb_doublewrite' is a read only variable
SET @@GLOBAL.innodb_doublewrite=on;
ERROR HY000: Variable 'innodb_doublewrite' is a read only variable
SELECT COUNT(@@GLOBAL.innodb_doublewrite);
COUNT(@@GLOBAL.innodb_doublewrite)
1
1 Expected
SELECT IF(@@GLOBAL.innodb_doublewrite, "ON", "OFF") = VARIABLE_VALUE
FROM performance_schema.global_variables
WHERE VARIABLE_NAME='innodb_doublewrite';
IF(@@GLOBAL.innodb_doublewrite, "ON", "OFF") = VARIABLE_VALUE
1
1 Expected
SELECT COUNT(@@GLOBAL.innodb_doublewrite);
COUNT(@@GLOBAL.innodb_doublewrite)
1
1 Expected
SELECT COUNT(VARIABLE_VALUE)
FROM performance_schema.global_variables 
WHERE VARIABLE_NAME='innodb_doublewrite';
COUNT(VARIABLE_VALUE)
1
1 Expected
SELECT @@innodb_doublewrite = @@GLOBAL.innodb_doublewrite;
@@innodb_doublewrite = @@GLOBAL.innodb_doublewrite
1
1 Expected
SELECT COUNT(@@innodb_doublewrite);
COUNT(@@innodb_doublewrite)
1
1 Expected
SELECT COUNT(@@local.innodb_doublewrite);
ERROR HY000: Variable 'innodb_doublewrite' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@SESSION.innodb_doublewrite);
ERROR HY000: Variable 'innodb_doublewrite' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@GLOBAL.innodb_doublewrite);
COUNT(@@GLOBAL.innodb_doublewrite)
1
1 Expected
