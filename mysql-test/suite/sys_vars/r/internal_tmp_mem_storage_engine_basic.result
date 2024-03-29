SELECT @@global.internal_tmp_mem_storage_engine;
@@global.internal_tmp_mem_storage_engine
TempTable
SELECT @@session.internal_tmp_mem_storage_engine;
@@session.internal_tmp_mem_storage_engine
TempTable
SET @@global.internal_tmp_mem_storage_engine = 'memory';
SELECT @@global.internal_tmp_mem_storage_engine;
@@global.internal_tmp_mem_storage_engine
MEMORY
SET @@global.internal_tmp_mem_storage_engine = 'temptable';
SELECT @@global.internal_tmp_mem_storage_engine;
@@global.internal_tmp_mem_storage_engine
TempTable
SET @@session.internal_tmp_mem_storage_engine = 'memory';
SELECT @@session.internal_tmp_mem_storage_engine;
@@session.internal_tmp_mem_storage_engine
MEMORY
SET @@session.internal_tmp_mem_storage_engine = 'temptable';
SELECT @@session.internal_tmp_mem_storage_engine;
@@session.internal_tmp_mem_storage_engine
TempTable
SET @@global.internal_tmp_mem_storage_engine = 123;
ERROR 42000: Variable 'internal_tmp_mem_storage_engine' can't be set to the value of '123'
SELECT @@global.internal_tmp_mem_storage_engine;
@@global.internal_tmp_mem_storage_engine
TempTable
SET @@global.internal_tmp_mem_storage_engine = NULL;
ERROR 42000: Variable 'internal_tmp_mem_storage_engine' can't be set to the value of 'NULL'
SELECT @@global.internal_tmp_mem_storage_engine;
@@global.internal_tmp_mem_storage_engine
TempTable
SET @@global.internal_tmp_mem_storage_engine = 'foo';
ERROR 42000: Variable 'internal_tmp_mem_storage_engine' can't be set to the value of 'foo'
SELECT @@global.internal_tmp_mem_storage_engine;
@@global.internal_tmp_mem_storage_engine
TempTable
SET @@global.internal_tmp_mem_storage_engine = default;
SET @@session.internal_tmp_mem_storage_engine = default;
#
# WL#14728: internal_tmp_mem_storage_engine should
#   require SESSION_VARIABLES_ADMIN
#
CREATE USER foo@localhost, bar@localhost;
GRANT SESSION_VARIABLES_ADMIN ON *.* TO foo@localhost;
SET @@global.internal_tmp_mem_storage_engine = 123;
ERROR 42000: Access denied; you need (at least one of) the SUPER or SYSTEM_VARIABLES_ADMIN privilege(s) for this operation
SET @@session.internal_tmp_mem_storage_engine = 'memory';
SET @@global.internal_tmp_mem_storage_engine = default;
ERROR 42000: Access denied; you need (at least one of) the SUPER or SYSTEM_VARIABLES_ADMIN privilege(s) for this operation
SET @@session.internal_tmp_mem_storage_engine = default;
SET @@global.internal_tmp_mem_storage_engine = 123;
ERROR 42000: Access denied; you need (at least one of) the SUPER or SYSTEM_VARIABLES_ADMIN privilege(s) for this operation
SET @@session.internal_tmp_mem_storage_engine = 'memory';
ERROR 42000: Access denied; you need (at least one of) the SYSTEM_VARIABLES_ADMIN or SESSION_VARIABLES_ADMIN privilege(s) for this operation
SET @@global.internal_tmp_mem_storage_engine = default;
ERROR 42000: Access denied; you need (at least one of) the SUPER or SYSTEM_VARIABLES_ADMIN privilege(s) for this operation
SET @@session.internal_tmp_mem_storage_engine = default;
ERROR 42000: Access denied; you need (at least one of) the SYSTEM_VARIABLES_ADMIN or SESSION_VARIABLES_ADMIN privilege(s) for this operation
REVOKE SESSION_VARIABLES_ADMIN ON *.* FROM foo@localhost;
DROP USER foo@localhost, bar@localhost;
# End of 8.0 tests
