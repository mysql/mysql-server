#
# Similar to sync_slave_with_master but supports a configurable timeout
#  let $slave_sync_timeout =  300;
#  source include/wait_for_slave_to_sync_with_master;
#

connection slave;
stop slave;
start slave;

connection master;

let $file = query_get_value(SHOW MASTER STATUS, File, 1);
let $pos = query_get_value(SHOW MASTER STATUS, Position, 1);

connection slave;
--disable_result_log
--disable_query_log
eval select master_pos_wait("$file", $pos, $slave_sync_timeout);
--enable_result_log
--enable_query_log

connection master;
sync_slave_with_master;

connection master;

