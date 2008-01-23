# Check that server is compiled and started with support for NDB
disable_query_log;
--require r/true.require
select (support = 'YES' or support = 'DEFAULT') as `TRUE` from information_schema.engines where engine = 'ndbcluster';
--source include/ndb_not_readonly.inc

# for performance in test suite set temporary
set global ndb_table_temporary=1;
set ndb_table_temporary=1;

enable_query_log;

#cleanup
disable_query_log;
disable_warnings;
--error 0,1051
drop table if exists t1,t2,t3,t4,t5,t6,t7,t8,t9,t10;
flush tables;
flush status;
enable_warnings;
enable_query_log;
