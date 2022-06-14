#
# Bug #28699403: SHOW GRANTS THROW ERROR FOR EXISTING SESSION WHEN
#  MANDATORY_ROLES CHANGED
CREATE USER u1@localhost;
CREATE ROLE r1;
CREATE TABLE t1 (a INT);
GRANT ALL ON t1 TO r1;
SET GLOBAL mandatory_roles="r1";
SET GLOBAL activate_all_roles_on_login = 'ON';
# in connection con1
SHOW GRANTS;
Grants for u1@localhost
GRANT USAGE ON *.* TO `u1`@`localhost`
GRANT ALL PRIVILEGES ON `test`.`t1` TO `u1`@`localhost`
GRANT `r1`@`%` TO `u1`@`localhost`
SELECT a FROM t1;
a
# in connection default
SET GLOBAL mandatory_roles="";
# in connection con1
# test: Must not fail
SHOW GRANTS;
Grants for u1@localhost
GRANT USAGE ON *.* TO `u1`@`localhost`
SELECT * FROM t1;
ERROR 42000: SELECT command denied to user 'u1'@'localhost' for table 't1'
# in connection default
DROP USER u1@localhost;
DROP ROLE r1;
DROP TABLE t1;
SET GLOBAL mandatory_roles=DEFAULT;
SET GLOBAL activate_all_roles_on_login=DEFAULT;
# End of 8.0 tests
