--echo
--echo ===== CLEAN UP =====

--echo ## Default connection
--connection con2
DEALLOCATE PREPARE st1;
DEALLOCATE PREPARE st2;
DEALLOCATE PREPARE st3;
DEALLOCATE PREPARE st4;
DROP PROCEDURE SampleProc1;
--connection default
--disable_query_log
SET @pfs_columns = NULL;
--enable_query_log
DROP PROCEDURE index_test;
--disable_warnings
DROP TABLE IF EXISTS test.indexes_off;
DROP TABLE IF EXISTS test.indexes_on;
--enable_warnings
DROP TABLE test.t1;
DROP TABLE test.t2;
DROP TABLE test.t3;
REVOKE ALL PRIVILEGES, GRANT OPTION FROM user1@localhost;
REVOKE ALL PRIVILEGES, GRANT OPTION FROM user2@localhost;
REVOKE ALL PRIVILEGES, GRANT OPTION FROM user3@localhost;
DROP USER user1@localhost;
DROP USER user2@localhost;
DROP USER user3@localhost;
--disconnect con1
--disconnect con2
--disconnect con3

UPDATE performance_schema.threads SET INSTRUMENTED = 'YES';
