include/assert.inc [Initial value for binlog_expire_logs_auto_purge is TRUE]
SET @@GLOBAL.binlog_expire_logs_auto_purge = FALSE;
include/assert.inc [Value for binlog_expire_logs_auto_purge was set to FALSE]
SET @@GLOBAL.binlog_expire_logs_auto_purge = DEFAULT;
include/assert.inc [Value for binlog_expire_logs_auto_purge was set to DEFAULT (TRUE)]
SET @@GLOBAL.binlog_expire_logs_auto_purge = 1;
include/assert.inc [Value for binlog_expire_logs_auto_purge was set to TRUE (1)]
SET @@GLOBAL.binlog_expire_logs_auto_purge = ON;
include/assert.inc [Value for binlog_expire_logs_auto_purge was set to TRUE (ON)]
SET @@GLOBAL.binlog_expire_logs_auto_purge = 0;
include/assert.inc [Value for binlog_expire_logs_auto_purge was set to FALSE (0)]
SET @@GLOBAL.binlog_expire_logs_auto_purge = OFF;
include/assert.inc [Value for binlog_expire_logs_auto_purge was set to FALSE (OFF)]
SET @@global.binlog_expire_logs_auto_purge = -1;
ERROR 42000: Variable 'binlog_expire_logs_auto_purge' can't be set to the value of '-1'
SELECT @@global.binlog_expire_logs_auto_purge;
@@global.binlog_expire_logs_auto_purge
0
SET @@global.binlog_expire_logs_auto_purge = "AAAA";
ERROR 42000: Variable 'binlog_expire_logs_auto_purge' can't be set to the value of 'AAAA'
SELECT @@global.binlog_expire_logs_auto_purge;
@@global.binlog_expire_logs_auto_purge
0
SET @@global.binlog_expire_logs_auto_purge = NULL;
ERROR 42000: Variable 'binlog_expire_logs_auto_purge' can't be set to the value of 'NULL'
SELECT @@global.binlog_expire_logs_auto_purge;
@@global.binlog_expire_logs_auto_purge
0
SET @@global.binlog_expire_logs_auto_purge = 42949672950;
ERROR 42000: Variable 'binlog_expire_logs_auto_purge' can't be set to the value of '42949672950'
SELECT @@global.binlog_expire_logs_auto_purge;
@@global.binlog_expire_logs_auto_purge
0
SET @@session.binlog_expire_logs_auto_purge = 0;
ERROR HY000: Variable 'binlog_expire_logs_auto_purge' is a GLOBAL variable and should be set with SET GLOBAL
SELECT @@session.binlog_expire_logs_auto_purge;
ERROR HY000: Variable 'binlog_expire_logs_auto_purge' is a GLOBAL variable
SELECT @@global.binlog_expire_logs_auto_purge = VARIABLE_VALUE
FROM performance_schema.global_variables
WHERE VARIABLE_NAME='binlog_expire_logs_auto_purge';
@@global.binlog_expire_logs_auto_purge = VARIABLE_VALUE
1
SELECT @@binlog_expire_logs_auto_purge = VARIABLE_VALUE
FROM performance_schema.session_variables
WHERE VARIABLE_NAME='binlog_expire_logs_auto_purge';
@@binlog_expire_logs_auto_purge = VARIABLE_VALUE
1
SET @@global.binlog_expire_logs_auto_purge = 1;
SELECT @@binlog_expire_logs_auto_purge = @@global.binlog_expire_logs_auto_purge;
@@binlog_expire_logs_auto_purge = @@global.binlog_expire_logs_auto_purge
1
SET binlog_expire_logs_auto_purge = 1;
ERROR HY000: Variable 'binlog_expire_logs_auto_purge' is a GLOBAL variable and should be set with SET GLOBAL
SELECT @@binlog_expire_logs_auto_purge;
@@binlog_expire_logs_auto_purge
1
SET local.binlog_expire_logs_auto_purge = 1;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'local.binlog_expire_logs_auto_purge = 1' at line 1
SELECT local.binlog_expire_logs_auto_purge;
ERROR 42S02: Unknown table 'local' in field list
SET global.binlog_expire_logs_auto_purge = 1;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'global.binlog_expire_logs_auto_purge = 1' at line 1
SELECT global.binlog_expire_logs_auto_purge;
ERROR 42S02: Unknown table 'global' in field list
SELECT binlog_expire_logs_auto_purge = @@global.binlog_expire_logs_auto_purge;
ERROR 42S22: Unknown column 'binlog_expire_logs_auto_purge' in 'field list'
SET @@global.binlog_expire_logs_auto_purge = 1;;
SELECT @@global.binlog_expire_logs_auto_purge;
@@global.binlog_expire_logs_auto_purge
1
