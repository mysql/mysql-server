#
# Check if server has support for loading plugins
#
if (`SELECT @@have_dynamic_loading != 'YES'`) {
  --skip simple parser requires dynamic loading
}

#
# Check if the variable MYPLUGLIB_SO is set
#
if (`SELECT LENGTH('$MYPLUGLIB_SO') = 0`) {
  --skip simple parser is not built (no mypluglib.so)
}
