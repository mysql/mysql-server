# Reset the sys.sys_config table

# Note, we have to disable the triggers to reset the set_by column back to NULL
# as the sys.sys_config_update_set_user trigger will overwrite it to the current user
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
