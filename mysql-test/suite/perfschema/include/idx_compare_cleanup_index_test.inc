--echo
--echo ===== CLEAN UP =====

--enable_query_log
DROP PROCEDURE index_test;
--disable_warnings
DROP TABLE IF EXISTS test.indexes_off;
DROP TABLE IF EXISTS test.indexes_on;
--enable_warnings
