#
# Check if server has support for loading plugins
#
if (`SELECT @@have_dynamic_loading != 'YES'`) {
  --skip UDF requires dynamic loading
}

#
# Check if the variable UDF_EXAMPLE_SO is set
#
--require r/have_udf_example.require
disable_query_log;
eval select LENGTH('$UDF_EXAMPLE_SO') > 0 as 'have_udf_example_lib';
enable_query_log;
