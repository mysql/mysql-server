-- require r/not_ndb.require
disable_query_log;
# so that both DISABLED and NO is output as NO
-- replace_result DISABLED NO
show variables like "have_ndbcluster";
enable_query_log;

