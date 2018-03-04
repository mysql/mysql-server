disable_query_log;

#
# Check if the variable VALIDATE_PASSWORD_COMPONENT is set
#
if (!$VALIDATE_PASSWORD_COMPONENT) {
   --skip component requires the environment variable \$VALIDATE_PASSWORD_COMPONENT to be set (normally done by mtr), see the file plugin.defs
}

#
## Check if --plugin-dir was setup for component_test
#
if (`SELECT CONCAT('--plugin-dir=', REPLACE(@@plugin_dir, '\\\\', '/')) != '$VALIDATE_PASSWORD_COMPONENT_OPT/'`) {
    --skip component_test requires that --plugin-dir is set to the component_test dir (the .opt file does not contain \$TEST_VALIDATE_PASSWORD_COMPONENT_OPT)
}

enable_query_log;
