# ==== Purpose ====
#
# Used by scripts testing XA crash recovery, it cleans up the alternative
# connection used in
# `execute_to_conditional_timestamp_sync_point.inc`. When run with the PS
# protocol, related tests would fail due to the connection not being
# properly reaped.
#
# ==== References ====
#
# WL#11300: Crash-safe XA + binary log
#
# Related tests;
#   see common/xa_crash_safe/setup.inc
#
--connection $statement_connection
--error CR_SERVER_LOST
--reap
--disconnect $statement_connection
--connection default
