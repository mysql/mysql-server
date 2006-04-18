#
# Check if server has support for loading udf's
# i.e it will support dlopen
#
--require r/have_udf.require
disable_query_log;
show variables like "have_dynamic_loading";
enable_query_log;
