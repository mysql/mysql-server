let $restart_parameters = restart: --innodb_dedicated_server=ON --innodb_buffer_pool_size=512M --innodb_debug_sys_mem_size=$debug_sys_mem_size"G"; let $explicit_default_wait_counter=100000;
--source include/restart_mysqld.inc

let $actual_innodb_log_file_size = `select  ROUND(@@global.innodb_log_file_size/1024,0)`;
--echo actual_innodb_log_file_size  = $actual_innodb_log_file_size

let $actual_innodb_log_files_in_group = `select @@global.innodb_log_files_in_group`;
--echo actual_innodb_log_files_in_group = $actual_innodb_log_files_in_group
