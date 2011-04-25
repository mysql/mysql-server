#
# Check if server has support for loading plugins
#
if (`SELECT @@have_dynamic_loading != 'YES'`) {
  --skip Example plugin requires dynamic loading
}

#
# Check if the variable HA_EXAMPLE_SO is set
#
if (`SELECT LENGTH('$HA_EXAMPLE_SO') = 0`) {
  --skip Need example plugin
}
