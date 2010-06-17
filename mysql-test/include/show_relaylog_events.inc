##############################################################################
# Show relay log events 
#
# Useage: 
# let $binlog_file= slave-relay-bin.000002; 
# let $binlog_start= 106; 
# let $binlog_limit= 1, 3; 
# source include/show_relaylog_events.inc;
#
# It shows the first relay log file if $binlog_file is not given. 
#
# It shows events from the end position of the description event if
# $binlog_start is not given.
#
# It shows all of the events if $binlog_limit is not given. 
# $binlog_format has the same semantic with 'LIMIT' option.
#
##############################################################################

--let $is_relay_log= 1
--source include/show_events.inc
