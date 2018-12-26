# ==== Purpose ====
#
# Auxiliary file used by transaction_gtid.test
#
# Invoked between transactions in order to reset the state:
# - set GTID_NEXT to AUTOMATIC since this is required after
#   any transaction that has GTID_NEXT=UUID:NUMBER
# - RESET MASTER in order to clear @@global.gtid_executed, so
#   that the same GTID can be executed again.
# - truncate the performance_schema.events_transaction_* tables
#
# All this is done on the connection 'server_1'.

--disable_query_log
--connection server_1
RESET MASTER;
TRUNCATE TABLE performance_schema.events_transactions_history;
TRUNCATE TABLE performance_schema.events_transactions_current;
--enable_query_log
--connection default
