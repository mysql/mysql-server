# Check that server is compiled and started with support for NDB
#disable_query_log;
#--require r/true.require
#select (support = 'YES' or support = 'DEFAULT') as `TRUE` from information_schema.engines where engine = 'ndbcluster';
#--source include/ndb_not_readonly.inc
#enable_query_log;
# always make sure we have both mysql servers started ok before test starts
# there are some initial startup bugs that are avoided by doing this, avoiding sporadic
# failures in mysql-test-run
--source include/have_multi_ndb.inc
