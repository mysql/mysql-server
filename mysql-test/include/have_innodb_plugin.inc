--source include/not_embedded.inc
disable_query_log;
--require r/true.require
SELECT (plugin_library LIKE 'ha_innodb_plugin%') AS `TRUE` FROM information_schema.plugins WHERE LOWER(plugin_name) = 'innodb' AND LOWER(plugin_status) = 'active';
enable_query_log;
