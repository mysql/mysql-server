disable_query_log;
--require r/true.require
select (PLUGIN_LIBRARY LIKE 'auth_test_plugin%') as `TRUE` FROM INFORMATION_SCHEMA.PLUGINS 
  WHERE PLUGIN_NAME='test_plugin_server';
enable_query_log;
