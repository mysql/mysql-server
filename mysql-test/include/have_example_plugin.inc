#
# Check if server has support for loading plugins
#
if (`SELECT @@have_dynamic_loading != 'YES'`) {
  --skip Example plugin requires dynamic loading
}

#
# Check if the variable HA_EXAMPLE_SO is set
#
--require r/have_example_plugin.require
disable_query_log;
eval select LENGTH('$HA_EXAMPLE_SO') > 0 as 'have_example_plugin';
enable_query_log;
