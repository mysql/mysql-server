# ==== Purpose ====
#
# Restart server
#
# ==== Usage ====
#
# --let PLUGIN_DIR_OPT = --plugin-dir=<path_to_plugin_dir>
# --source include/keyring_tests/start_server_with_manifest.inc
#
# ==== Parameters ====
#
# $RESTART_PARAMETERS
#    Server restart parameters
#

--echo # Re-starting mysql server with manifest file
let $restart_parameters = restart: $PLUGIN_DIR_OPT;
let $explicit_default_wait_counter=4000;
--source include/restart_mysqld_no_echo.inc
