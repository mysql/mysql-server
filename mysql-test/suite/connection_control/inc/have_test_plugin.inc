disable_query_log;
#
# Check if server has support for loading plugin
#
if (`SELECT @@have_dynamic_loading != 'YES'`) {
  --skip The connection_control plugin requires dynamic loading
}

#
# Check if the variable PLUGIN_AUTH is set
#
if (!$PLUGIN_AUTH) {
  --skip The connection_control plugin requires the environment variable \$PLUGIN_AUTH to be set (normally done by mtr)
}

enable_query_log;
