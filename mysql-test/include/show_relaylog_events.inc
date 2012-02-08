# ==== Purpose ====
#
# Execute SHOW RELAYLOG EVENTS and mask non-deterministic output.
#
# ==== Usage ====
#
# See include/show_binlog_events.inc

--let $include_filename= show_relaylog_events.inc
--source include/begin_include_file.inc

--let $is_relay_log= 1
--source include/show_events.inc

--let $include_filename= show_relaylog_events.inc
--source include/end_include_file.inc
