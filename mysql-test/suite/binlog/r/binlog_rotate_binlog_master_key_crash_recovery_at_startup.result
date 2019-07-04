CREATE TABLE t1 (a INT) ENGINE=InnoDB;
INSERT INTO t1 VALUES(1);
include/assert.inc [the first encrypted binary log is encrypted using first master key]
ALTER INSTANCE ROTATE BINLOG MASTER KEY;
INSERT INTO t1 VALUES(2);
# Part 1: crash_before_set_old_master_key_seqno_on_keyring
# Adding debug point 'crash_before_set_old_master_key_seqno_on_keyring' to @@GLOBAL.debug
ALTER INSTANCE ROTATE BINLOG MASTER KEY;
ERROR HY000: Lost connection to MySQL server during query
# Restart the master server
include/assert.inc [Verify that 'binlog_encryption' is enabled during above crash recovery.]
include/assert.inc [the before binary log is encrypted using current master key with key id: 2]
# Adding debug point 'verify_unusable_encryption_keys_are_purged' to @@GLOBAL.debug
ALTER INSTANCE ROTATE BINLOG MASTER KEY;
# Removing debug point 'verify_unusable_encryption_keys_are_purged' from @@GLOBAL.debug
INSERT INTO t1 VALUES (1);
include/assert.inc [the after binary log is encrypted using rotated master key with key id: 3]
include/assert.inc [the before binary log is re-encrypted using rotated master key with key id: 3]
include/assert.inc [the first encrypted binary log is re-encrypted using rotated master key with key id: 3]
# Part 2: crash_after_set_old_master_key_seqno_on_keyring
# Adding debug point 'crash_after_set_old_master_key_seqno_on_keyring' to @@GLOBAL.debug
ALTER INSTANCE ROTATE BINLOG MASTER KEY;
ERROR HY000: Lost connection to MySQL server during query
# Restart the master server
include/assert.inc [Verify that 'binlog_encryption' is enabled during above crash recovery.]
include/assert.inc [the before binary log is encrypted using current master key with key id: 4]
# Adding debug point 'verify_unusable_encryption_keys_are_purged' to @@GLOBAL.debug
ALTER INSTANCE ROTATE BINLOG MASTER KEY;
# Removing debug point 'verify_unusable_encryption_keys_are_purged' from @@GLOBAL.debug
INSERT INTO t1 VALUES (1);
include/assert.inc [the after binary log is encrypted using rotated master key with key id: 5]
include/assert.inc [the before binary log is re-encrypted using rotated master key with key id: 5]
include/assert.inc [the first encrypted binary log is re-encrypted using rotated master key with key id: 5]
# Part 3: crash_after_set_new_master_key_seqno_on_keyring
# Adding debug point 'crash_after_set_new_master_key_seqno_on_keyring' to @@GLOBAL.debug
ALTER INSTANCE ROTATE BINLOG MASTER KEY;
ERROR HY000: Lost connection to MySQL server during query
# Restart the master server
include/assert.inc [Verify that 'binlog_encryption' is enabled during above crash recovery.]
include/assert.inc [the before binary log is encrypted using current master key with key id: 6]
# Adding debug point 'verify_unusable_encryption_keys_are_purged' to @@GLOBAL.debug
ALTER INSTANCE ROTATE BINLOG MASTER KEY;
# Removing debug point 'verify_unusable_encryption_keys_are_purged' from @@GLOBAL.debug
INSERT INTO t1 VALUES (1);
include/assert.inc [the after binary log is encrypted using rotated master key with key id: 7]
include/assert.inc [the before binary log is re-encrypted using rotated master key with key id: 7]
include/assert.inc [the first encrypted binary log is re-encrypted using rotated master key with key id: 7]
# Part 4: crash_after_generate_new_master_key_on_keyring
# Adding debug point 'crash_after_generate_new_master_key_on_keyring' to @@GLOBAL.debug
ALTER INSTANCE ROTATE BINLOG MASTER KEY;
ERROR HY000: Lost connection to MySQL server during query
# Restart the master server
include/assert.inc [Verify that 'binlog_encryption' is enabled during above crash recovery.]
include/assert.inc [the before binary log is encrypted using current master key with key id: 8]
# Adding debug point 'verify_unusable_encryption_keys_are_purged' to @@GLOBAL.debug
ALTER INSTANCE ROTATE BINLOG MASTER KEY;
# Removing debug point 'verify_unusable_encryption_keys_are_purged' from @@GLOBAL.debug
INSERT INTO t1 VALUES (1);
include/assert.inc [the after binary log is encrypted using rotated master key with key id: 9]
include/assert.inc [the before binary log is re-encrypted using rotated master key with key id: 9]
include/assert.inc [the first encrypted binary log is re-encrypted using rotated master key with key id: 9]
# Part 5: crash_after_remove_master_key_seqno_from_keyring
# Adding debug point 'crash_after_remove_master_key_seqno_from_keyring' to @@GLOBAL.debug
ALTER INSTANCE ROTATE BINLOG MASTER KEY;
ERROR HY000: Lost connection to MySQL server during query
# Restart the master server
include/assert.inc [Verify that 'binlog_encryption' is enabled during above crash recovery.]
include/assert.inc [the before binary log is encrypted using current master key with key id: 10]
# Adding debug point 'verify_unusable_encryption_keys_are_purged' to @@GLOBAL.debug
ALTER INSTANCE ROTATE BINLOG MASTER KEY;
# Removing debug point 'verify_unusable_encryption_keys_are_purged' from @@GLOBAL.debug
INSERT INTO t1 VALUES (1);
include/assert.inc [the after binary log is encrypted using rotated master key with key id: 11]
include/assert.inc [the before binary log is re-encrypted using rotated master key with key id: 11]
include/assert.inc [the first encrypted binary log is re-encrypted using rotated master key with key id: 11]
# Part 6: crash_after_set_master_key_seqno_on_keyring
# Adding debug point 'crash_after_set_master_key_seqno_on_keyring' to @@GLOBAL.debug
ALTER INSTANCE ROTATE BINLOG MASTER KEY;
ERROR HY000: Lost connection to MySQL server during query
# Restart the master server
include/assert.inc [Verify that 'binlog_encryption' is enabled during above crash recovery.]
include/assert.inc [the before binary log is encrypted using current master key with key id: 12]
# Adding debug point 'verify_unusable_encryption_keys_are_purged' to @@GLOBAL.debug
ALTER INSTANCE ROTATE BINLOG MASTER KEY;
# Removing debug point 'verify_unusable_encryption_keys_are_purged' from @@GLOBAL.debug
INSERT INTO t1 VALUES (1);
include/assert.inc [the after binary log is encrypted using rotated master key with key id: 13]
include/assert.inc [the before binary log is re-encrypted using rotated master key with key id: 13]
include/assert.inc [the first encrypted binary log is re-encrypted using rotated master key with key id: 13]
# Part 7: crash_after_remove_old_master_key_seqno_from_keyring
# Adding debug point 'crash_after_remove_old_master_key_seqno_from_keyring' to @@GLOBAL.debug
ALTER INSTANCE ROTATE BINLOG MASTER KEY;
ERROR HY000: Lost connection to MySQL server during query
# Restart the master server
include/assert.inc [Verify that 'binlog_encryption' is enabled during above crash recovery.]
include/assert.inc [the before binary log is encrypted using current master key with key id: 14]
# Adding debug point 'verify_unusable_encryption_keys_are_purged' to @@GLOBAL.debug
ALTER INSTANCE ROTATE BINLOG MASTER KEY;
# Removing debug point 'verify_unusable_encryption_keys_are_purged' from @@GLOBAL.debug
INSERT INTO t1 VALUES (1);
include/assert.inc [the after binary log is encrypted using rotated master key with key id: 15]
include/assert.inc [the before binary log is re-encrypted using rotated master key with key id: 15]
include/assert.inc [the first encrypted binary log is re-encrypted using rotated master key with key id: 15]
# Part 8: crash_after_remove_new_master_key_seqno_from_keyring
# Adding debug point 'crash_after_remove_new_master_key_seqno_from_keyring' to @@GLOBAL.debug
ALTER INSTANCE ROTATE BINLOG MASTER KEY;
ERROR HY000: Lost connection to MySQL server during query
# Restart the master server
include/assert.inc [Verify that 'binlog_encryption' is enabled during above crash recovery.]
include/assert.inc [the before binary log is encrypted using current master key with key id: 16]
# Adding debug point 'verify_unusable_encryption_keys_are_purged' to @@GLOBAL.debug
ALTER INSTANCE ROTATE BINLOG MASTER KEY;
# Removing debug point 'verify_unusable_encryption_keys_are_purged' from @@GLOBAL.debug
INSERT INTO t1 VALUES (1);
include/assert.inc [the after binary log is encrypted using rotated master key with key id: 17]
include/assert.inc [the before binary log is re-encrypted using rotated master key with key id: 17]
include/assert.inc [the first encrypted binary log is re-encrypted using rotated master key with key id: 17]
DROP TABLE t1;
