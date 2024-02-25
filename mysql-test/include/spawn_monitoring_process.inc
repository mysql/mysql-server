# ==== Purpose ====
#
# Launch a mysqld monitoring process if needed
#
# ==== Usage ====
#
# [--let $rpl_group_replication= 1]
# --source include/spawn_monitoring_process.inc
#
# Parameters:
#   $rpl_group_replication
#     Shall we preserve some the group replication settings
#
# Please see include/restart_with_mysqld_safe.inc
#

--let $include_filename= spawn_monitoring_process.inc
--source include/begin_include_file.inc

let $have_windows = `SELECT CONVERT(@@version_compile_os using latin1) IN ("Win32", "Win64", "Windows")`;

if (!$have_windows) {
  --disable_query_log
  SET SESSION sql_log_bin= 0;
  SET @auto_increment_increment_save= @@SESSION.auto_increment_increment;
  SET @auto_increment_offset_save= @@SESSION.auto_increment_offset;
  SET @@SESSION.auto_increment_increment= 1;
  SET @@SESSION.auto_increment_offset= 1;
  CREATE TABLE test.r_vars (id INT PRIMARY KEY AUTO_INCREMENT, var_name VARCHAR(64), var_value VARCHAR(256));
  INSERT INTO test.r_vars (var_name, var_value)
   SELECT * FROM performance_schema.global_variables
   WHERE VARIABLE_NAME LIKE 'replica_parallel_workers'
   OR VARIABLE_NAME LIKE 'replica_parallel_type'
   OR VARIABLE_NAME LIKE 'replica_preserve_commit_order'
   OR VARIABLE_NAME LIKE 'binlog_transaction_dependency_tracking'
   OR VARIABLE_NAME LIKE 'transaction_write_set_extraction'
   OR VARIABLE_NAME LIKE 'enforce_gtid_consistency'
   OR VARIABLE_NAME LIKE 'gtid_mode'
   ORDER BY VARIABLE_NAME;

  if(!$rpl_group_replication) {
    INSERT INTO test.r_vars (var_name, var_value)
     SELECT * FROM performance_schema.global_variables
     WHERE VARIABLE_NAME LIKE 'relay_log'
     OR VARIABLE_NAME LIKE 'rpl_semi_sync_replica_enabled'
     OR VARIABLE_NAME LIKE 'rpl_semi_sync_source_enabled'
     ORDER BY VARIABLE_NAME;
  }

  if($rpl_group_replication)
  {
    --let $gr_ssl_disabled= `SELECT @@group_replication_ssl_mode LIKE 'DISABLED'`
    if(!$gr_ssl_disabled) {
       INSERT INTO test.r_vars (var_name, var_value)
         SELECT * FROM performance_schema.global_variables
         WHERE VARIABLE_NAME LIKE 'ssl_ca'
         OR VARIABLE_NAME LIKE 'ssl_cert'
         OR VARIABLE_NAME LIKE 'ssl_key'
         ORDER BY VARIABLE_NAME;
    }

    INSERT INTO test.r_vars (var_name, var_value)
     SELECT * FROM performance_schema.global_variables
     WHERE VARIABLE_NAME LIKE 'group_replication_ssl_mode'
     OR VARIABLE_NAME LIKE 'group_replication_gtid_assignment_block_size'
     OR VARIABLE_NAME LIKE 'group_replication_ssl_mode'
     OR VARIABLE_NAME LIKE 'group_replication_recovery_ssl_ca'
     OR VARIABLE_NAME LIKE 'group_replication_recovery_ssl_cert'
     OR VARIABLE_NAME LIKE 'group_replication_recovery_ssl_key'
     ORDER BY VARIABLE_NAME;
  }
  --let $suite_mysqld_settings=
  --let $count_vars= `SELECT COUNT(*) FROM test.r_vars`
  --let $var_id= 1
  while ( $var_id <= $count_vars )
  {
    --let $var_name= `SELECT var_name FROM test.r_vars WHERE id=$var_id`
    --let $var_value= `SELECT var_value FROM test.r_vars WHERE id=$var_id`
    --let $suite_mysqld_settings= $suite_mysqld_settings --loose-$var_name=$var_value
    --inc $var_id
  }
  DROP TABLE test.r_vars;
  SET @@SESSION.auto_increment_increment= @auto_increment_increment_save;
  SET @@SESSION.auto_increment_offset= @auto_increment_offset_save;
  SET SESSION sql_log_bin= 1;
  --enable_query_log

  --let $mysqld_extra_settings= $mysqld_extra_settings $suite_mysqld_settings

  --source include/restart_with_mysqld_safe.inc
}

--let $include_filename= spawn_monitoring_process.inc
--source include/end_include_file.inc
