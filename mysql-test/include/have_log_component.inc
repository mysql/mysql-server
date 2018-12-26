disable_query_log;

#
# Check if the variable LOGGING_COMPONENTS is set
#
if (!$LOGGING_COMPONENTS) {
   --skip component requires the environment variable \$LOGGING_COMPONENTS to be set (normally done by mtr), see the file plugin.defs
}

#
## Check if --plugin-dir was setup for component_test
#
if (`SELECT CONCAT('--plugin-dir=', REPLACE(@@plugin_dir, '\\\\', '/')) != '$LOGGING_COMPONENTS_OPT/'`) {
    --skip component_test requires that --plugin-dir is set to the component_test dir (the .opt file does not contain \$LOGGING_COMPONENTS_OPT)
}

enable_query_log;
