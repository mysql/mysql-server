#
# 0. Create a user and a connection with it
CREATE USER u1@localhost;
GRANT ALL ON db1.* TO u1@localhost;
SELECT CURRENT_USER();
CURRENT_USER()
u1@localhost
SHOW GRANTS FOR CURRENT_USER();
Grants for u1@localhost
GRANT USAGE ON *.* TO `u1`@`localhost`
GRANT ALL PRIVILEGES ON `db1`.* TO `u1`@`localhost`
#
# 1. Test the use by a unprivileged user
include/assert.inc [By default the value is set to OFF]
SET GLOBAL require_row_format=true;
ERROR HY000: Variable 'require_row_format' is a SESSION variable and can't be used with SET GLOBAL
SET PERSIST require_row_format=true;
ERROR HY000: Variable 'require_row_format' is a SESSION variable and can't be used with SET GLOBAL
SET PERSIST_ONLY require_row_format=true;
ERROR HY000: Variable 'require_row_format' is a SESSION variable and can't be used with SET GLOBAL
SET SESSION require_row_format=false;
SET SESSION require_row_format=true;
SET SESSION require_row_format=false;
ERROR 42000: Access denied; you need (at least one of) the SUPER or SYSTEM_VARIABLES_ADMIN or SESSION_VARIABLES_ADMIN privilege(s) for this operation
SET SESSION require_row_format=DEFAULT;
ERROR 42000: Access denied; you need (at least one of) the SUPER or SYSTEM_VARIABLES_ADMIN or SESSION_VARIABLES_ADMIN privilege(s) for this operation
#
# 2. Test the use by a user with SESSION_VARIABLES_ADMIN privilege.
GRANT SESSION_VARIABLES_ADMIN ON *.* TO u1@localhost;
SELECT CURRENT_USER();
CURRENT_USER()
u1@localhost
SHOW GRANTS FOR CURRENT_USER();
Grants for u1@localhost
GRANT USAGE ON *.* TO `u1`@`localhost`
GRANT SESSION_VARIABLES_ADMIN ON *.* TO `u1`@`localhost`
GRANT ALL PRIVILEGES ON `db1`.* TO `u1`@`localhost`
SET SESSION require_row_format=false;
SET SESSION require_row_format=true;
SET SESSION require_row_format=DEFAULT;
#
# 3. Test the use by a user with SYSTEM_VARIABLES_ADMIN privilege.
REVOKE SESSION_VARIABLES_ADMIN ON *.* FROM u1@localhost;
GRANT SYSTEM_VARIABLES_ADMIN ON *.* TO u1@localhost;
SELECT CURRENT_USER();
CURRENT_USER()
u1@localhost
SHOW GRANTS FOR CURRENT_USER();
Grants for u1@localhost
GRANT USAGE ON *.* TO `u1`@`localhost`
GRANT SYSTEM_VARIABLES_ADMIN ON *.* TO `u1`@`localhost`
GRANT ALL PRIVILEGES ON `db1`.* TO `u1`@`localhost`
SET SESSION require_row_format=true;
SET SESSION require_row_format=false;
SET SESSION require_row_format=true;
#
# 4. Test the use by a user with SUPER privilege.
REVOKE SESSION_VARIABLES_ADMIN ON *.* FROM u1@localhost;
GRANT SUPER ON *.* TO u1@localhost;
Warnings:
Warning	1287	The SUPER privilege identifier is deprecated
SELECT CURRENT_USER();
CURRENT_USER()
u1@localhost
SHOW GRANTS FOR CURRENT_USER();
Grants for u1@localhost
GRANT SUPER ON *.* TO `u1`@`localhost`
GRANT SYSTEM_VARIABLES_ADMIN ON *.* TO `u1`@`localhost`
GRANT ALL PRIVILEGES ON `db1`.* TO `u1`@`localhost`
SET SESSION require_row_format=true;
SELECT @@SESSION.require_row_format = true;
@@SESSION.require_row_format = true
1
SET SESSION require_row_format=false;
SELECT @@SESSION.require_row_format = false;
@@SESSION.require_row_format = false
1
SET SESSION require_row_format=true;
SELECT @@SESSION.require_row_format = true;
@@SESSION.require_row_format = true
1
#
# 5. Test errors when changing the variable to invalid values
SET SESSION require_row_format = -1;
ERROR 42000: Variable 'require_row_format' can't be set to the value of '-1'
SET SESSION require_row_format = 100000000000;
ERROR 42000: Variable 'require_row_format' can't be set to the value of '100000000000'
SET SESSION require_row_format = 10000.01;
ERROR 42000: Incorrect argument type to variable 'require_row_format'
SET SESSION require_row_format = 'test';
ERROR 42000: Variable 'require_row_format' can't be set to the value of 'test'
SET SESSION require_row_format = '';
ERROR 42000: Variable 'require_row_format' can't be set to the value of ''
#
# 6. Cleanup
DROP USER u1@localhost;
SET SESSION require_row_format=default;
