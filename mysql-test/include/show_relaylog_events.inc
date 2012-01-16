# ==== Purpose ====
#
# Execute SHOW RELAYLOG EVENTS and mask non-deterministic output.
#
# ==== Usage ====
#
# See include/show_binlog_events.inc

--let $is_relay_log= 1
--source include/show_events.inc
