#
# Bug#31013538 ALL PRIVILEGES ON A DATABASE FOR
#              ROLE DOESN'T ALLOW USER TO CREATE
#
CREATE USER u1;
CREATE ROLE r1;
GRANT CREATE, DROP ON db_name.* TO r1;
GRANT r1 TO u1;
# Grants without role activation
SHOW GRANTS;
Grants for u1@%
GRANT USAGE ON *.* TO `u1`@`%`
GRANT `r1`@`%` TO `u1`@`%`
SET ROLE r1;
# Grants after role activation: Must show privileges from r1
SHOW GRANTS;
Grants for u1@%
GRANT USAGE ON *.* TO `u1`@`%`
GRANT CREATE, DROP ON `db_name`.* TO `u1`@`%`
GRANT `r1`@`%` TO `u1`@`%`
CREATE DATABASE db_name;
DROP DATABASE db_name;
CREATE DATABASE db1name;
DROP DATABASE db1name;
DROP ROLE r1;
DROP USER u1;

# End of 8.0 tests

