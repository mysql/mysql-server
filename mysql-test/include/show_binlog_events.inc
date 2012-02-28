# ==== Purpose ====
#
# Execute SHOW BINLOG EVENTS and mask non-deterministic output.
#
# ==== Usage ====
#
# [--let $binlog_file= [<FILENAME> | LAST]]
# [--let $binlog_start= <POSITION> ]
# [--let $binlog_limit= 1, 3 ]
# [--let $keep_gtid_events= 1]
# --source include/show_binlog_events.inc
#
# Parameters:
#
#   $binlog_file
#     Filename for the 'IN' clause of SHOW BINLOG EVENTS. If none
#     given, no argument is given to SHOW BINLOG EVENTS, meaning that
#     it uses the first binlog. If you set this to "LAST", it prints
#     the last binlog (according to SHOW MASTER STATUS).
#
#   $binlog_start
#     Position for the 'FROM' clause of SHOW BINLOG EVENTS. If none
#     given, starts right after the Format_description_log_event.
#
#   $binlog_limit
#     Limit for the 'LIMIT' clause of SHOW BINLOG EVENTS, i.e.:
#       $binlog_limit= 3    -- print three events
#       $binlog_limit= 4, 3 -- skip four events, print the three next events
#
#   $keep_gtid_events
#     By default, Gtid_log_event and Previous_gtid_log_event are
#     filtered out, so that the output is independent of whether GTIDs
#     are enabled or not.  If this flag is set, events are kept but
#     the actual GTID values are masked out.

--let $include_filename= show_binlog_events.inc
--source include/begin_include_file.inc

--let $is_relay_log= 0
--source include/show_events.inc

--let $include_filename= show_binlog_events.inc
--source include/end_include_file.inc
