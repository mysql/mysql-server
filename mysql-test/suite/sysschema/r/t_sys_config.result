DESC sys.sys_config;
Field	Type	Null	Key	Default	Extra
variable	varchar(128)	NO	PRI	NULL	
value	varchar(128)	YES		NULL	
set_time	timestamp	YES		CURRENT_TIMESTAMP	DEFAULT_GENERATED on update CURRENT_TIMESTAMP
set_by	varchar(128)	YES		NULL	
SELECT variable, value, set_by FROM sys.sys_config;
variable	value	set_by
diagnostics.allow_i_s_tables	OFF	NULL
diagnostics.include_raw	OFF	NULL
ps_thread_trx_info.max_length	65535	NULL
statement_performance_analyzer.limit	100	NULL
statement_performance_analyzer.view	NULL	NULL
statement_truncate_len	64	NULL
UPDATE sys.sys_config SET value = 128 WHERE variable = 'statement_truncate_len';
SELECT variable, value, set_by FROM sys.sys_config;
variable	value	set_by
diagnostics.allow_i_s_tables	OFF	NULL
diagnostics.include_raw	OFF	NULL
ps_thread_trx_info.max_length	65535	NULL
statement_performance_analyzer.limit	100	NULL
statement_performance_analyzer.view	NULL	NULL
statement_truncate_len	128	NULL
INSERT INTO sys.sys_config (variable, value) VALUES ('foo', 'bar');
SELECT variable, value, set_by FROM sys.sys_config;
variable	value	set_by
diagnostics.allow_i_s_tables	OFF	NULL
diagnostics.include_raw	OFF	NULL
foo	bar	NULL
ps_thread_trx_info.max_length	65535	NULL
statement_performance_analyzer.limit	100	NULL
statement_performance_analyzer.view	NULL	NULL
statement_truncate_len	128	NULL
SET @sys.ignore_sys_config_triggers := true;
DELETE FROM sys.sys_config;
INSERT IGNORE INTO sys.sys_config (variable, value) VALUES
('statement_truncate_len', 64),
('statement_performance_analyzer.limit', 100),
('statement_performance_analyzer.view', NULL),
('diagnostics.allow_i_s_tables', 'OFF'),
('diagnostics.include_raw', 'OFF'),
('ps_thread_trx_info.max_length', 65535);
SET @sys.ignore_sys_config_triggers := NULL;
