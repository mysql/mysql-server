include/assert.inc [Default GLOBAL value is OFF]
include/assert.inc [Default SESSION value is OFF]
SET @@PERSIST_ONLY.binlog_transaction_compression = DEFAULT;
include/assert.inc [Reported value matches the one set (set DEFAULT in PERSIST_ONLY scope)]
SET @@PERSIST_ONLY.binlog_transaction_compression = OFF;
include/assert.inc [Reported value matches the one set (set OFF in PERSIST_ONLY scope)]
SET @@PERSIST_ONLY.binlog_transaction_compression = FALSE;
include/assert.inc [Reported value matches the one set (set FALSE in PERSIST_ONLY scope)]
SET @@PERSIST_ONLY.binlog_transaction_compression = 0;
include/assert.inc [Reported value matches the one set (set 0 in PERSIST_ONLY scope)]
SET @@PERSIST_ONLY.binlog_transaction_compression = ON;
include/assert.inc [Reported value matches the one set (set ON in PERSIST_ONLY scope)]
SET @@PERSIST_ONLY.binlog_transaction_compression = TRUE;
include/assert.inc [Reported value matches the one set (set TRUE in PERSIST_ONLY scope)]
SET @@PERSIST_ONLY.binlog_transaction_compression = 1;
include/assert.inc [Reported value matches the one set (set 1 in PERSIST_ONLY scope)]
CREATE USER user1;
SET GLOBAL binlog_transaction_compression = ON;
ERROR 42000: Access denied; you need (at least one of) the SUPER or SYSTEM_VARIABLES_ADMIN privilege(s) for this operation
GRANT SYSTEM_VARIABLES_ADMIN ON *.* TO user1@'%';
SET GLOBAL binlog_transaction_compression=ON;
DROP USER user1;
BEGIN;
SET SESSION binlog_transaction_compression=ON;
ERROR HY000: The system variable binlog_transaction_compression cannot be set when there is an ongoing transaction.
SET GLOBAL binlog_transaction_compression=ON;
SET PERSIST binlog_transaction_compression=ON;
SET PERSIST_ONLY binlog_transaction_compression=ON;
ROLLBACK;
RESET PERSIST;
SET @@session.binlog_transaction_compression = 0;
SET @@global.binlog_transaction_compression = 0;
