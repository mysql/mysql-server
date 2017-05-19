disable_query_log;

#
# Check if the variable TEST_UDF_REGISTRATION is set
#
if (!$TEST_UDF_REGISTRATION) {
   --skip component requires the environment variable \$TEST_UDF_REGISTRATION to be set (normally done by mtr), see the file plugin.defs
}

#
## Check if --plugin-dir was setup for component_test
#
if (`SELECT CONCAT('--plugin-dir=', REPLACE(@@plugin_dir, '\\\\', '/')) != '$TEST_UDF_REGISTRATION_OPT/'`) {
    --skip component_test requires that --plugin-dir is set to the component_test dir (the .opt file does not contain \$TEST_UDF_REGISTRATION_OPT)
}

enable_query_log;
