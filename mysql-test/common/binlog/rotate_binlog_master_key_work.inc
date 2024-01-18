# ==== Purpose ====
#
# To verify that 'ALTER INSTANCE ROTATE BINLOG MASTER KEY' works as expected.
#
# ==== Implementation ====
#
# 1. Verify that the before (latest) binary log is encrypted using
#    current master key.
# 2. Set debug point to verify_unusable_encryption_keys_are_purged.
# 3. Execute 'ALTER INSTANCE ROTATE BINLOG MASTER KEY'.
# 4. Verify that the new generated binary log is encrypted using
#    rotated master key.
# 5. Verify that the before binary log is re-encrypted using
#    rotated master key.
# 6. Verify that the first encrypted binary log is re-encrypted
#    using rotated master key.
# 
# ==== Usage ====
#
# --let $MASTER_DATADIR= `select @@datadir`
# --let $first_encrypted_binary_log=
# --let $current_master_key_id=
# --let $current_comparison_digit=
# --let $rotated_master_key_id=
# --let $rotated_comparison_digit=
# --source common/binlog/rotate_binlog_master_key_work.inc
#
# Parameters:
#
# $first_encrypted_binary_log
#   The first encrypted binary log.
#
# $current_master_key_id
#   The master key id before rotating binlog master key.
#
# $current_comparison_digit
#   The comparison digit of $current_master_key_id
#
# $rotated_master_key_id
#   The master key id after rotating binlog master key.
#
# $rotated_comparison_digit
#   The comparison digit of $rotated_master_key_id
#
# ==== References ====
#
# Wl#12080 Add support to binary log encryption key rotation and cleanup


--let $before_binary_log=query_get_value(SHOW BINARY LOG STATUS, File, 1)
--let $rpl_log_file=$MASTER_DATADIR$before_binary_log
--source include/rpl/get_log_encryption_key_id.inc
--let $assert_text=the before binary log is encrypted using current master key with key id: $current_master_key_id
--let $assert_cond= RIGHT("$rpl_encryption_key_id", $current_comparison_digit) = "_$current_master_key_id"
--source include/assert.inc

--let $debug_point=verify_unusable_encryption_keys_are_purged
--source include/add_debug_point.inc
ALTER INSTANCE ROTATE BINLOG MASTER KEY;
--source include/remove_debug_point.inc
INSERT INTO t1 VALUES (1);

--let $after_binary_log=query_get_value(SHOW BINARY LOG STATUS, File, 1)
--let $rpl_log_file=$MASTER_DATADIR$after_binary_log
--source include/rpl/get_log_encryption_key_id.inc
--let $assert_text=the after binary log is encrypted using rotated master key with key id: $rotated_master_key_id
--let $assert_cond= RIGHT("$rpl_encryption_key_id", $rotated_comparison_digit) = "_$rotated_master_key_id"
--source include/assert.inc

--let $rpl_log_file=$MASTER_DATADIR$before_binary_log
--source include/rpl/get_log_encryption_key_id.inc
--let $assert_text=the before binary log is re-encrypted using rotated master key with key id: $rotated_master_key_id
--let $assert_cond= RIGHT("$rpl_encryption_key_id", $rotated_comparison_digit) = "_$rotated_master_key_id"
--source include/assert.inc

--let $rpl_log_file=$MASTER_DATADIR$first_encrypted_binary_log
--source include/rpl/get_log_encryption_key_id.inc
--let $assert_text=the first encrypted binary log is re-encrypted using rotated master key with key id: $rotated_master_key_id
--let $assert_cond= RIGHT("$rpl_encryption_key_id", $rotated_comparison_digit) = "_$rotated_master_key_id"
--source include/assert.inc
