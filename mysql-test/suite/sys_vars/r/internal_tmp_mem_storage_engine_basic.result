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
