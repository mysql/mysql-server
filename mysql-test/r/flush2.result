select @@GLOBAL.relay_log_info_repository into @save_relay_log_info_repository;
select @@GLOBAL.expire_logs_days into @save_expire_logs_days;
Warnings:
Warning	1287	'@@expire_logs_days' is deprecated and will be removed in a future release. Please use binlog_expire_logs_seconds instead.
set @@GLOBAL.relay_log_info_repository = 'FILE';
Warnings:
Warning	1287	'FILE' is deprecated and will be removed in a future release. Please use 'TABLE' instead
flush logs;
set global expire_logs_days = 3;
Warnings:
Warning	1287	'@@expire_logs_days' is deprecated and will be removed in a future release. Please use binlog_expire_logs_seconds instead.
show variables like 'log_bin%';
Variable_name	Value
log_bin	OFF
log_bin_basename	
log_bin_index	
log_bin_trust_function_creators	ON
log_bin_use_v1_row_events	OFF
show variables like 'relay_log%';
Variable_name	Value
relay_log	hostname-relay-bin
relay_log_basename	MYSQLTEST_VARDIR/mysqld.1/data/hostname-relay-bin
relay_log_index	MYSQLTEST_VARDIR/mysqld.1/data/hostname-relay-bin.index
relay_log_info_file	relay-log.info
relay_log_info_repository	FILE
relay_log_purge	ON
relay_log_recovery	OFF
relay_log_space_limit	0
flush logs;
show variables like 'log_bin%';
Variable_name	Value
log_bin	OFF
log_bin_basename	
log_bin_index	
log_bin_trust_function_creators	ON
log_bin_use_v1_row_events	OFF
show variables like 'relay_log%';
Variable_name	Value
relay_log	hostname-relay-bin
relay_log_basename	MYSQLTEST_VARDIR/mysqld.1/data/hostname-relay-bin
relay_log_index	MYSQLTEST_VARDIR/mysqld.1/data/hostname-relay-bin.index
relay_log_info_file	relay-log.info
relay_log_info_repository	FILE
relay_log_purge	ON
relay_log_recovery	OFF
relay_log_space_limit	0
set @@GLOBAL.relay_log_info_repository = @save_relay_log_info_repository;
set @@GLOBAL.expire_logs_days = @save_expire_logs_days;
Warnings:
Warning	1287	'@@expire_logs_days' is deprecated and will be removed in a future release. Please use binlog_expire_logs_seconds instead.
