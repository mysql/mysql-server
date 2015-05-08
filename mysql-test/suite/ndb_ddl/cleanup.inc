# No need to include the cleanup commands in .result file
--disable_query_log

# Drop the special databases and thus all tables within them
DROP DATABASE ndb_ddl_test;
DROP DATABASE ndb_ddl_test2;

--enable_query_log


