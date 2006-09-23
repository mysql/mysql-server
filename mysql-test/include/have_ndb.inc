# Check that server is compiled and started with support for NDB
disable_query_log;
--require r/true.require
select support = 'Enabled' as `TRUE` from information_schema.engines where engine = 'ndbcluster';
enable_query_log;

# Check that NDB is installed and known to be working
# This will disable ndb from the shell script 'mysql-test-run'

-- require r/have_ndb_status_ok.require
disable_query_log;
eval select "$NDB_STATUS_OK" as ndb_status_ok;
enable_query_log;


