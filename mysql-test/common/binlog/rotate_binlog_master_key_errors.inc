# ==== Purpose ====
#
# To cover an error branch when executing the command
# 'ALTER INSTANCE ROTATE BINLOG MASTER KEY' on a server
# with binary log enabled.
#
# ==== Implementation ====
#
# 1. Set a debug point.
# 2. Verify that an expected error is emitted to the client side on the
#    session when executing the command 'ALTER INSTANCE ROTATE BINLOG
#    MASTER KEY'.
# 3. Remove the debug point.
# 4. Verify that 'ALTER INSTANCE ROTATE BINLOG MASTER KEY' works after
#    an error happens in the last execution.
#
# ==== Usage ====
#
# --let $MASTER_DATADIR= `select @@datadir`
# --let $debug_point=
# --let $expected_error=
# --let $first_encrypted_binary_log=
# --let $current_master_key_id=
# --let $current_comparison_digit=
# --let $rotated_master_key_id=
# --let $rotated_comparison_digit=
# --let $rotate_binary_log=
# --source common/binlog/rotate_binlog_master_key_errors.inc
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
# $rotate_binary_log
#   Set it to 1 if you need to rotate binary log after executing
#   'ALTER INSTANCE ROTATE BINLOG MASTER KEY' throws an expected
#   error. Otherwise set it to 0.
#
# ==== References ====
#
# Wl#12080 Add support to binary log encryption key rotation and cleanup

--source include/add_debug_point.inc
--replace_result \\ /
--error $expected_error
ALTER INSTANCE ROTATE BINLOG MASTER KEY;
--source include/remove_debug_point.inc

if ($rotate_binary_log)
{
  FLUSH BINARY LOGS;
}

--source common/binlog/rotate_binlog_master_key_work.inc
