disable_query_log;
--require r/not_true.require
select (PLUGIN_LIBRARY LIKE 'ha_innodb_plugin%') as `TRUE` from information_schema.plugins where PLUGIN_NAME='InnoDB';
enable_query_log;
