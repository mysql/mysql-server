disable_query_log;

#
# Check if the variable STATUS_VAR_SERVICE is set
#
if (!$STATUS_VAR_SERVICE) {
   --skip component requires the environment variable \$STATUS_VAR_SERVICE to be set (normally done by mtr), see the file plugin.defs
}

#
## Check if --plugin-dir was setup for component_test
#
if (`SELECT CONCAT('--plugin-dir=', REPLACE(@@plugin_dir, '\\\\', '/')) != '$STATUS_VAR_SERVICE_OPT/'`) {
    --skip component_test requires that --plugin-dir is set to the component_test dir (the .opt file does not contain \$STATUS_VAR_SERVICE_OPT)
}

enable_query_log;
