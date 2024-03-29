# This file contains test cases to validate the behavior of system
# variable 'table_encryption_privilege_check' with various
# permissions a user --echo # can have.
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
# SET table_encryption_privilege_check
#
SET SESSION table_encryption_privilege_check=true;
ERROR HY000: Variable 'table_encryption_privilege_check' is a GLOBAL variable and should be set with SET GLOBAL
SET GLOBAL table_encryption_privilege_check=true;
ERROR 42000: Access denied; you need (at least one of) the SUPER or SYSTEM_VARIABLES_ADMIN privilege(s) for this operation
SET PERSIST table_encryption_privilege_check=true;
ERROR 42000: Access denied; you need (at least one of) the SUPER or SYSTEM_VARIABLES_ADMIN privilege(s) for this operation
SET PERSIST_ONLY table_encryption_privilege_check=true;
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
# SET table_encryption_privilege_check
#
SET GLOBAL table_encryption_privilege_check=true;
ERROR 42000: Access denied; you need (at least one of) the SUPER or SYSTEM_VARIABLES_ADMIN privilege(s) for this operation
SET PERSIST table_encryption_privilege_check=true;
ERROR 42000: Access denied; you need (at least one of) the SUPER or SYSTEM_VARIABLES_ADMIN privilege(s) for this operation
SET PERSIST_ONLY table_encryption_privilege_check=true;
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
# SET table_encryption_privilege_check
#
SET GLOBAL table_encryption_privilege_check=true;
ERROR 42000: Access denied; you need (at least one of) the SUPER privilege(s) for this operation
SET PERSIST table_encryption_privilege_check=true;
ERROR 42000: Access denied; you need (at least one of) the SUPER privilege(s) for this operation
SET PERSIST_ONLY table_encryption_privilege_check=true;
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
# SET table_encryption_privilege_check
#
SET GLOBAL table_encryption_privilege_check=true;
ERROR 42000: Access denied; you need (at least one of) the SUPER privilege(s) for this operation
SET PERSIST table_encryption_privilege_check=true;
ERROR 42000: Access denied; you need (at least one of) the SUPER privilege(s) for this operation
SET PERSIST_ONLY table_encryption_privilege_check=true;
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
# SET table_encryption_privilege_check
#
SET GLOBAL table_encryption_privilege_check=true;
ERROR 42000: Access denied; you need (at least one of) the SUPER privilege(s) for this operation
SET PERSIST table_encryption_privilege_check=true;
ERROR 42000: Access denied; you need (at least one of) the SUPER privilege(s) for this operation
SET PERSIST_ONLY table_encryption_privilege_check=true;
SELECT @@global.table_encryption_privilege_check = false;
@@global.table_encryption_privilege_check = false
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
# SET table_encryption_privilege_check
#
SET GLOBAL table_encryption_privilege_check=false;
SELECT @@global.table_encryption_privilege_check = false;
@@global.table_encryption_privilege_check = false
1
SET PERSIST table_encryption_privilege_check=true;
SELECT @@global.table_encryption_privilege_check = true;
@@global.table_encryption_privilege_check = true
1
SET PERSIST_ONLY table_encryption_privilege_check=true;
ERROR 42000: Access denied; you need SYSTEM_VARIABLES_ADMIN and PERSIST_RO_VARIABLES_ADMIN privileges for this operation
REVOKE SUPER ON *.* FROM u1@localhost;
Warnings:
Warning	1287	The SUPER privilege identifier is deprecated
DROP USER u1@localhost;
RESET PERSIST;
SET GLOBAL table_encryption_privilege_check=default;
