--echo #
--echo # Bug #30896461: FUNCTION PRIVILEGES INHERITED
--echo #   AS PROCEDURE PRIVILEGES
--echo #

--source include/count_sessions.inc

CREATE USER b30896461_test1@localhost;
CREATE USER b30896461_test2@localhost;

CREATE SCHEMA `B30896461`;

CREATE FUNCTION `B30896461`.`testFn`() RETURNS INTEGER DETERMINISTIC RETURN 1;

GRANT b30896461_test1@localhost TO b30896461_test2@localhost;
GRANT EXECUTE ON FUNCTION `B30896461`.`testFn` TO b30896461_test1@localhost;
GRANT SELECT ON test.* TO b30896461_test1@localhost;
GRANT SELECT ON test.* TO b30896461_test2@localhost;
FLUSH PRIVILEGES;

connect(con_b30896461_test1,localhost,b30896461_test1,,test);
SELECT `B30896461`.`testFn`();
SELECT `B30896461`.`testfn`();

disable_query_log;
disable_result_log;
let $lctn= query_get_value(SHOW VARIABLES LIKE 'lower_case_table_names', Value, 1);
if ($lctn != 0)
{
  # Must pass in casemode 0 and 2
  echo # testing database casemode;
  SELECT `b30896461`.`testfn`();
}
if ($lctn == 0)
{
  # Must fail in casemode 1
  echo # testing database casemode;
  --error ER_PROCACCESS_DENIED_ERROR
  SELECT `b30896461`.`testfn`();
}
enable_query_log;
enable_result_log;
--replace_result b30896461 B30896461 testfn testFn
SHOW GRANTS;

connect(con_b30896461_test2,localhost,b30896461_test2,,test);
--echo # Test: must work
SELECT `B30896461`.`testFn`();
--echo # Test: must work
SELECT `B30896461`.`testfn`();
disable_query_log;
disable_result_log;
let $lctn= query_get_value(SHOW VARIABLES LIKE 'lower_case_table_names', Value, 1);
if ($lctn != 0)
{
  # Must pass in casemode 0 and 2
  echo # testing database casemode;
  SELECT `b30896461`.`testfn`();
}
if ($lctn == 0)
{
  # Must fail in casemode 1
  echo # testing database casemode;
  --error ER_PROCACCESS_DENIED_ERROR
  SELECT `b30896461`.`testfn`();
}
enable_query_log;
enable_result_log;
--echo # Test: must show a function grant
--replace_result b30896461 B30896461 testfn testFn
SHOW GRANTS;

connection default;
disconnect con_b30896461_test1;
disconnect con_b30896461_test2;

DROP USER b30896461_test1@localhost, b30896461_test2@localhost;
DROP SCHEMA `B30896461`;

--source include/wait_until_count_sessions.inc

--echo # End of 8.0 tests
