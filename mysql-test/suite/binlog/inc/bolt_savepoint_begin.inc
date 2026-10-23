# Start one case of binlog_bolt_savepoint_matrix.
#
# Input: $sp_case (case name, echoed).
# Rotates to a fresh active binlog so that the checks in
# bolt_savepoint_check.inc see only this case's transaction.
--echo # $sp_case
--disable_query_log
FLUSH BINARY LOGS;
--enable_query_log
--let $sp_bolt_before = query_get_value(SHOW GLOBAL STATUS LIKE 'Binlog_large_transaction_optimization_count', Value, 1)
--let $sp_missed_before = query_get_value(SHOW GLOBAL STATUS LIKE 'Binlog_large_transaction_optimization_missed_count', Value, 1)
