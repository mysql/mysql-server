--disable_query_log
--require r/true.require
select (PLUGIN_LIBRARY LIKE 'qa_auth_interface%') as `TRUE` FROM INFORMATION_SCHEMA.PLUGINS
  WHERE PLUGIN_NAME='qa_auth_interface';
--enable_query_log
