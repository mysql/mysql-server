RESET MASTER;
#
# 1. Save current binlog file name
include/save_binlog_position.inc
#
# 2. Generate new binlogs
FLUSH BINARY LOGS;
FLUSH BINARY LOGS;
#
# 3. Case1: Purge binlogs using 'PURGE BINARY LOGS TO'
#
# 3.1 Lock instance for backup
LOCK INSTANCE FOR BACKUP;
#
# 3.2 Purge binlogs
include/save_binlog_position.inc
FLUSH BINARY LOGS;
PURGE BINARY LOGS TO 'BINLOG_FILE';
ERROR HY000: Could not purge binary logs since another session is executing LOCK INSTANCE FOR BACKUP. Wait for that session to release the lock.
#
# 3.3 Verify that binlogs are not purged
include/assert.inc [Expected binary log file]
#
# 3.4 Unlock instance for backup
UNLOCK INSTANCE;
#
# 4. Case2: Purge binlogs using 'PURGE BINARY LOGS BEFORE'
#
# 4.1 Lock instance for backup
LOCK INSTANCE FOR BACKUP;
#
# 4.2 Purge binlogs
FLUSH BINARY LOGS;
ERROR HY000: Could not purge binary logs since another session is executing LOCK INSTANCE FOR BACKUP. Wait for that session to release the lock.
#
# 4.3 Verify that binlogs are not purged
include/assert.inc [Expected binary log file]
#
# 4.4 Unlock instance
UNLOCK INSTANCE;
