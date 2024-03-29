include/assert.inc [Default GLOBAL value is 3]
include/assert.inc [Default SESSION value 3]
SET @@PERSIST_ONLY.binlog_transaction_compression_level_zstd = DEFAULT;
include/assert.inc [Reported value matches the one set (set DEFAULT in PERSIST_ONLY scope)]
SET @@PERSIST_ONLY.binlog_transaction_compression_level_zstd = 22;
include/assert.inc [Reported value matches the one set (set 22 in PERSIST_ONLY scope)]
SET @@PERSIST_ONLY.binlog_transaction_compression_level_zstd = 1;
include/assert.inc [Reported value matches the one set (set 1 in PERSIST_ONLY scope)]
SET GLOBAL binlog_transaction_compression_level_zstd = 0;
Warnings:
Warning	1292	Truncated incorrect binlog_transaction_compression_level_zstd value: '0'
SELECT @@global.binlog_transaction_compression_level_zstd;
@@global.binlog_transaction_compression_level_zstd
1
SET SESSION binlog_transaction_compression_level_zstd = 0;
Warnings:
Warning	1292	Truncated incorrect binlog_transaction_compression_level_zstd value: '0'
SELECT @@session.binlog_transaction_compression_level_zstd;
@@session.binlog_transaction_compression_level_zstd
1
SET GLOBAL binlog_transaction_compression_level_zstd = 23;
Warnings:
Warning	1292	Truncated incorrect binlog_transaction_compression_level_zstd value: '23'
SELECT @@global.binlog_transaction_compression_level_zstd;
@@global.binlog_transaction_compression_level_zstd
22
SET SESSION binlog_transaction_compression_level_zstd = 23;
Warnings:
Warning	1292	Truncated incorrect binlog_transaction_compression_level_zstd value: '23'
SELECT @@session.binlog_transaction_compression_level_zstd;
@@session.binlog_transaction_compression_level_zstd
22
CREATE USER user1;
SET GLOBAL binlog_transaction_compression_level_zstd = 10;
ERROR 42000: Access denied; you need (at least one of) the SUPER or SYSTEM_VARIABLES_ADMIN privilege(s) for this operation
GRANT SYSTEM_VARIABLES_ADMIN ON *.* TO user1@'%';
SET GLOBAL binlog_transaction_compression_level_zstd= 10;
DROP USER user1;
BEGIN;
SET SESSION binlog_transaction_compression_level_zstd=11;
ERROR HY000: The system variable binlog_transaction_compression_level_zstd cannot be set when there is an ongoing transaction.
SET GLOBAL binlog_transaction_compression_level_zstd=11;
SET PERSIST binlog_transaction_compression_level_zstd=11;
SET PERSIST_ONLY binlog_transaction_compression_level_zstd=11;
ROLLBACK;
RESET PERSIST;
SET @@session.binlog_transaction_compression_level_zstd = 3;
SET @@global.binlog_transaction_compression_level_zstd = 3;
