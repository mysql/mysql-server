--disable_query_log
--require r/true.require
select (support = 'YES' or support = 'DEFAULT') as `TRUE` from information_schema.engines where engine = 'archive';
--enable_query_log
