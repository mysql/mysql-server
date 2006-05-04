# Check that server is compiled and started with support for NDB
-- require r/have_ndb.require
disable_query_log;
show variables like "have_ndbcluster";
enable_query_log;

# Check that NDB is installed and known to be working
-- require r/have_ndb_status_ok.require
disable_query_log;
eval select "$NDB_STATUS_OK" as ndb_status_ok;
enable_query_log;


