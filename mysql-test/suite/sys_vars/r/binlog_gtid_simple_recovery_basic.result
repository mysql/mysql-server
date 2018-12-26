SELECT @@GLOBAL.binlog_gtid_simple_recovery;
@@GLOBAL.binlog_gtid_simple_recovery
1
SELECT @@SESSION.binlog_gtid_simple_recovery;
ERROR HY000: Variable 'binlog_gtid_simple_recovery' is a GLOBAL variable
SHOW GLOBAL VARIABLES LIKE 'binlog_gtid_simple_recovery';
Variable_name	Value
binlog_gtid_simple_recovery	ON
SHOW SESSION VARIABLES LIKE 'binlog_gtid_simple_recovery';
Variable_name	Value
binlog_gtid_simple_recovery	ON
SELECT * FROM performance_schema.global_variables WHERE VARIABLE_NAME='binlog_gtid_simple_recovery';
VARIABLE_NAME	VARIABLE_VALUE
binlog_gtid_simple_recovery	ON
SELECT * FROM performance_schema.session_variables WHERE VARIABLE_NAME='binlog_gtid_simple_recovery';
VARIABLE_NAME	VARIABLE_VALUE
binlog_gtid_simple_recovery	ON
SET GLOBAL binlog_gtid_simple_recovery=1;
ERROR HY000: Variable 'binlog_gtid_simple_recovery' is a read only variable
SET SESSION binlog_gtid_simple_recovery=1;
ERROR HY000: Variable 'binlog_gtid_simple_recovery' is a read only variable
