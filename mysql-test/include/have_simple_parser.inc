#
# Check if server has support for loading plugins
#
if (`SELECT @@have_dynamic_loading != 'YES'`) {
  --skip simple parser requires dynamic loading
}

#
# Check if the variable MYPLUGLIB_SO is set
#
--require r/have_simple_parser.require
disable_query_log;
eval select LENGTH('$MYPLUGLIB_SO') > 0 as 'have_simple_parser';
enable_query_log;
