# ==== Purpose ====
#
# Ensure that the server has ndb_log_transaction_dependency enabled.
#
# ==== Usage ====
#
# --source include/have_ndb_binlog_trans_dependency.inc

if (`SELECT @@GLOBAL.ndb_log_transaction_dependency = 'OFF'`)
{
  --skip Test requires ndb_log_transaction_dependency enabled
}
