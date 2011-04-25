#
# Check if server has support for loading plugins
#
if (`SELECT @@have_dynamic_loading != 'YES'`) {
  --skip UDF requires dynamic loading
}

#
# Check if the variable UDF_EXAMPLE_SO is set
#
#
if (`SELECT LENGTH('$UDF_EXAMPLE_SO') = 0`) {
  --skip Need udf example
}
