
================================================================================
SETUP
================================================================================
CREATE TABLE test.t1 (c1 INT);
PREPARE stmt1 FROM "SELECT c1 FROM test.t1";
EXECUTE stmt1;
c1
SHOW SESSION STATUS LIKE "%com_stmt_%prepare%";
Variable_name	Value
Com_stmt_prepare	1
Com_stmt_reprepare	0
SELECT * FROM performance_schema.session_status WHERE VARIABLE_NAME LIKE "%com_stmt%";
VARIABLE_NAME	VARIABLE_VALUE
Com_stmt_reprepare	0
SELECT VARIABLE_VALUE FROM performance_schema.global_status
WHERE VARIABLE_NAME LIKE "Com_stmt_reprepare"
  INTO @count_global_reprepare_before;
ALTER TABLE test.t1 ADD COLUMN c2 INTEGER;
EXECUTE stmt1;
c1
SHOW SESSION STATUS LIKE "%com_stmt_%prepare%";
Variable_name	Value
Com_stmt_prepare	2
Com_stmt_reprepare	1
SELECT * FROM performance_schema.session_status WHERE VARIABLE_NAME LIKE "%com_stmt%";
VARIABLE_NAME	VARIABLE_VALUE
Com_stmt_reprepare	1
SELECT VARIABLE_VALUE FROM performance_schema.global_status
WHERE VARIABLE_NAME LIKE "Com_stmt_reprepare"
  INTO @count_global_reprepare_after;
SELECT @count_global_reprepare_after - @count_global_reprepare_before;
@count_global_reprepare_after - @count_global_reprepare_before
1

================================================================================
CLEANUP
================================================================================
DROP TABLE test.t1;
