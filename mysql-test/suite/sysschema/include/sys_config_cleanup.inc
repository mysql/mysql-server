# Reset the sys.sys_config table

# Note, we can *not* reset the set_by column back to NULL
# as the sys.sys_config_update_set_user trigger will overwrite it to the current user
UPDATE sys.sys_config SET value = 64 WHERE variable = 'statement_truncate_len';

DELETE FROM sys.sys_config WHERE variable NOT IN ('statement_truncate_len');
