--exec test x$NDB_STATUS_OK = x1
-- require r/have_ndb.require
disable_query_log;
show variables like "have_ndbcluster";
enable_query_log;

