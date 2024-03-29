USE test;
-- This file contains queries that can be used to check table consistency after running the stress_tx_rr test suite.
-- Contains a mix of queries used throughout the actual test suite.
-- When server is running, run this script e.g. like this:
--  ./bin/mysql -v -u root -h 127.0.0.1 < mysql-test/suite/engines/rr_trx/check_consistency.sql

CHECK TABLE t1;
ANALYZE TABLE t1;

-- Sum of all integers that are part of the test data should be 0
SELECT SUM(`int1` + `int1_key` + `int1_unique`
         + `int2` + `int2_key` + `int2_unique`)
         AS TotalSum
         FROM t1;

-- No uncommitted data should be visible to a REPEATABLE-READ transaction
SELECT * FROM t1 WHERE `is_uncommitted` = 1;

-- No rows marked as consistent should have row-sum not equal to 0
SELECT * FROM t1 WHERE @sum:=`int1` + `int1_key` + `int1_unique` + `int2` + `int2_key` + `int2_unique` <> 0 AND `is_consistent` = 1;

-- Check the table count. SHOULD NOT BE 0.
SELECT COUNT(*) FROM t1;

-- The count of rows with pk divisible by 5 should be constant.
-- (less useful when there is no concurrency, though)
SELECT COUNT(*) FROM t1 WHERE `pk` MOD 5 = 0 AND `pk` BETWEEN 1 AND 1000;

-- Check statistics (any number is OK, we are only looking for an impractical amount of errors
SELECT * FROM statistics;

