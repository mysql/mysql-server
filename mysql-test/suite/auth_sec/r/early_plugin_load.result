# Setup : Restart server with --early_plugin_load option
# 1. Check that KEYRING_FILE plugin is installed properly
SELECT PLUGIN_NAME, PLUGIN_STATUS, LOAD_OPTION FROM INFORMATION_SCHEMA.PLUGINS
WHERE PLUGIN_NAME LIKE 'keyring_file';
PLUGIN_NAME	PLUGIN_STATUS	LOAD_OPTION
keyring_file	ACTIVE	ON
SHOW VARIABLES LIKE 'keyring_file';
Variable_name	Value
# 3. Try uninstalling keyring_file plugin
UNINSTALL PLUGIN keyring_file;
