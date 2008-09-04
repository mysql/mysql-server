#
# Check if server has support for loading udf's
# i.e it will support dlopen
#
--require r/have_dynamic_loading.require
disable_query_log;
show variables like 'have_dynamic_loading';
enable_query_log;

#
# Check if the variable EXAMPLE_PLUGIN is set
#
--require r/have_example_plugin.require
disable_query_log;
eval select LENGTH('$EXAMPLE_PLUGIN') > 0 as 'have_example_plugin';
enable_query_log;
