# Check that server is compiled and started with support for NDB
disable_query_log;
--require r/true.require
select support = 'Enabled' as `TRUE` from information_schema.engines where engine = 'ndbcluster';
enable_query_log;




