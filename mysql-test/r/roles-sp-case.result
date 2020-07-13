#
# Bug #30896461: FUNCTION PRIVILEGES INHERITED
#   AS PROCEDURE PRIVILEGES
#
CREATE USER b30896461_test1@localhost;
CREATE USER b30896461_test2@localhost;
CREATE SCHEMA `B30896461`;
CREATE FUNCTION `B30896461`.`testFn`() RETURNS INTEGER DETERMINISTIC RETURN 1;
GRANT b30896461_test1@localhost TO b30896461_test2@localhost;
GRANT EXECUTE ON FUNCTION `B30896461`.`testFn` TO b30896461_test1@localhost;
GRANT SELECT ON test.* TO b30896461_test1@localhost;
GRANT SELECT ON test.* TO b30896461_test2@localhost;
FLUSH PRIVILEGES;
SELECT `B30896461`.`testFn`();
`B30896461`.`testFn`()
1
SELECT `B30896461`.`testfn`();
`B30896461`.`testfn`()
1
# testing database casemode
SHOW GRANTS;
Grants for B30896461_test1@localhost
GRANT USAGE ON *.* TO `B30896461_test1`@`localhost`
GRANT SELECT ON `test`.* TO `B30896461_test1`@`localhost`
GRANT EXECUTE ON FUNCTION `B30896461`.`testFn` TO `B30896461_test1`@`localhost`
# Test: must work
SELECT `B30896461`.`testFn`();
`B30896461`.`testFn`()
1
# Test: must work
SELECT `B30896461`.`testfn`();
`B30896461`.`testfn`()
1
# testing database casemode
# Test: must show a function grant
SHOW GRANTS;
Grants for B30896461_test2@localhost
GRANT USAGE ON *.* TO `B30896461_test2`@`localhost`
GRANT SELECT ON `test`.* TO `B30896461_test2`@`localhost`
GRANT EXECUTE ON FUNCTION `B30896461`.`testFn` TO `B30896461_test2`@`localhost`
GRANT `B30896461_test1`@`localhost` TO `B30896461_test2`@`localhost`
DROP USER b30896461_test1@localhost, b30896461_test2@localhost;
DROP SCHEMA `B30896461`;
# End of 8.0 tests
