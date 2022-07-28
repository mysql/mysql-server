SET @start_global_value = @@global.innodb_rollback_segments;
SELECT @start_global_value;
@start_global_value
128
#
# Default value is 128 and exists as global only
#
SELECT @@GLOBAL.innodb_rollback_segments;
@@GLOBAL.innodb_rollback_segments
128
SELECT @@GLOBAL.innodb_rollback_segments >=0;
@@GLOBAL.innodb_rollback_segments >=0
1
SELECT @@GLOBAL.innodb_rollback_segments <=128;
@@GLOBAL.innodb_rollback_segments <=128
1
SELECT COUNT(@@GLOBAL.innodb_rollback_segments);
COUNT(@@GLOBAL.innodb_rollback_segments)
1
SELECT COUNT(@@innodb_rollback_segments);
COUNT(@@innodb_rollback_segments)
1
SHOW GLOBAL variables LIKE 'innodb_rollback_segments';
Variable_name	Value
innodb_rollback_segments	128
SHOW SESSION variables LIKE 'innodb_rollback_segments';
Variable_name	Value
innodb_rollback_segments	128
# Expected below: error 'Variable is a GLOBAL variable'
SELECT @@SESSION.innodb_rollback_segments;
ERROR HY000: Variable 'innodb_rollback_segments' is a GLOBAL variable
SELECT COUNT(@@SESSION.innodb_rollback_segments);
ERROR HY000: Variable 'innodb_rollback_segments' is a GLOBAL variable
SELECT innodb_rollback_segments;
ERROR 42S22: Unknown column 'innodb_rollback_segments' in 'field list'
SELECT @@LOCAL.innodb_rollback_segments;
ERROR HY000: Variable 'innodb_rollback_segments' is a GLOBAL variable
SELECT COUNT(@@LOCAL.innodb_rollback_segments);
ERROR HY000: Variable 'innodb_rollback_segments' is a GLOBAL variable
SELECT innodb_rollback_segments;
ERROR 42S22: Unknown column 'innodb_rollback_segments' in 'field list'
#
# Check if accessing variable with and without GLOBAL point to same variable
#
SELECT @@innodb_rollback_segments = @@GLOBAL.innodb_rollback_segments;
@@innodb_rollback_segments = @@GLOBAL.innodb_rollback_segments
1
#
# Check if the value in GLOBAL table matches value in performance_schema
#
SELECT * FROM performance_schema.global_variables 
WHERE variable_name='innodb_rollback_segments';
VARIABLE_NAME	VARIABLE_VALUE
innodb_rollback_segments	128
SELECT * FROM performance_schema.session_variables 
WHERE variable_name='innodb_rollback_segments';
VARIABLE_NAME	VARIABLE_VALUE
innodb_rollback_segments	128
#
# Check if value can be set
#
SET GLOBAL innodb_rollback_segments=100;
SELECT @@global.innodb_rollback_segments;
@@global.innodb_rollback_segments
100
SELECT * FROM performance_schema.global_variables 
WHERE variable_name='innodb_rollback_segments';
VARIABLE_NAME	VARIABLE_VALUE
innodb_rollback_segments	100
SELECT * FROM performance_schema.session_variables 
WHERE variable_name='innodb_rollback_segments';
VARIABLE_NAME	VARIABLE_VALUE
innodb_rollback_segments	100
SET SESSION innodb_rollback_segments=101;
ERROR HY000: Variable 'innodb_rollback_segments' is a GLOBAL variable and should be set with SET GLOBAL
#
# Valid values are 1 - 128
#
SET GLOBAL innodb_rollback_segments=1.1;
ERROR 42000: Incorrect argument type to variable 'innodb_rollback_segments'
SET GLOBAL innodb_rollback_segments=1e1;
ERROR 42000: Incorrect argument type to variable 'innodb_rollback_segments'
SET GLOBAL innodb_rollback_segments="foo";
ERROR 42000: Incorrect argument type to variable 'innodb_rollback_segments'
SET GLOBAL innodb_rollback_segments=-7;
Warnings:
Warning	1292	Truncated incorrect innodb_rollback_segments value: '-7'
SELECT @@global.innodb_rollback_segments;
@@global.innodb_rollback_segments
1
SET GLOBAL innodb_rollback_segments=0;
Warnings:
Warning	1292	Truncated incorrect innodb_rollback_segments value: '0'
SELECT @@global.innodb_rollback_segments;
@@global.innodb_rollback_segments
1
SET GLOBAL innodb_rollback_segments=50;
SELECT @@global.innodb_rollback_segments;
@@global.innodb_rollback_segments
50
SET GLOBAL innodb_rollback_segments=129;
Warnings:
Warning	1292	Truncated incorrect innodb_rollback_segments value: '129'
SELECT @@global.innodb_rollback_segments;
@@global.innodb_rollback_segments
128
SELECT * FROM performance_schema.global_variables 
WHERE variable_name='innodb_rollback_segments';
VARIABLE_NAME	VARIABLE_VALUE
innodb_rollback_segments	128
#
# cleanup
#
SET @@global.innodb_rollback_segments = @start_global_value;
SELECT @@global.innodb_rollback_segments;
@@global.innodb_rollback_segments
128
