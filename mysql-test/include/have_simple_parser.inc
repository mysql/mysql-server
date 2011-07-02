#
# Check if server has support for loading plugins
#
if (`SELECT @@have_dynamic_loading != 'YES'`) {
  --skip simple parser requires dynamic loading
}

#
# Check if the variable SIMPLE_PARSER is set
#
if (!$MYPLUGLIB_SO) {
  --skip simple parser is not built (no mypluglib.so)
}

