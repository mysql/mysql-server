SELECT COUNT(@@GLOBAL.binlog_encryption);
COUNT(@@GLOBAL.binlog_encryption)
1
SELECT COUNT(@@SESSION.binlog_encryption);
ERROR HY000: Variable 'binlog_encryption' is a GLOBAL variable
SELECT VARIABLE_NAME FROM performance_schema.global_variables WHERE VARIABLE_NAME='binlog_encryption';
VARIABLE_NAME
binlog_encryption
SELECT VARIABLE_NAME FROM performance_schema.session_variables WHERE VARIABLE_NAME='binlog_encryption';
VARIABLE_NAME
binlog_encryption
SET GLOBAL binlog_encryption= ON;
include/assert.inc ['binlog_encryption is a dynamic variable']
SET GLOBAL binlog_encryption= OFF;
include/assert.inc ['binlog_encryption should be OFF']
SET GLOBAL binlog_encryption= ON;
include/assert.inc ['binlog_encryption should be ON']
SET GLOBAL binlog_encryption= 0;
include/assert.inc ['binlog_encryption should be OFF']
SET GLOBAL binlog_encryption= 1;
include/assert.inc ['binlog_encryption should be ON']
SET GLOBAL binlog_encryption= DEFAULT;
include/assert.inc ['binlog_encryption should be OFF']
SET GLOBAL binlog_encryption= NULL;
ERROR 42000: Variable 'binlog_encryption' can't be set to the value of 'NULL'
SET GLOBAL binlog_encryption= '';
ERROR 42000: Variable 'binlog_encryption' can't be set to the value of ''
SET GLOBAL binlog_encryption= -1;
ERROR 42000: Variable 'binlog_encryption' can't be set to the value of '-1'
SET GLOBAL binlog_encryption= 1.0;
ERROR 42000: Incorrect argument type to variable 'binlog_encryption'
SET GLOBAL binlog_encryption= 'GARBAGE';
ERROR 42000: Variable 'binlog_encryption' can't be set to the value of 'GARBAGE'
SET GLOBAL binlog_encryption= 2;
ERROR 42000: Variable 'binlog_encryption' can't be set to the value of '2'
Expect value still set to "OFF"
SELECT @@global.binlog_encryption;
@@global.binlog_encryption
0
CREATE USER user1;
SET GLOBAL binlog_encryption=ON;
ERROR 42000: Access denied; you need (at least one of) the SUPER or SYSTEM_VARIABLES_ADMIN privilege(s) for this operation
ALTER INSTANCE ROTATE BINLOG MASTER KEY;
ERROR 42000: Access denied; you need (at least one of) the SUPER or BINLOG_ENCRYPTION_ADMIN privilege(s) for this operation
GRANT SYSTEM_VARIABLES_ADMIN ON *.* TO user1@'%';
SET GLOBAL binlog_encryption=ON;
ERROR 42000: Access denied; you need (at least one of) the SUPER or BINLOG_ENCRYPTION_ADMIN privilege(s) for this operation
ALTER INSTANCE ROTATE BINLOG MASTER KEY;
ERROR 42000: Access denied; you need (at least one of) the SUPER or BINLOG_ENCRYPTION_ADMIN privilege(s) for this operation
GRANT BINLOG_ENCRYPTION_ADMIN ON *.* TO user1@'%';
SET GLOBAL binlog_encryption=ON;
ALTER INSTANCE ROTATE BINLOG MASTER KEY;
REVOKE SYSTEM_VARIABLES_ADMIN ON *.* FROM user1@'%';
ALTER INSTANCE ROTATE BINLOG MASTER KEY;
SET GLOBAL binlog_encryption=OFF;
ERROR 42000: Access denied; you need (at least one of) the SUPER or SYSTEM_VARIABLES_ADMIN privilege(s) for this operation
REVOKE BINLOG_ENCRYPTION_ADMIN ON *.* FROM user1@'%';
DROP USER user1;
