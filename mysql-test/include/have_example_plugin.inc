#
# Check if server has support for loading udf's
# i.e it will support dlopen
#
--source include/have_dynamic_loading.inc

#
# Check if the variable HA_EXAMPLE_SO is set
#
--require r/have_example_plugin.require
disable_query_log;
eval select LENGTH('$HA_EXAMPLE_SO') > 0 as 'have_example_plugin';
enable_query_log;
