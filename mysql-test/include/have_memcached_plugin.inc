disable_query_log;
--require r/true.require
select (PLUGIN_LIBRARY LIKE 'libmemcached%') as `TRUE` FROM INFORMATION_SCHEMA.PLUGINS 
  WHERE PLUGIN_NAME='daemon_memcached';
enable_query_log;
