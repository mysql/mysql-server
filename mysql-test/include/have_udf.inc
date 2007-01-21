#
# Check if server has support for loading udf's
# i.e it will support dlopen
#
--require r/have_dynamic_loading.require
disable_query_log;
show variables like "have_dynamic_loading";
enable_query_log;

#
# Check if the variable UDF_EXAMPLE_LIB is set
#
--require r/have_udf_example.require
disable_query_log;
eval select LENGTH("$UDF_EXAMPLE_LIB") > 0 as "have_udf_example_lib";
enable_query_log;
