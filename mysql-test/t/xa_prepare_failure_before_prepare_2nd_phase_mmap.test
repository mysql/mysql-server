# ==== Purpose ====
#
# This script tests server behavior when a crash occurs during the
# execution of `XA PREPARE`, after the transaction was prepared in the
# internal transaction coordinator and just before engines mark the
# transaction as prepared in the TC, when binary logging is disabled.
#
# For further details, check xa_prepare_failure_before_prepare_2nd_phase.test
#
--source t/xa_prepare_failure_before_prepare_2nd_phase.test
