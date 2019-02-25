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
# [--let $keep_view_change_events= 1]
# [--let $mask_binlog_commit_events= 1]
# [--let $mask_anonymous_gtid_events= 1]
# [--let $mask_user_password_events= 1]
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
#
#   $keep_view_change_events
#     By default, View Change Events are filtered out, so that the output is
#     independent of Group Replication being enabled. If this flag is set,
#     events are kept.
#
#   $mask_binlog_commit_events
#     If specified, any Xid_log_event is masked as a
#     Query_log_event(COMMIT), to make the output uniform for
#     transactional and non-transactional engines.
#
#   $show_binlog_events_verbose
#     Print the SHOW BINLOG EVENT statement, with filenames and positions
#     masked out.
#
#   $show_binlog_events_mask_columns
#     By default, this script masks columns 2, 4, and 5 (offset,
#     server_id, and end_log_pos). To mask a different set of columns,
#     set $show_binlog_events_mask_columns to a comma-separated list
#     of column numbers. The contents of a masked column is replaced
#     by a single '#' character.
#
#   $mask_anonymous_gtid_event
#     Mask ANONYMOUS_GTID_LOG_EVENT as GTID_LOG_EVENTS to make a non-GTID
#     enabled event stream similar to a GTID enabled one, as the server now
#     generates [ANONYMOUS_]GTID always.
#
#   $keep_ddl_xid
#     When specified leaves the XID field printed out. The flag is not supposed
#     to be specified in "old" tests that do not expect DDL Query_log_event:s
#     with XID info.
#
#   $mask_user_password_events
#     To mask random password generated with create/alter user which used
#     caching_sha2 authentication plugin. Note this is the default
#     authentication plugin since 8.0.4
#
#   $mask_grant_as_events
#      Do not print the AS <...> WITH <...> clause in the GRANT statement
#      to avoid the test failure on the builds where partial_revokes is
#      ON by default.
#
--let $include_filename= show_binlog_events.inc
--source include/begin_include_file.inc

--let $is_relay_log= 0
--source include/show_events.inc

--let $include_filename= show_binlog_events.inc
--source include/end_include_file.inc
