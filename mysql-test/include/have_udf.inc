#
# Check if server has support for loading plugins
#
if (`SELECT @@have_dynamic_loading != 'YES'`) {
  --skip UDF requires dynamic loading
}

#
# Check if the variable UDF_EXAMPLE_LIB is set
#
if (!$UDF_EXAMPLE_LIB) {
  --skip UDF requires the environment variable \$UDF_EXAMPLE_LIB to be set (normally done by mtr)
}

#
# Check if --plugin-dir was setup for udf
#
if (`SELECT CONCAT('--plugin-dir=', REPLACE(@@plugin_dir, '\\\\', '/')) != '$UDF_EXAMPLE_LIB_OPT/'`) {
  --skip UDF requires that --plugin-dir is set to the udf plugin dir (either the .opt file does not contain \$UDF_EXAMPLE_LIB_OPT or another plugin is in use)
}
