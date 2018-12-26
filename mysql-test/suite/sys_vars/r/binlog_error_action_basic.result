SELECT @@GLOBAL.binlog_error_action;
@@GLOBAL.binlog_error_action
ABORT_SERVER
ABORT_SERVER Expected
SELECT @@SESSION.binlog_error_action;
ERROR HY000: Variable 'binlog_error_action' is a GLOBAL variable
SET @start_value= @@global.binlog_error_action;
SHOW GLOBAL VARIABLES LIKE 'binlog_error_action';
Variable_name	Value
binlog_error_action	ABORT_SERVER
SET @@GLOBAL.binlog_error_action=IGNORE_ERROR;
SELECT @@GLOBAL.binlog_error_action;
@@GLOBAL.binlog_error_action
IGNORE_ERROR
IGNORE_ERROR Expected
SET @@GLOBAL.binlog_error_action=ABORT_SERVER;
SELECT @@GLOBAL.binlog_error_action;
@@GLOBAL.binlog_error_action
ABORT_SERVER
ABORT_SERVER Expected
SELECT @@GLOBAL.binlog_error_action = VARIABLE_VALUE
FROM performance_schema.global_variables
WHERE VARIABLE_NAME='binlog_error_action';
@@GLOBAL.binlog_error_action = VARIABLE_VALUE
1
1 Expected
SELECT COUNT(@@GLOBAL.binlog_error_action);
COUNT(@@GLOBAL.binlog_error_action)
1
1 Expected
SELECT COUNT(VARIABLE_VALUE)
FROM performance_schema.global_variables
WHERE VARIABLE_NAME='binlog_error_action';
COUNT(VARIABLE_VALUE)
1
1 Expected
SELECT COUNT(@@binlog_error_action);
COUNT(@@binlog_error_action)
1
1 Expected
SELECT COUNT(@@GLOBAL.binlog_error_action);
COUNT(@@GLOBAL.binlog_error_action)
1
1 Expected
SET @@global.binlog_error_action= @start_value;
