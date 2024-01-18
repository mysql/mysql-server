# ==== Purpose ====
#
# Used by scripts testing XA crash recovery, it cleans up temporary files
# created by `common/xa_crash_safe/utility_functions.inc`.
#
# ==== References ====
#
# WL#11300: Crash-safe XA + binary log
#
# Related tests;
#   see common/xa_crash_safe/setup.inc
#
--remove_file $exec_xa_transaction_body
--remove_file $clean_up_xa_transactions
--remove_file $error_on_recovery_for_normal_transaction
--remove_file $error_on_recovery_for_xa_transaction
