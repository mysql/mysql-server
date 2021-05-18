SET @@GLOBAL.binlog_encryption=ON;
include/assert.inc [the first encrypted binary log is encrypted using first master key]
CREATE TABLE t1 (a INT) ENGINE=InnoDB;
INSERT INTO t1 VALUES(1);
ALTER INSTANCE ROTATE BINLOG MASTER KEY;
INSERT INTO t1 VALUES(2);
# Part 1: crash_before_set_old_master_key_seqno_on_keyring
# Adding debug point 'crash_before_set_old_master_key_seqno_on_keyring' to @@GLOBAL.debug
ALTER INSTANCE ROTATE BINLOG MASTER KEY;
ERROR HY000: Lost connection to MySQL server during query
# Restart the master server
include/assert.inc ['binlog_encryption' is off after above crash recovery.]
#
# Verify that an error 'ER_RPL_ENCRYPTION_MASTER_KEY_RECOVERY_FAILED'
# is emitted to the client side when executing the command
# 'SET @@GLOBAL.binlog_encryption=ON' if there is an error
# when fetching master key seqno from keyring.
#
# Adding debug point 'fail_to_fetch_master_key_seqno_from_keyring' to @@GLOBAL.debug
SET @@GLOBAL.binlog_encryption=ON;
ERROR HY000: Unable to recover binlog encryption master key, please check if keyring plugin is loaded.
# Removing debug point 'fail_to_fetch_master_key_seqno_from_keyring' from @@GLOBAL.debug
#
# Verify that an error 'ER_RPL_ENCRYPTION_MASTER_KEY_RECOVERY_FAILED'
# is emitted to the client side when executing the command
# 'SET @@GLOBAL.binlog_encryption=ON' if there is an error
# when fetching old master key seqno from keyring.
#
# Adding debug point 'fail_to_fetch_old_master_key_seqno_from_keyring' to @@GLOBAL.debug
SET @@GLOBAL.binlog_encryption=ON;
ERROR HY000: Unable to recover binlog encryption master key, please check if keyring plugin is loaded.
# Removing debug point 'fail_to_fetch_old_master_key_seqno_from_keyring' from @@GLOBAL.debug
SET @@GLOBAL.binlog_encryption=ON;
include/assert.inc [Verify that 'binlog_encryption' can be enabled after above crash recovery.]
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
include/assert.inc ['binlog_encryption' is off after above crash recovery.]
#
# Verify that an error 'ER_binlog_MASTER_KEY_RECOVERY_OUT_OF_COMBINATION'
# is emitted to the client side when executing the command
# 'SET @@GLOBAL.binlog_encryption=ON' if the combination of
# values of new_master_key_seqno, master_key_seqno and
# old_master_key_seqno are wrong when recovering binlog master key.
#
# Adding debug point 'simulate_master_key_recovery_out_of_combination' to @@GLOBAL.debug
SET @@GLOBAL.binlog_encryption=ON;
ERROR HY000: Unable to recover binary log master key, the combination of new_master_key_seqno=0, master_key_seqno=4 and old_master_key_seqno=3 are wrong.
# Removing debug point 'simulate_master_key_recovery_out_of_combination' from @@GLOBAL.debug
SET @@GLOBAL.binlog_encryption=ON;
include/assert.inc [Verify that 'binlog_encryption' can be enabled after above crash recovery.]
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
include/assert.inc ['binlog_encryption' is off after above crash recovery.]
#
# Verify that an error 'ER_RPL_ENCRYPTION_MASTER_KEY_RECOVERY_FAILED'
# is emitted to the client side when executing the command
# 'SET @@GLOBAL.binlog_encryption=ON' if there is an error
# when fetching new master key from keyring.
#
# Adding debug point 'fail_to_fetch_new_master_key_from_keyring' to @@GLOBAL.debug
SET @@GLOBAL.binlog_encryption=ON;
ERROR HY000: Unable to recover binlog encryption master key, please check if keyring plugin is loaded.
# Removing debug point 'fail_to_fetch_new_master_key_from_keyring' from @@GLOBAL.debug
SET @@GLOBAL.binlog_encryption=ON;
include/assert.inc [Verify that 'binlog_encryption' can be enabled after above crash recovery.]
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
include/assert.inc ['binlog_encryption' is off after above crash recovery.]
#
# Verify that an error 'ER_RPL_ENCRYPTION_MASTER_KEY_RECOVERY_FAILED'
# is emitted to the client side when executing the command
# 'SET @@GLOBAL.binlog_encryption=ON' if there is an error
# when fetching new master key seqno from keyring.
#
# Adding debug point 'fail_to_fetch_new_master_key_seqno_from_keyring' to @@GLOBAL.debug
SET @@GLOBAL.binlog_encryption=ON;
ERROR HY000: Unable to recover binlog encryption master key, please check if keyring plugin is loaded.
# Removing debug point 'fail_to_fetch_new_master_key_seqno_from_keyring' from @@GLOBAL.debug
SET @@GLOBAL.binlog_encryption=ON;
include/assert.inc [Verify that 'binlog_encryption' can be enabled after above crash recovery.]
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
include/assert.inc ['binlog_encryption' is off after above crash recovery.]
SET @@GLOBAL.binlog_encryption=ON;
include/assert.inc [Verify that 'binlog_encryption' can be enabled after above crash recovery.]
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
include/assert.inc ['binlog_encryption' is off after above crash recovery.]
SET @@GLOBAL.binlog_encryption=ON;
include/assert.inc [Verify that 'binlog_encryption' can be enabled after above crash recovery.]
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
include/assert.inc ['binlog_encryption' is off after above crash recovery.]
SET @@GLOBAL.binlog_encryption=ON;
include/assert.inc [Verify that 'binlog_encryption' can be enabled after above crash recovery.]
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
include/assert.inc ['binlog_encryption' is off after above crash recovery.]
SET @@GLOBAL.binlog_encryption=ON;
include/assert.inc [Verify that 'binlog_encryption' can be enabled after above crash recovery.]
include/assert.inc [the before binary log is encrypted using current master key with key id: 16]
# Adding debug point 'verify_unusable_encryption_keys_are_purged' to @@GLOBAL.debug
ALTER INSTANCE ROTATE BINLOG MASTER KEY;
# Removing debug point 'verify_unusable_encryption_keys_are_purged' from @@GLOBAL.debug
INSERT INTO t1 VALUES (1);
include/assert.inc [the after binary log is encrypted using rotated master key with key id: 17]
include/assert.inc [the before binary log is re-encrypted using rotated master key with key id: 17]
include/assert.inc [the first encrypted binary log is re-encrypted using rotated master key with key id: 17]
# Part 9: crash_after_set_old_master_key_seqno_on_keyring
# Adding debug point 'crash_after_set_old_master_key_seqno_on_keyring' to @@GLOBAL.debug
ALTER INSTANCE ROTATE BINLOG MASTER KEY;
ERROR HY000: Lost connection to MySQL server during query
# Restart the master server
include/assert.inc ['binlog_encryption' is off after above crash recovery.]
#
# Verify that an error 'ER_RPL_ENCRYPTION_FAILED_TO_STORE_KEY'
# is emitted to the client side when executing the command
# 'SET @@GLOBAL.binlog_encryption=ON' if there is an error
# on setting new master key seqno on keyring when recovering
# binlog master key.
#
# Adding debug point 'fail_to_set_new_master_key_seqno_on_keyring' to @@GLOBAL.debug
SET @@GLOBAL.binlog_encryption=ON;
ERROR HY000: Failed to store key, please check if keyring plugin is loaded.
# Removing debug point 'fail_to_set_new_master_key_seqno_on_keyring' from @@GLOBAL.debug
SET @@GLOBAL.binlog_encryption=ON;
include/assert.inc [Verify that 'binlog_encryption' can be enabled after above crash recovery.]
include/assert.inc [the before binary log is encrypted using current master key with key id: 18]
# Adding debug point 'verify_unusable_encryption_keys_are_purged' to @@GLOBAL.debug
ALTER INSTANCE ROTATE BINLOG MASTER KEY;
# Removing debug point 'verify_unusable_encryption_keys_are_purged' from @@GLOBAL.debug
INSERT INTO t1 VALUES (1);
include/assert.inc [the after binary log is encrypted using rotated master key with key id: 19]
include/assert.inc [the before binary log is re-encrypted using rotated master key with key id: 19]
include/assert.inc [the first encrypted binary log is re-encrypted using rotated master key with key id: 19]
DROP TABLE t1;
SET @@GLOBAL.binlog_encryption=DEFAULT_BINLOG_ENCRYPTION;
