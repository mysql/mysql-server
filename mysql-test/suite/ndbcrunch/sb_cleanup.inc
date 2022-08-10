--disable_query_log
# Cleanup by dropping entire database
DROP DATABASE sbtest;

DROP USER sbuser;
--enable_query_log

echo Test completed;
