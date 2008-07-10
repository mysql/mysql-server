# suite/funcs_1/datadict/is_table_query.inc
#
# Check that every INFORMATION_SCHEMA table can be queried with a SELECT
# statement, just as if it were an ordinary user-defined table.
# (Requirement 3.2.1.1)
#
# The variable $is_table must be set before sourcing this script.
#
# Author:
# 2008-01-23 mleich WL#4203 Reorganize and fix the data dictionary tests of
#                           testsuite funcs_1
#                   Create this script based on older scripts and new code.
#
--disable_warnings
DROP VIEW      IF EXISTS test.v1;
DROP PROCEDURE IF EXISTS test.p1;
DROP FUNCTION  IF EXISTS test.f1;
--enable_warnings
eval CREATE VIEW test.v1 AS     SELECT * FROM information_schema.$is_table;
eval CREATE PROCEDURE test.p1() SELECT * FROM information_schema.$is_table;
delimiter //;
eval CREATE FUNCTION test.f1() returns BIGINT
BEGIN
   DECLARE counter BIGINT DEFAULT NULL;
   SELECT COUNT(*) INTO counter FROM information_schema.$is_table;
   RETURN counter;
END//
delimiter ;//


# We are not interested to check the content here.
--echo # Attention: The printing of the next result sets is disabled.
--disable_result_log
eval SELECT * FROM information_schema.$is_table;
SELECT * FROM test.v1;
CALL test.p1;
SELECT test.f1();
--enable_result_log

DROP VIEW test.v1;
DROP PROCEDURE test.p1;
DROP FUNCTION test.f1;
