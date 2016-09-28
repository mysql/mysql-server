# ==== Purpose ====
#
# Execute SHOW RELAYLOG EVENTS and mask non-deterministic output.
#
# ==== Usage ====
#
# See include/show_binlog_events.inc
#
# One additional parameter is supported:
#
#   $rpl_channel_name
#     If set, the clause FOR CHANNEL $rpl_channel_name is added.

--let $include_filename= show_relaylog_events.inc
if ($rpl_channel_name)
{
  --let $include_filename= $include_filename [FOR CHANNEL $rpl_channel_name]
}
--source include/begin_include_file.inc

--let $is_relay_log= 1
--source include/show_events.inc

--let $include_filename= show_relaylog_events.inc
--source include/end_include_file.inc
