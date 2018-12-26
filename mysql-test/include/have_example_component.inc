disable_query_log;

#
# Check if the variable COMPONENT_1 is set
#
if (!$EXAMPLE_COMPONENT_1) {
   --skip component requires the environment variable \$COMPONENT_1 to be set (normally done by mtr), see the file plugin.defs
}

#
## Check if --plugin-dir was setup for component_test
#
if (`SELECT CONCAT('--plugin-dir=', REPLACE(@@plugin_dir, '\\\\', '/')) != '$EXAMPLE_COMPONENT_1_OPT/'`) {
    --skip component_test requires that --plugin-dir is set to the component_test dir (the .opt file does not contain \$COMPONENT_1_OPT)
}

enable_query_log;
