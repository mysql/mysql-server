#
# Check if server has support for loading plugins
#
if (`SELECT @@have_dynamic_loading != 'YES'`) {
  --skip Example plugin requires dynamic loading
}

#
# Check if the variable EXAMPLE_PLUGIN is set
#
if (!$HA_EXAMPLE_SO) {
  --skip Need example plugin
}

