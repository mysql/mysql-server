# ==== Purpose ====
#
# To cover an error branch when executing the command
# 'ALTER INSTANCE ROTATE BINLOG MASTER KEY' on a binlogless slave.
#
# ==== Implementation ====
#
# 1. Set a debug point.
# 2. Verify that an expected error is emitted to the client side on the
#    session on binlogless slave when executing the command
#    'ALTER INSTANCE ROTATE BINLOG MASTER KEY'.
# 3. Remove the debug point.
# 4. Verify that 'ALTER INSTANCE ROTATE BINLOG MASTER KEY' works after
#    encountering the above error.
#
# ==== Usage ====
#
# --let $SLAVE_DATADIR= `select @@datadir`
# --let $relay_log_base=`SELECT @@GLOBAL.relay_log`
# --let $first_relay_log_suffix=.000001
# --let $debug_point=
# --let $expected_error=
# --let $current_master_key_id=
# --let $rotated_master_key_id=
# --let $before_relay_log_suffix=
# --let $after_relay_log_suffix=
# --source suite/rpl/include/rpl_rotate_binlog_master_key_errors.inc
#
# Parameters:
#
# $debug_point
#   Set a debug point to cause 'ALTER INSTANCE ROTATE BINLOG MASTER KEY'
#   throw an expected error specified by $expected_error.
#
# $expected_error
#   Specify an expected error while executing
#   'ALTER INSTANCE ROTATE BINLOG MASTER KEY'
#   with a debug_point set by $debug_point.
#
# $current_master_key_id
#   The master key id before rotating binlog master key.
#
# $rotated_master_key_id
#   The master key id after rotating binlog master key.
#
# $before_relay_log_suffix
#   The relay log suffix before rotating binlog master key.
#
# $after_relay_log_suffix
#   The relay log suffix after rotating binlog master key.
#
# ==== References ====
#
# Wl#12080 Add support to binary log encryption key rotation and cleanup

--source include/add_debug_point.inc
--replace_result \\ /
--error $expected_error
ALTER INSTANCE ROTATE BINLOG MASTER KEY;
--source include/remove_debug_point.inc

--let $rpl_log_file=$SLAVE_DATADIR$relay_log_base$before_relay_log_suffix
--source include/rpl_get_log_encryption_key_id.inc
--let $assert_text=The before relay log is encrypted using current master key with key id: $current_master_key_id
--let $assert_cond= RIGHT("$rpl_encryption_key_id", 2) = "_$current_master_key_id"
--source include/assert.inc

--let $debug_point=verify_unusable_encryption_keys_are_purged
--source include/add_debug_point.inc
ALTER INSTANCE ROTATE BINLOG MASTER KEY;
--source include/remove_debug_point.inc

--let $rpl_log_file=$SLAVE_DATADIR$relay_log_base$after_relay_log_suffix
--source include/rpl_get_log_encryption_key_id.inc
--let $assert_text=The after relay log is encrypted using rotated master key with key id: $rotated_master_key_id
--let $assert_cond= RIGHT("$rpl_encryption_key_id", 2) = "_$rotated_master_key_id"
--source include/assert.inc

--let $rpl_log_file=$SLAVE_DATADIR$relay_log_base$before_relay_log_suffix
--source include/rpl_get_log_encryption_key_id.inc
--let $assert_text=The before relay log is re-encrypted using rotated master key with key id: $rotated_master_key_id
--let $assert_cond= RIGHT("$rpl_encryption_key_id", 2) = "_$rotated_master_key_id"
--source include/assert.inc

--let $rpl_log_file=$SLAVE_DATADIR$relay_log_base$first_relay_log_suffix
--source include/rpl_get_log_encryption_key_id.inc
--let $assert_text=The first relay log is re-encrypted using rotated master key with key id: $rotated_master_key_id
--let $assert_cond= RIGHT("$rpl_encryption_key_id", 2) = "_$rotated_master_key_id"
--source include/assert.inc
