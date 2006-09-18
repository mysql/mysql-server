# Check that server is compiled and started with support for NDB
-- require r/have_ndb.require
disable_query_log;
show variables like "have_ndbcluster";
enable_query_log;


