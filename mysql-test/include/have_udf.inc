#
# Check if server has support for loading plugins
#
if (`SELECT @@have_dynamic_loading != 'YES'`) {
  --skip UDF requires dynamic loading
}

#
# Check if the variable UDF_EXAMPLE_LIB is set
#
if (!$UDF_EXAMPLE_SO) {
  --skip Need udf example
}

