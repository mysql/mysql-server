--source include/master-slave.inc

connection slave;
# Check that server is compiled and started with support for NDB
disable_query_log;
--require r/true.require
select (support = 'YES' or support = 'DEFAULT') as `TRUE` from information_schema.engines where engine = 'ndbcluster';
--source include/ndb_not_readonly.inc
enable_query_log;

# Set the default connection to 'master'
connection master;
