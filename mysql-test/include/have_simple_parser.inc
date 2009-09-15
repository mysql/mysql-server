#
# Check if server has support for loading udf's
# i.e it will support dlopen
#
--source include/have_dynamic_loading.inc

#
# Check if the variable SIMPLE_PARSER is set
#
--require r/have_simple_parser.require
disable_query_log;
eval select LENGTH('$SIMPLE_PARSER') > 0 as 'have_simple_parser';
enable_query_log;
