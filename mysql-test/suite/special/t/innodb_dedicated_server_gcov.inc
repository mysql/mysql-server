let $restart_parameters = restart: --innodb_dedicated_server=ON --innodb_buffer_pool_size=512M --innodb_debug_sys_mem_size=$debug_sys_mem_size"G"; let $explicit_default_wait_counter=100000;
--source include/restart_mysqld.inc

--let $actual_innodb_redo_log_capacity = `select ROUND(@@global.innodb_redo_log_capacity/1024/1024,0)`
--echo actual_innodb_redo_log_capacity  = $actual_innodb_redo_log_capacity
