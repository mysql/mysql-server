# This file contains test cases to validate the behavior of system
# variables 'default_table_encryption' with various permissions a user
# can have.
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
# VARIABLE USED BY NORMAL USER.
#
#
# SET default_table_encryption
#
SET SESSION default_table_encryption=true;
ERROR 42000: Access denied; you need (at least one of) the SUPER or SYSTEM_VARIABLES_ADMIN and TABLE_ENCRYPTION_ADMIN privilege(s) for this operation
SET SESSION default_table_encryption=false;
SET SESSION default_table_encryption=DEFAULT;
SET GLOBAL default_table_encryption=true;
ERROR 42000: Access denied; you need (at least one of) the SUPER or SYSTEM_VARIABLES_ADMIN privilege(s) for this operation
SET PERSIST default_table_encryption=true;
ERROR 42000: Access denied; you need (at least one of) the SUPER or SYSTEM_VARIABLES_ADMIN privilege(s) for this operation
SET PERSIST_ONLY default_table_encryption=true;
ERROR 42000: Access denied; you need SYSTEM_VARIABLES_ADMIN and PERSIST_RO_VARIABLES_ADMIN privileges for this operation
#
# VARIABLE USED BY USER WITH TABLE_ENCRYPTION_ADMIN privilege.
#
GRANT TABLE_ENCRYPTION_ADMIN ON *.* TO u1@localhost;
SELECT CURRENT_USER();
CURRENT_USER()
u1@localhost
SHOW GRANTS FOR CURRENT_USER();
Grants for u1@localhost
GRANT USAGE ON *.* TO `u1`@`localhost`
GRANT TABLE_ENCRYPTION_ADMIN ON *.* TO `u1`@`localhost`
GRANT ALL PRIVILEGES ON `db1`.* TO `u1`@`localhost`
#
# SET default_table_encryption
#
SET SESSION default_table_encryption=true;
ERROR 42000: Access denied; you need (at least one of) the SUPER or SYSTEM_VARIABLES_ADMIN and TABLE_ENCRYPTION_ADMIN privilege(s) for this operation
SET SESSION default_table_encryption=false;
SET SESSION default_table_encryption=DEFAULT;
SET GLOBAL default_table_encryption=true;
ERROR 42000: Access denied; you need (at least one of) the SUPER or SYSTEM_VARIABLES_ADMIN privilege(s) for this operation
SET PERSIST default_table_encryption=true;
ERROR 42000: Access denied; you need (at least one of) the SUPER or SYSTEM_VARIABLES_ADMIN privilege(s) for this operation
SET PERSIST_ONLY default_table_encryption=true;
ERROR 42000: Access denied; you need SYSTEM_VARIABLES_ADMIN and PERSIST_RO_VARIABLES_ADMIN privileges for this operation
#
# VARIABLE USED BY USER WITH SYSTEM_VARIABLES_ADMIN privilege.
#
REVOKE TABLE_ENCRYPTION_ADMIN ON *.* FROM u1@localhost;
GRANT SYSTEM_VARIABLES_ADMIN ON *.* TO u1@localhost;
SELECT CURRENT_USER();
CURRENT_USER()
u1@localhost
SHOW GRANTS FOR CURRENT_USER();
Grants for u1@localhost
GRANT USAGE ON *.* TO `u1`@`localhost`
GRANT SYSTEM_VARIABLES_ADMIN ON *.* TO `u1`@`localhost`
GRANT ALL PRIVILEGES ON `db1`.* TO `u1`@`localhost`
#
# SET default_table_encryption
#
SET SESSION default_table_encryption=true;
ERROR 42000: Access denied; you need (at least one of) the SUPER or SYSTEM_VARIABLES_ADMIN and TABLE_ENCRYPTION_ADMIN privilege(s) for this operation
SET SESSION default_table_encryption=false;
SET SESSION default_table_encryption=DEFAULT;
SET GLOBAL default_table_encryption=true;
ERROR 42000: Access denied; you need (at least one of) the SUPER or SYSTEM_VARIABLES_ADMIN and TABLE_ENCRYPTION_ADMIN privilege(s) for this operation
SET PERSIST default_table_encryption=true;
ERROR 42000: Access denied; you need (at least one of) the SUPER or SYSTEM_VARIABLES_ADMIN and TABLE_ENCRYPTION_ADMIN privilege(s) for this operation
SET PERSIST_ONLY default_table_encryption=true;
ERROR 42000: Access denied; you need SYSTEM_VARIABLES_ADMIN and PERSIST_RO_VARIABLES_ADMIN privileges for this operation
#
# VARIABLE USED BY USER WITH BOTH TABLE_ENCRYPTION_ADMIN and 
# SYSTEM_VARIABLES_ADMIN privilege.
#
GRANT TABLE_ENCRYPTION_ADMIN ON *.* TO u1@localhost;
SELECT CURRENT_USER();
CURRENT_USER()
u1@localhost
SHOW GRANTS FOR CURRENT_USER();
Grants for u1@localhost
GRANT USAGE ON *.* TO `u1`@`localhost`
GRANT SYSTEM_VARIABLES_ADMIN,TABLE_ENCRYPTION_ADMIN ON *.* TO `u1`@`localhost`
GRANT ALL PRIVILEGES ON `db1`.* TO `u1`@`localhost`
#
# SET default_table_encryption
#
SET SESSION default_table_encryption=true;
SELECT @@session.default_table_encryption = true;
@@session.default_table_encryption = true
1
SET SESSION default_table_encryption=false;
SELECT @@session.default_table_encryption = false;
@@session.default_table_encryption = false
1
SET SESSION default_table_encryption=DEFAULT;
SELECT @@session.default_table_encryption = false;
@@session.default_table_encryption = false
1
SET GLOBAL default_table_encryption=false;
SELECT @@global.default_table_encryption = false;
@@global.default_table_encryption = false
1
SET PERSIST default_table_encryption=true;
SELECT @@global.default_table_encryption = true;
@@global.default_table_encryption = true
1
SET PERSIST_ONLY default_table_encryption=true;
ERROR 42000: Access denied; you need SYSTEM_VARIABLES_ADMIN and PERSIST_RO_VARIABLES_ADMIN privileges for this operation
#
# VARIABLE USED BY USER WITH PERSIST_RO_VARIABLES_ADMIN,
# TABLE_ENCRYPTION_ADMIN && SYSTEM_VARIABLES_ADMIN privilege.
#
GRANT PERSIST_RO_VARIABLES_ADMIN ON *.* TO u1@localhost;
SELECT CURRENT_USER();
CURRENT_USER()
u1@localhost
SHOW GRANTS FOR CURRENT_USER();
Grants for u1@localhost
GRANT USAGE ON *.* TO `u1`@`localhost`
GRANT PERSIST_RO_VARIABLES_ADMIN,SYSTEM_VARIABLES_ADMIN,TABLE_ENCRYPTION_ADMIN ON *.* TO `u1`@`localhost`
GRANT ALL PRIVILEGES ON `db1`.* TO `u1`@`localhost`
#
# SET default_table_encryption
#
SET SESSION default_table_encryption=true;
SELECT @@session.default_table_encryption = true;
@@session.default_table_encryption = true
1
SET GLOBAL default_table_encryption=false;
SELECT @@global.default_table_encryption = false;
@@global.default_table_encryption = false
1
SET PERSIST default_table_encryption=true;
SELECT @@global.default_table_encryption = true;
@@global.default_table_encryption = true
1
SET PERSIST_ONLY default_table_encryption=false;
SELECT @@global.default_table_encryption = true;
@@global.default_table_encryption = true
1
#
# Test changing the value to invalid value
#
SET GLOBAL default_table_encryption = -1;
ERROR 42000: Variable 'default_table_encryption' can't be set to the value of '-1'
SELECT @@global.default_table_encryption;
@@global.default_table_encryption
1
SET GLOBAL default_table_encryption = 100000000000;
ERROR 42000: Variable 'default_table_encryption' can't be set to the value of '100000000000'
SELECT @@global.default_table_encryption;
@@global.default_table_encryption
1
SET GLOBAL default_table_encryption = 10000.01;
ERROR 42000: Incorrect argument type to variable 'default_table_encryption'
SELECT @@global.default_table_encryption;
@@global.default_table_encryption
1
SET @@global.default_table_encryption = 'test';
ERROR 42000: Variable 'default_table_encryption' can't be set to the value of 'test'
SELECT @@global.default_table_encryption;
@@global.default_table_encryption
1
SET @@global.default_table_encryption = ' ';
ERROR 42000: Variable 'default_table_encryption' can't be set to the value of ' '
SELECT @@global.default_table_encryption;
@@global.default_table_encryption
1
#
# VARIABLE USED BY USER WITH SUPER privilege.
#
REVOKE TABLE_ENCRYPTION_ADMIN, SYSTEM_VARIABLES_ADMIN,
PERSIST_RO_VARIABLES_ADMIN ON *.* FROM u1@localhost;
GRANT SUPER ON *.* TO u1@localhost;
Warnings:
Warning	1287	The SUPER privilege identifier is deprecated
SELECT CURRENT_USER();
CURRENT_USER()
u1@localhost
SHOW GRANTS FOR CURRENT_USER();
Grants for u1@localhost
GRANT SUPER ON *.* TO `u1`@`localhost`
GRANT ALL PRIVILEGES ON `db1`.* TO `u1`@`localhost`
#
# SET default_table_encryption
#
SET SESSION default_table_encryption=true;
SELECT @@session.default_table_encryption = true;
@@session.default_table_encryption = true
1
SET GLOBAL default_table_encryption=false;
SELECT @@global.default_table_encryption = false;
@@global.default_table_encryption = false
1
SET PERSIST default_table_encryption=true;
SELECT @@global.default_table_encryption = true;
@@global.default_table_encryption = true
1
SET PERSIST_ONLY default_table_encryption=true;
ERROR 42000: Access denied; you need SYSTEM_VARIABLES_ADMIN and PERSIST_RO_VARIABLES_ADMIN privileges for this operation
REVOKE SUPER ON *.* FROM u1@localhost;
Warnings:
Warning	1287	The SUPER privilege identifier is deprecated
DROP USER u1@localhost;
RESET PERSIST;
SET GLOBAL default_table_encryption=default;
