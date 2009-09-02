disable_query_log;
--require r/true.require
select (support = 'YES' or support = 'DEFAULT' or support = 'ENABLED') as `TRUE` from information_schema.engines where engine = 'innodb';
enable_query_log;
