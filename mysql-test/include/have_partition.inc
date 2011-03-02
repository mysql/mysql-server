disable_query_log;
--require r/true.require
SELECT (plugin_status = 'ACTIVE') AS `TRUE` FROM information_schema.plugins WHERE plugin_name LIKE 'partition%';
enable_query_log;
