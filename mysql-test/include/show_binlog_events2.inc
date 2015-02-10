# Call show_binlog_events.inc with parameters set in backwards
# compatible mode in order to get the output as much as possible
# same as before.
--let $show_binlog_events_mask_columns=2,5
--source include/show_binlog_events.inc

