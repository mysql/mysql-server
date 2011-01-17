# Include file for rpl_show_relaylog_events.inc

--let $log_type= BINLOG
if ($is_relay_log) {
  --let $log_type= RELAYLOG
}
--let $args=
if ($binlog_file != '') {
  --let $args= IN <FILE>
}
if ($binlog_limit) {
  --let $args= $args LIMIT $binlog_limit
}
--echo ******** [$CURRENT_CONNECTION] SHOW $log_type EVENTS $args ********
--source include/show_events.inc


