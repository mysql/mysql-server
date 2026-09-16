--disable_query_log
let $saved_log_lsn_current_status=`
  SELECT STATUS
  FROM INFORMATION_SCHEMA.INNODB_METRICS
  WHERE NAME="log_lsn_current"
`;
if ($saved_log_lsn_current_status == "disabled") {
  SET GLOBAL innodb_monitor_enable = log_lsn_current;
}

let $saved_log_lsn_last_checkpoint_status=`
  SELECT STATUS
  FROM INFORMATION_SCHEMA.INNODB_METRICS
  WHERE NAME="log_lsn_last_checkpoint"
`;
if ($saved_log_lsn_last_checkpoint_status == "disabled") {
  SET GLOBAL innodb_monitor_enable = log_lsn_last_checkpoint;
}
--enable_query_log


let $wait_condition =
  SELECT 0<MIN(`COUNT`) AND MAX(`COUNT`) = MIN(`COUNT`)
  FROM INFORMATION_SCHEMA.INNODB_METRICS
  WHERE NAME IN ('log_lsn_last_checkpoint', 'log_lsn_current');
--source include/wait_condition.inc

--disable_query_log
if ($saved_log_lsn_current_status == "disabled") {
  SET GLOBAL innodb_monitor_disable = log_lsn_current;
}
if ($saved_log_lsn_last_checkpoint_status == "disabled") {
  SET GLOBAL innodb_monitor_disable = log_lsn_last_checkpoint;
}
--enable_query_log
