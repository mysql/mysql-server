disable_query_log;
#
# Check if server has support for loading plugins
#
if (`SELECT @@have_dynamic_loading != 'YES'`) {
  --skip daemon example plugin requires dynamic loading
}

#
# Check if the variable DAEMONEXAMPLE is set
#
if (!$DAEMONEXAMPLE) {
  --skip daemon_example plugin requires the environment variable \$DAEMONEXAMPLE to be set (normally done by mtr)
}

enable_query_log;
