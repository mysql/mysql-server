#
# WL#12261 Control (enforce and disable) table encryption
#
# Pre-define user u1, which is used in different tests below.
CREATE USER u1@localhost;
GRANT ALL ON db1.* TO u1@localhost;
GRANT CREATE TABLESPACE, PROCESS, SYSTEM_VARIABLES_ADMIN ON *.* TO u1@localhost;
SET GLOBAL debug= '+d,skip_table_encryption_admin_check_for_set';
# This test run CREATE/ALTER DATABASE in different configurations,
#
# - Setting table_encryption_privilege_check to true/false.
# - Setting default_table_encryption to true/false.
# - With and without ENCRYPTION clause.
# - With and without user holding TABLE_ENCRYPTION_ADMIN privilege.
# - Test SHOW CREATE DATABASE
# - Test INFORMATION_SCHEMA.SCHEMATA
# - Check for warnings generated.
#
`````````````````````````````````````````````````````````
# CREATE DATABASE without DEFAULT ENCRYPTION clause
# and with different values for system variable
# 'table_encryption_privilege_check' and 'default_table_encryption'
# [CREATE DATABASE] Case 1 )
`````````````````````````````````````````````````````````
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=false;;
CREATE DATABASE db1;
SHOW WARNINGS;
Level	Code	Message
SHOW CREATE DATABASE db1;
Database	Create Database
db1	CREATE DATABASE `db1` /*!40100 DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci */ /*!80016 DEFAULT ENCRYPTION='N' */
SELECT SCHEMA_NAME, DEFAULT_ENCRYPTION
FROM INFORMATION_SCHEMA.SCHEMATA WHERE SCHEMA_NAME='db1';
SCHEMA_NAME	DEFAULT_ENCRYPTION
db1	NO
DROP DATABASE db1;
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=false;
# [CREATE DATABASE] Case 2 )
`````````````````````````````````````````````````````````
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=true;;
CREATE DATABASE db1;
SHOW WARNINGS;
Level	Code	Message
SHOW CREATE DATABASE db1;
Database	Create Database
db1	CREATE DATABASE `db1` /*!40100 DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci */ /*!80016 DEFAULT ENCRYPTION='Y' */
SELECT SCHEMA_NAME, DEFAULT_ENCRYPTION
FROM INFORMATION_SCHEMA.SCHEMATA WHERE SCHEMA_NAME='db1';
SCHEMA_NAME	DEFAULT_ENCRYPTION
db1	YES
DROP DATABASE db1;
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=false;
# [CREATE DATABASE] Case 3 )
`````````````````````````````````````````````````````````
SET GLOBAL table_encryption_privilege_check=true;
SET SESSION default_table_encryption=false;;
CREATE DATABASE db1;
SHOW WARNINGS;
Level	Code	Message
SHOW CREATE DATABASE db1;
Database	Create Database
db1	CREATE DATABASE `db1` /*!40100 DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci */ /*!80016 DEFAULT ENCRYPTION='N' */
SELECT SCHEMA_NAME, DEFAULT_ENCRYPTION
FROM INFORMATION_SCHEMA.SCHEMATA WHERE SCHEMA_NAME='db1';
SCHEMA_NAME	DEFAULT_ENCRYPTION
db1	NO
DROP DATABASE db1;
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=false;
# [CREATE DATABASE] Case 4 )
`````````````````````````````````````````````````````````
SET GLOBAL table_encryption_privilege_check=true;
SET SESSION default_table_encryption=true;;
CREATE DATABASE db1;
SHOW WARNINGS;
Level	Code	Message
SHOW CREATE DATABASE db1;
Database	Create Database
db1	CREATE DATABASE `db1` /*!40100 DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci */ /*!80016 DEFAULT ENCRYPTION='Y' */
SELECT SCHEMA_NAME, DEFAULT_ENCRYPTION
FROM INFORMATION_SCHEMA.SCHEMATA WHERE SCHEMA_NAME='db1';
SCHEMA_NAME	DEFAULT_ENCRYPTION
db1	YES
DROP DATABASE db1;
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=false;
# [CREATE DATABASE] Case 5 )
`````````````````````````````````````````````````````````
# Granting TABLE_ENCRYPTION_ADMIN for user.
GRANT TABLE_ENCRYPTION_ADMIN ON *.* TO u1@localhost;
SET GLOBAL table_encryption_privilege_check=true;
SET SESSION default_table_encryption=false;;
CREATE DATABASE db1;
SHOW WARNINGS;
Level	Code	Message
SHOW CREATE DATABASE db1;
Database	Create Database
db1	CREATE DATABASE `db1` /*!40100 DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci */ /*!80016 DEFAULT ENCRYPTION='N' */
SELECT SCHEMA_NAME, DEFAULT_ENCRYPTION
FROM INFORMATION_SCHEMA.SCHEMATA WHERE SCHEMA_NAME='db1';
SCHEMA_NAME	DEFAULT_ENCRYPTION
db1	NO
DROP DATABASE db1;
# Revoke TABLE_ENCRYPTION_ADMIN
REVOKE TABLE_ENCRYPTION_ADMIN ON *.* FROM u1@localhost;
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=false;
# [CREATE DATABASE] Case 6 )
`````````````````````````````````````````````````````````
# Granting TABLE_ENCRYPTION_ADMIN for user.
GRANT TABLE_ENCRYPTION_ADMIN ON *.* TO u1@localhost;
SET GLOBAL table_encryption_privilege_check=true;
SET SESSION default_table_encryption=true;;
CREATE DATABASE db1;
SHOW WARNINGS;
Level	Code	Message
SHOW CREATE DATABASE db1;
Database	Create Database
db1	CREATE DATABASE `db1` /*!40100 DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci */ /*!80016 DEFAULT ENCRYPTION='Y' */
SELECT SCHEMA_NAME, DEFAULT_ENCRYPTION
FROM INFORMATION_SCHEMA.SCHEMATA WHERE SCHEMA_NAME='db1';
SCHEMA_NAME	DEFAULT_ENCRYPTION
db1	YES
DROP DATABASE db1;
# Revoke TABLE_ENCRYPTION_ADMIN
REVOKE TABLE_ENCRYPTION_ADMIN ON *.* FROM u1@localhost;
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=false;
`````````````````````````````````````````````````````````
# CREATE DATABASE with DEFAULT ENCRYPTION clause 'y/n'
# and with different values for system variable
# 'table_encryption_privilege_check' and 'default_table_encryption'.
# [CREATE DATABASE] Case 7 )
`````````````````````````````````````````````````````````
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=false;;
CREATE DATABASE db1 DEFAULT ENCRYPTION='y';
SHOW WARNINGS;
Level	Code	Message
SHOW CREATE DATABASE db1;
Database	Create Database
db1	CREATE DATABASE `db1` /*!40100 DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci */ /*!80016 DEFAULT ENCRYPTION='Y' */
SELECT SCHEMA_NAME, DEFAULT_ENCRYPTION
FROM INFORMATION_SCHEMA.SCHEMATA WHERE SCHEMA_NAME='db1';
SCHEMA_NAME	DEFAULT_ENCRYPTION
db1	YES
DROP DATABASE db1;
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=false;
# [CREATE DATABASE] Case 8 )
`````````````````````````````````````````````````````````
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=false;;
CREATE DATABASE db1 DEFAULT ENCRYPTION='n';
SHOW WARNINGS;
Level	Code	Message
SHOW CREATE DATABASE db1;
Database	Create Database
db1	CREATE DATABASE `db1` /*!40100 DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci */ /*!80016 DEFAULT ENCRYPTION='N' */
SELECT SCHEMA_NAME, DEFAULT_ENCRYPTION
FROM INFORMATION_SCHEMA.SCHEMATA WHERE SCHEMA_NAME='db1';
SCHEMA_NAME	DEFAULT_ENCRYPTION
db1	NO
DROP DATABASE db1;
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=false;
# [CREATE DATABASE] Case 9 )
`````````````````````````````````````````````````````````
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=true;;
CREATE DATABASE db1 DEFAULT ENCRYPTION='y';
SHOW WARNINGS;
Level	Code	Message
SHOW CREATE DATABASE db1;
Database	Create Database
db1	CREATE DATABASE `db1` /*!40100 DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci */ /*!80016 DEFAULT ENCRYPTION='Y' */
SELECT SCHEMA_NAME, DEFAULT_ENCRYPTION
FROM INFORMATION_SCHEMA.SCHEMATA WHERE SCHEMA_NAME='db1';
SCHEMA_NAME	DEFAULT_ENCRYPTION
db1	YES
DROP DATABASE db1;
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=false;
# [CREATE DATABASE] Case 10 )
`````````````````````````````````````````````````````````
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=true;;
CREATE DATABASE db1 DEFAULT ENCRYPTION='n';
SHOW WARNINGS;
Level	Code	Message
SHOW CREATE DATABASE db1;
Database	Create Database
db1	CREATE DATABASE `db1` /*!40100 DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci */ /*!80016 DEFAULT ENCRYPTION='N' */
SELECT SCHEMA_NAME, DEFAULT_ENCRYPTION
FROM INFORMATION_SCHEMA.SCHEMATA WHERE SCHEMA_NAME='db1';
SCHEMA_NAME	DEFAULT_ENCRYPTION
db1	NO
DROP DATABASE db1;
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=false;
`````````````````````````````````````````````````````````
# Without the keyword DEFAULT.
# [CREATE DATABASE] Case 11 )
`````````````````````````````````````````````````````````
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=false;;
CREATE DATABASE db1 ENCRYPTION='y';
SHOW WARNINGS;
Level	Code	Message
SHOW CREATE DATABASE db1;
Database	Create Database
db1	CREATE DATABASE `db1` /*!40100 DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci */ /*!80016 DEFAULT ENCRYPTION='Y' */
SELECT SCHEMA_NAME, DEFAULT_ENCRYPTION
FROM INFORMATION_SCHEMA.SCHEMATA WHERE SCHEMA_NAME='db1';
SCHEMA_NAME	DEFAULT_ENCRYPTION
db1	YES
DROP DATABASE db1;
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=false;
# [CREATE DATABASE] Case 12 )
`````````````````````````````````````````````````````````
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=false;;
CREATE DATABASE db1 ENCRYPTION='n';
SHOW WARNINGS;
Level	Code	Message
SHOW CREATE DATABASE db1;
Database	Create Database
db1	CREATE DATABASE `db1` /*!40100 DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci */ /*!80016 DEFAULT ENCRYPTION='N' */
SELECT SCHEMA_NAME, DEFAULT_ENCRYPTION
FROM INFORMATION_SCHEMA.SCHEMATA WHERE SCHEMA_NAME='db1';
SCHEMA_NAME	DEFAULT_ENCRYPTION
db1	NO
DROP DATABASE db1;
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=false;
# [CREATE DATABASE] Case 13 )
`````````````````````````````````````````````````````````
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=true;;
CREATE DATABASE db1 ENCRYPTION='y';
SHOW WARNINGS;
Level	Code	Message
SHOW CREATE DATABASE db1;
Database	Create Database
db1	CREATE DATABASE `db1` /*!40100 DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci */ /*!80016 DEFAULT ENCRYPTION='Y' */
SELECT SCHEMA_NAME, DEFAULT_ENCRYPTION
FROM INFORMATION_SCHEMA.SCHEMATA WHERE SCHEMA_NAME='db1';
SCHEMA_NAME	DEFAULT_ENCRYPTION
db1	YES
DROP DATABASE db1;
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=false;
# [CREATE DATABASE] Case 14 )
`````````````````````````````````````````````````````````
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=true;;
CREATE DATABASE db1 ENCRYPTION='n';
SHOW WARNINGS;
Level	Code	Message
SHOW CREATE DATABASE db1;
Database	Create Database
db1	CREATE DATABASE `db1` /*!40100 DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci */ /*!80016 DEFAULT ENCRYPTION='N' */
SELECT SCHEMA_NAME, DEFAULT_ENCRYPTION
FROM INFORMATION_SCHEMA.SCHEMATA WHERE SCHEMA_NAME='db1';
SCHEMA_NAME	DEFAULT_ENCRYPTION
db1	NO
DROP DATABASE db1;
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=false;
`````````````````````````````````````````````````````````
# We expect failure because the encryption request is different from
# global 'default_table_encryption' setting.
# [CREATE DATABASE] Case 15 )
`````````````````````````````````````````````````````````
SET GLOBAL table_encryption_privilege_check=true;
SET SESSION default_table_encryption=false;;
CREATE DATABASE db1 DEFAULT ENCRYPTION='y';
ERROR HY000: Database default encryption differ from 'default_table_encryption' setting, and user doesn't have enough privilege.
SHOW WARNINGS;
Level	Code	Message
Error	3827	Database default encryption differ from 'default_table_encryption' setting, and user doesn't have enough privilege.
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=false;
# [CREATE DATABASE] Case 16 )
`````````````````````````````````````````````````````````
SET GLOBAL table_encryption_privilege_check=true;
SET SESSION default_table_encryption=false;;
CREATE DATABASE db1 DEFAULT ENCRYPTION='n';
SHOW WARNINGS;
Level	Code	Message
SHOW CREATE DATABASE db1;
Database	Create Database
db1	CREATE DATABASE `db1` /*!40100 DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci */ /*!80016 DEFAULT ENCRYPTION='N' */
SELECT SCHEMA_NAME, DEFAULT_ENCRYPTION
FROM INFORMATION_SCHEMA.SCHEMATA WHERE SCHEMA_NAME='db1';
SCHEMA_NAME	DEFAULT_ENCRYPTION
db1	NO
DROP DATABASE db1;
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=false;
# [CREATE DATABASE] Case 17 )
`````````````````````````````````````````````````````````
SET GLOBAL table_encryption_privilege_check=true;
SET SESSION default_table_encryption=true;;
CREATE DATABASE db1 DEFAULT ENCRYPTION='y';
SHOW WARNINGS;
Level	Code	Message
SHOW CREATE DATABASE db1;
Database	Create Database
db1	CREATE DATABASE `db1` /*!40100 DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci */ /*!80016 DEFAULT ENCRYPTION='Y' */
SELECT SCHEMA_NAME, DEFAULT_ENCRYPTION
FROM INFORMATION_SCHEMA.SCHEMATA WHERE SCHEMA_NAME='db1';
SCHEMA_NAME	DEFAULT_ENCRYPTION
db1	YES
DROP DATABASE db1;
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=false;
`````````````````````````````````````````````````````````
# We expect failure because the encryption request is different from
# global 'default_table_encryption' setting.
# [CREATE DATABASE] Case 18 )
`````````````````````````````````````````````````````````
SET GLOBAL table_encryption_privilege_check=true;
SET SESSION default_table_encryption=true;;
CREATE DATABASE db1 DEFAULT ENCRYPTION='n';
ERROR HY000: Database default encryption differ from 'default_table_encryption' setting, and user doesn't have enough privilege.
SHOW WARNINGS;
Level	Code	Message
Error	3827	Database default encryption differ from 'default_table_encryption' setting, and user doesn't have enough privilege.
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=false;
# [CREATE DATABASE] Case 19 )
`````````````````````````````````````````````````````````
# Granting TABLE_ENCRYPTION_ADMIN for user.
GRANT TABLE_ENCRYPTION_ADMIN ON *.* TO u1@localhost;
SET GLOBAL table_encryption_privilege_check=true;
SET SESSION default_table_encryption=false;;
CREATE DATABASE db1 DEFAULT ENCRYPTION='y';
SHOW WARNINGS;
Level	Code	Message
SHOW CREATE DATABASE db1;
Database	Create Database
db1	CREATE DATABASE `db1` /*!40100 DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci */ /*!80016 DEFAULT ENCRYPTION='Y' */
SELECT SCHEMA_NAME, DEFAULT_ENCRYPTION
FROM INFORMATION_SCHEMA.SCHEMATA WHERE SCHEMA_NAME='db1';
SCHEMA_NAME	DEFAULT_ENCRYPTION
db1	YES
DROP DATABASE db1;
# Revoke TABLE_ENCRYPTION_ADMIN
REVOKE TABLE_ENCRYPTION_ADMIN ON *.* FROM u1@localhost;
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=false;
# [CREATE DATABASE] Case 20 )
`````````````````````````````````````````````````````````
# Granting TABLE_ENCRYPTION_ADMIN for user.
GRANT TABLE_ENCRYPTION_ADMIN ON *.* TO u1@localhost;
SET GLOBAL table_encryption_privilege_check=true;
SET SESSION default_table_encryption=false;;
CREATE DATABASE db1 DEFAULT ENCRYPTION='n';
SHOW WARNINGS;
Level	Code	Message
SHOW CREATE DATABASE db1;
Database	Create Database
db1	CREATE DATABASE `db1` /*!40100 DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci */ /*!80016 DEFAULT ENCRYPTION='N' */
SELECT SCHEMA_NAME, DEFAULT_ENCRYPTION
FROM INFORMATION_SCHEMA.SCHEMATA WHERE SCHEMA_NAME='db1';
SCHEMA_NAME	DEFAULT_ENCRYPTION
db1	NO
DROP DATABASE db1;
# Revoke TABLE_ENCRYPTION_ADMIN
REVOKE TABLE_ENCRYPTION_ADMIN ON *.* FROM u1@localhost;
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=false;
# [CREATE DATABASE] Case 21 )
`````````````````````````````````````````````````````````
# Granting TABLE_ENCRYPTION_ADMIN for user.
GRANT TABLE_ENCRYPTION_ADMIN ON *.* TO u1@localhost;
SET GLOBAL table_encryption_privilege_check=true;
SET SESSION default_table_encryption=true;;
CREATE DATABASE db1 DEFAULT ENCRYPTION='y';
SHOW WARNINGS;
Level	Code	Message
SHOW CREATE DATABASE db1;
Database	Create Database
db1	CREATE DATABASE `db1` /*!40100 DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci */ /*!80016 DEFAULT ENCRYPTION='Y' */
SELECT SCHEMA_NAME, DEFAULT_ENCRYPTION
FROM INFORMATION_SCHEMA.SCHEMATA WHERE SCHEMA_NAME='db1';
SCHEMA_NAME	DEFAULT_ENCRYPTION
db1	YES
DROP DATABASE db1;
# Revoke TABLE_ENCRYPTION_ADMIN
REVOKE TABLE_ENCRYPTION_ADMIN ON *.* FROM u1@localhost;
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=false;
# [CREATE DATABASE] Case 22 )
`````````````````````````````````````````````````````````
# Granting TABLE_ENCRYPTION_ADMIN for user.
GRANT TABLE_ENCRYPTION_ADMIN ON *.* TO u1@localhost;
SET GLOBAL table_encryption_privilege_check=true;
SET SESSION default_table_encryption=true;;
CREATE DATABASE db1 DEFAULT ENCRYPTION='n';
SHOW WARNINGS;
Level	Code	Message
SHOW CREATE DATABASE db1;
Database	Create Database
db1	CREATE DATABASE `db1` /*!40100 DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci */ /*!80016 DEFAULT ENCRYPTION='N' */
SELECT SCHEMA_NAME, DEFAULT_ENCRYPTION
FROM INFORMATION_SCHEMA.SCHEMATA WHERE SCHEMA_NAME='db1';
SCHEMA_NAME	DEFAULT_ENCRYPTION
db1	NO
DROP DATABASE db1;
# Revoke TABLE_ENCRYPTION_ADMIN
REVOKE TABLE_ENCRYPTION_ADMIN ON *.* FROM u1@localhost;
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=false;
`````````````````````````````````````````````````````````
# With invalid value for DEFAULT ENCRYPTION
# [CREATE DATABASE] Case 23 )
`````````````````````````````````````````````````````````
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=false;;
CREATE DATABASE db1 DEFAULT ENCRYPTION='k';
ERROR HY000: Incorrect argument (should be Y or N) value: 'k'
SHOW WARNINGS;
Level	Code	Message
Error	1525	Incorrect argument (should be Y or N) value: 'k'
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=false;
`````````````````````````````````````````````````````````
# Check with legacy syntax.
CREATE DATABASE `db1` /*!80016 DEFAULT ENCRYPTION='Y' */;
SHOW CREATE DATABASE db1;
Database	Create Database
db1	CREATE DATABASE `db1` /*!40100 DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci */ /*!80016 DEFAULT ENCRYPTION='Y' */
DROP DATABASE db1;
`````````````````````````````````````````````````````````
# See that we ignore the clause with invalid mysql version.
CREATE DATABASE `db1` /*!99999 DEFAULT ENCRYPTION='Y' */;
SHOW CREATE DATABASE db1;
Database	Create Database
db1	CREATE DATABASE `db1` /*!40100 DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci */ /*!80016 DEFAULT ENCRYPTION='N' */
DROP DATABASE db1;
`````````````````````````````````````````````````````````
# ALTER DATABASE withDEFAULT ENCRYPTION clause 'y/n'
# and with different values for system variable
# 'table_encryption_privilege_check' and 'default_table_encryption'
`````````````````````````````````````````````````````````
# Following cases are with database DEFAULT ENCRYPTION 'y'
# [ALTER DATABASE] Case 1 )
`````````````````````````````````````````````````````````
CREATE DATABASE db1 DEFAULT ENCRYPTION='n';
SHOW CREATE DATABASE db1;
Database	Create Database
db1	CREATE DATABASE `db1` /*!40100 DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci */ /*!80016 DEFAULT ENCRYPTION='N' */
SET SESSION default_table_encryption=false;;
SET GLOBAL table_encryption_privilege_check=false;
ALTER DATABASE db1 DEFAULT ENCRYPTION='n';
SHOW WARNINGS;
Level	Code	Message
SHOW CREATE DATABASE db1;
Database	Create Database
db1	CREATE DATABASE `db1` /*!40100 DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci */ /*!80016 DEFAULT ENCRYPTION='N' */
SELECT SCHEMA_NAME, DEFAULT_ENCRYPTION
FROM INFORMATION_SCHEMA.SCHEMATA WHERE SCHEMA_NAME='db1';
SCHEMA_NAME	DEFAULT_ENCRYPTION
db1	NO
DROP DATABASE db1;
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=false;
# [ALTER DATABASE] Case 2 )
`````````````````````````````````````````````````````````
CREATE DATABASE db1 DEFAULT ENCRYPTION='n';
SHOW CREATE DATABASE db1;
Database	Create Database
db1	CREATE DATABASE `db1` /*!40100 DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci */ /*!80016 DEFAULT ENCRYPTION='N' */
SET SESSION default_table_encryption=false;;
SET GLOBAL table_encryption_privilege_check=false;
ALTER DATABASE db1 DEFAULT ENCRYPTION='y';
SHOW WARNINGS;
Level	Code	Message
SHOW CREATE DATABASE db1;
Database	Create Database
db1	CREATE DATABASE `db1` /*!40100 DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci */ /*!80016 DEFAULT ENCRYPTION='Y' */
SELECT SCHEMA_NAME, DEFAULT_ENCRYPTION
FROM INFORMATION_SCHEMA.SCHEMATA WHERE SCHEMA_NAME='db1';
SCHEMA_NAME	DEFAULT_ENCRYPTION
db1	YES
DROP DATABASE db1;
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=false;
# [ALTER DATABASE] Case 3 )
`````````````````````````````````````````````````````````
CREATE DATABASE db1 DEFAULT ENCRYPTION='n';
SHOW CREATE DATABASE db1;
Database	Create Database
db1	CREATE DATABASE `db1` /*!40100 DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci */ /*!80016 DEFAULT ENCRYPTION='N' */
SET SESSION default_table_encryption=true;;
SET GLOBAL table_encryption_privilege_check=false;
ALTER DATABASE db1 DEFAULT ENCRYPTION='n';
SHOW WARNINGS;
Level	Code	Message
SHOW CREATE DATABASE db1;
Database	Create Database
db1	CREATE DATABASE `db1` /*!40100 DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci */ /*!80016 DEFAULT ENCRYPTION='N' */
SELECT SCHEMA_NAME, DEFAULT_ENCRYPTION
FROM INFORMATION_SCHEMA.SCHEMATA WHERE SCHEMA_NAME='db1';
SCHEMA_NAME	DEFAULT_ENCRYPTION
db1	NO
DROP DATABASE db1;
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=false;
# [ALTER DATABASE] Case 4 )
`````````````````````````````````````````````````````````
CREATE DATABASE db1 DEFAULT ENCRYPTION='n';
SHOW CREATE DATABASE db1;
Database	Create Database
db1	CREATE DATABASE `db1` /*!40100 DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci */ /*!80016 DEFAULT ENCRYPTION='N' */
SET SESSION default_table_encryption=true;;
SET GLOBAL table_encryption_privilege_check=false;
ALTER DATABASE db1 DEFAULT ENCRYPTION='y';
SHOW WARNINGS;
Level	Code	Message
SHOW CREATE DATABASE db1;
Database	Create Database
db1	CREATE DATABASE `db1` /*!40100 DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci */ /*!80016 DEFAULT ENCRYPTION='Y' */
SELECT SCHEMA_NAME, DEFAULT_ENCRYPTION
FROM INFORMATION_SCHEMA.SCHEMATA WHERE SCHEMA_NAME='db1';
SCHEMA_NAME	DEFAULT_ENCRYPTION
db1	YES
DROP DATABASE db1;
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=false;
# [ALTER DATABASE] Case 5 )
`````````````````````````````````````````````````````````
CREATE DATABASE db1 DEFAULT ENCRYPTION='n';
SHOW CREATE DATABASE db1;
Database	Create Database
db1	CREATE DATABASE `db1` /*!40100 DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci */ /*!80016 DEFAULT ENCRYPTION='N' */
SET SESSION default_table_encryption=false;;
SET GLOBAL table_encryption_privilege_check=true;
ALTER DATABASE db1 DEFAULT ENCRYPTION='n';
SHOW WARNINGS;
Level	Code	Message
SHOW CREATE DATABASE db1;
Database	Create Database
db1	CREATE DATABASE `db1` /*!40100 DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci */ /*!80016 DEFAULT ENCRYPTION='N' */
SELECT SCHEMA_NAME, DEFAULT_ENCRYPTION
FROM INFORMATION_SCHEMA.SCHEMATA WHERE SCHEMA_NAME='db1';
SCHEMA_NAME	DEFAULT_ENCRYPTION
db1	NO
DROP DATABASE db1;
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=false;
`````````````````````````````````````````````````````````
# We expect failure because the encryption request is different from
# global 'default_table_encryption' setting.
# [ALTER DATABASE] Case 6 )
`````````````````````````````````````````````````````````
CREATE DATABASE db1 DEFAULT ENCRYPTION='n';
SHOW CREATE DATABASE db1;
Database	Create Database
db1	CREATE DATABASE `db1` /*!40100 DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci */ /*!80016 DEFAULT ENCRYPTION='N' */
SET SESSION default_table_encryption=false;;
SET GLOBAL table_encryption_privilege_check=true;
ALTER DATABASE db1 DEFAULT ENCRYPTION='y';
ERROR HY000: Database default encryption differ from 'default_table_encryption' setting, and user doesn't have enough privilege.
SHOW WARNINGS;
Level	Code	Message
Error	3827	Database default encryption differ from 'default_table_encryption' setting, and user doesn't have enough privilege.
SHOW CREATE DATABASE db1;
Database	Create Database
db1	CREATE DATABASE `db1` /*!40100 DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci */ /*!80016 DEFAULT ENCRYPTION='N' */
SELECT SCHEMA_NAME, DEFAULT_ENCRYPTION
FROM INFORMATION_SCHEMA.SCHEMATA WHERE SCHEMA_NAME='db1';
SCHEMA_NAME	DEFAULT_ENCRYPTION
db1	NO
DROP DATABASE db1;
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=false;
`````````````````````````````````````````````````````````
# We expect failure because the encryption request is different from
# global 'default_table_encryption' setting.
# [ALTER DATABASE] Case 7 )
`````````````````````````````````````````````````````````
CREATE DATABASE db1 DEFAULT ENCRYPTION='n';
SHOW CREATE DATABASE db1;
Database	Create Database
db1	CREATE DATABASE `db1` /*!40100 DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci */ /*!80016 DEFAULT ENCRYPTION='N' */
SET SESSION default_table_encryption=true;;
SET GLOBAL table_encryption_privilege_check=true;
ALTER DATABASE db1 DEFAULT ENCRYPTION='n';
ERROR HY000: Database default encryption differ from 'default_table_encryption' setting, and user doesn't have enough privilege.
SHOW WARNINGS;
Level	Code	Message
Error	3827	Database default encryption differ from 'default_table_encryption' setting, and user doesn't have enough privilege.
SHOW CREATE DATABASE db1;
Database	Create Database
db1	CREATE DATABASE `db1` /*!40100 DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci */ /*!80016 DEFAULT ENCRYPTION='N' */
SELECT SCHEMA_NAME, DEFAULT_ENCRYPTION
FROM INFORMATION_SCHEMA.SCHEMATA WHERE SCHEMA_NAME='db1';
SCHEMA_NAME	DEFAULT_ENCRYPTION
db1	NO
DROP DATABASE db1;
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=false;
# [ALTER DATABASE] Case 8 )
`````````````````````````````````````````````````````````
CREATE DATABASE db1 DEFAULT ENCRYPTION='n';
SHOW CREATE DATABASE db1;
Database	Create Database
db1	CREATE DATABASE `db1` /*!40100 DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci */ /*!80016 DEFAULT ENCRYPTION='N' */
SET SESSION default_table_encryption=true;;
SET GLOBAL table_encryption_privilege_check=true;
ALTER DATABASE db1 DEFAULT ENCRYPTION='y';
SHOW WARNINGS;
Level	Code	Message
SHOW CREATE DATABASE db1;
Database	Create Database
db1	CREATE DATABASE `db1` /*!40100 DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci */ /*!80016 DEFAULT ENCRYPTION='Y' */
SELECT SCHEMA_NAME, DEFAULT_ENCRYPTION
FROM INFORMATION_SCHEMA.SCHEMATA WHERE SCHEMA_NAME='db1';
SCHEMA_NAME	DEFAULT_ENCRYPTION
db1	YES
DROP DATABASE db1;
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=false;
# [ALTER DATABASE] Case 9 )
`````````````````````````````````````````````````````````
CREATE DATABASE db1 DEFAULT ENCRYPTION='n';
SHOW CREATE DATABASE db1;
Database	Create Database
db1	CREATE DATABASE `db1` /*!40100 DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci */ /*!80016 DEFAULT ENCRYPTION='N' */
# GRANT TABLE_ENCRYPTION_ADMIN
GRANT TABLE_ENCRYPTION_ADMIN ON *.* TO u1@localhost;
SET SESSION default_table_encryption=false;;
SET GLOBAL table_encryption_privilege_check=true;
ALTER DATABASE db1 DEFAULT ENCRYPTION='n';
SHOW WARNINGS;
Level	Code	Message
SHOW CREATE DATABASE db1;
Database	Create Database
db1	CREATE DATABASE `db1` /*!40100 DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci */ /*!80016 DEFAULT ENCRYPTION='N' */
SELECT SCHEMA_NAME, DEFAULT_ENCRYPTION
FROM INFORMATION_SCHEMA.SCHEMATA WHERE SCHEMA_NAME='db1';
SCHEMA_NAME	DEFAULT_ENCRYPTION
db1	NO
DROP DATABASE db1;
# REVOKE TABLE_ENCRYPTION_ADMIN
REVOKE TABLE_ENCRYPTION_ADMIN ON *.* FROM u1@localhost;
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=false;
# [ALTER DATABASE] Case 10 )
`````````````````````````````````````````````````````````
CREATE DATABASE db1 DEFAULT ENCRYPTION='n';
SHOW CREATE DATABASE db1;
Database	Create Database
db1	CREATE DATABASE `db1` /*!40100 DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci */ /*!80016 DEFAULT ENCRYPTION='N' */
# GRANT TABLE_ENCRYPTION_ADMIN
GRANT TABLE_ENCRYPTION_ADMIN ON *.* TO u1@localhost;
SET SESSION default_table_encryption=false;;
SET GLOBAL table_encryption_privilege_check=true;
ALTER DATABASE db1 DEFAULT ENCRYPTION='y';
SHOW WARNINGS;
Level	Code	Message
SHOW CREATE DATABASE db1;
Database	Create Database
db1	CREATE DATABASE `db1` /*!40100 DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci */ /*!80016 DEFAULT ENCRYPTION='Y' */
SELECT SCHEMA_NAME, DEFAULT_ENCRYPTION
FROM INFORMATION_SCHEMA.SCHEMATA WHERE SCHEMA_NAME='db1';
SCHEMA_NAME	DEFAULT_ENCRYPTION
db1	YES
DROP DATABASE db1;
# REVOKE TABLE_ENCRYPTION_ADMIN
REVOKE TABLE_ENCRYPTION_ADMIN ON *.* FROM u1@localhost;
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=false;
# [ALTER DATABASE] Case 11 )
`````````````````````````````````````````````````````````
CREATE DATABASE db1 DEFAULT ENCRYPTION='n';
SHOW CREATE DATABASE db1;
Database	Create Database
db1	CREATE DATABASE `db1` /*!40100 DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci */ /*!80016 DEFAULT ENCRYPTION='N' */
# GRANT TABLE_ENCRYPTION_ADMIN
GRANT TABLE_ENCRYPTION_ADMIN ON *.* TO u1@localhost;
SET SESSION default_table_encryption=true;;
SET GLOBAL table_encryption_privilege_check=true;
ALTER DATABASE db1 DEFAULT ENCRYPTION='n';
SHOW WARNINGS;
Level	Code	Message
SHOW CREATE DATABASE db1;
Database	Create Database
db1	CREATE DATABASE `db1` /*!40100 DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci */ /*!80016 DEFAULT ENCRYPTION='N' */
SELECT SCHEMA_NAME, DEFAULT_ENCRYPTION
FROM INFORMATION_SCHEMA.SCHEMATA WHERE SCHEMA_NAME='db1';
SCHEMA_NAME	DEFAULT_ENCRYPTION
db1	NO
DROP DATABASE db1;
# REVOKE TABLE_ENCRYPTION_ADMIN
REVOKE TABLE_ENCRYPTION_ADMIN ON *.* FROM u1@localhost;
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=false;
# [ALTER DATABASE] Case 12 )
`````````````````````````````````````````````````````````
CREATE DATABASE db1 DEFAULT ENCRYPTION='n';
SHOW CREATE DATABASE db1;
Database	Create Database
db1	CREATE DATABASE `db1` /*!40100 DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci */ /*!80016 DEFAULT ENCRYPTION='N' */
# GRANT TABLE_ENCRYPTION_ADMIN
GRANT TABLE_ENCRYPTION_ADMIN ON *.* TO u1@localhost;
SET SESSION default_table_encryption=true;;
SET GLOBAL table_encryption_privilege_check=true;
ALTER DATABASE db1 DEFAULT ENCRYPTION='y';
SHOW WARNINGS;
Level	Code	Message
SHOW CREATE DATABASE db1;
Database	Create Database
db1	CREATE DATABASE `db1` /*!40100 DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci */ /*!80016 DEFAULT ENCRYPTION='Y' */
SELECT SCHEMA_NAME, DEFAULT_ENCRYPTION
FROM INFORMATION_SCHEMA.SCHEMATA WHERE SCHEMA_NAME='db1';
SCHEMA_NAME	DEFAULT_ENCRYPTION
db1	YES
DROP DATABASE db1;
# REVOKE TABLE_ENCRYPTION_ADMIN
REVOKE TABLE_ENCRYPTION_ADMIN ON *.* FROM u1@localhost;
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=false;
`````````````````````````````````````````````````````````
# Following cases are with database DEFAULT ENCRYPTION 'y'
# [ALTER DATABASE] Case 13 )
`````````````````````````````````````````````````````````
CREATE DATABASE db1 DEFAULT ENCRYPTION='y';
SHOW CREATE DATABASE db1;
Database	Create Database
db1	CREATE DATABASE `db1` /*!40100 DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci */ /*!80016 DEFAULT ENCRYPTION='Y' */
SET SESSION default_table_encryption=false;;
SET GLOBAL table_encryption_privilege_check=false;
ALTER DATABASE db1 DEFAULT ENCRYPTION='n';
SHOW WARNINGS;
Level	Code	Message
SHOW CREATE DATABASE db1;
Database	Create Database
db1	CREATE DATABASE `db1` /*!40100 DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci */ /*!80016 DEFAULT ENCRYPTION='N' */
SELECT SCHEMA_NAME, DEFAULT_ENCRYPTION
FROM INFORMATION_SCHEMA.SCHEMATA WHERE SCHEMA_NAME='db1';
SCHEMA_NAME	DEFAULT_ENCRYPTION
db1	NO
DROP DATABASE db1;
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=false;
# [ALTER DATABASE] Case 14 )
`````````````````````````````````````````````````````````
CREATE DATABASE db1 DEFAULT ENCRYPTION='y';
SHOW CREATE DATABASE db1;
Database	Create Database
db1	CREATE DATABASE `db1` /*!40100 DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci */ /*!80016 DEFAULT ENCRYPTION='Y' */
SET SESSION default_table_encryption=false;;
SET GLOBAL table_encryption_privilege_check=false;
ALTER DATABASE db1 DEFAULT ENCRYPTION='y';
SHOW WARNINGS;
Level	Code	Message
SHOW CREATE DATABASE db1;
Database	Create Database
db1	CREATE DATABASE `db1` /*!40100 DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci */ /*!80016 DEFAULT ENCRYPTION='Y' */
SELECT SCHEMA_NAME, DEFAULT_ENCRYPTION
FROM INFORMATION_SCHEMA.SCHEMATA WHERE SCHEMA_NAME='db1';
SCHEMA_NAME	DEFAULT_ENCRYPTION
db1	YES
DROP DATABASE db1;
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=false;
# [ALTER DATABASE] Case 15 )
`````````````````````````````````````````````````````````
CREATE DATABASE db1 DEFAULT ENCRYPTION='y';
SHOW CREATE DATABASE db1;
Database	Create Database
db1	CREATE DATABASE `db1` /*!40100 DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci */ /*!80016 DEFAULT ENCRYPTION='Y' */
SET SESSION default_table_encryption=true;;
SET GLOBAL table_encryption_privilege_check=false;
ALTER DATABASE db1 DEFAULT ENCRYPTION='n';
SHOW WARNINGS;
Level	Code	Message
SHOW CREATE DATABASE db1;
Database	Create Database
db1	CREATE DATABASE `db1` /*!40100 DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci */ /*!80016 DEFAULT ENCRYPTION='N' */
SELECT SCHEMA_NAME, DEFAULT_ENCRYPTION
FROM INFORMATION_SCHEMA.SCHEMATA WHERE SCHEMA_NAME='db1';
SCHEMA_NAME	DEFAULT_ENCRYPTION
db1	NO
DROP DATABASE db1;
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=false;
# [ALTER DATABASE] Case 16 )
`````````````````````````````````````````````````````````
CREATE DATABASE db1 DEFAULT ENCRYPTION='y';
SHOW CREATE DATABASE db1;
Database	Create Database
db1	CREATE DATABASE `db1` /*!40100 DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci */ /*!80016 DEFAULT ENCRYPTION='Y' */
SET SESSION default_table_encryption=true;;
SET GLOBAL table_encryption_privilege_check=false;
ALTER DATABASE db1 DEFAULT ENCRYPTION='y';
SHOW WARNINGS;
Level	Code	Message
SHOW CREATE DATABASE db1;
Database	Create Database
db1	CREATE DATABASE `db1` /*!40100 DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci */ /*!80016 DEFAULT ENCRYPTION='Y' */
SELECT SCHEMA_NAME, DEFAULT_ENCRYPTION
FROM INFORMATION_SCHEMA.SCHEMATA WHERE SCHEMA_NAME='db1';
SCHEMA_NAME	DEFAULT_ENCRYPTION
db1	YES
DROP DATABASE db1;
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=false;
# [ALTER DATABASE] Case 17 )
`````````````````````````````````````````````````````````
CREATE DATABASE db1 DEFAULT ENCRYPTION='y';
SHOW CREATE DATABASE db1;
Database	Create Database
db1	CREATE DATABASE `db1` /*!40100 DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci */ /*!80016 DEFAULT ENCRYPTION='Y' */
SET SESSION default_table_encryption=false;;
SET GLOBAL table_encryption_privilege_check=true;
ALTER DATABASE db1 DEFAULT ENCRYPTION='n';
SHOW WARNINGS;
Level	Code	Message
SHOW CREATE DATABASE db1;
Database	Create Database
db1	CREATE DATABASE `db1` /*!40100 DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci */ /*!80016 DEFAULT ENCRYPTION='N' */
SELECT SCHEMA_NAME, DEFAULT_ENCRYPTION
FROM INFORMATION_SCHEMA.SCHEMATA WHERE SCHEMA_NAME='db1';
SCHEMA_NAME	DEFAULT_ENCRYPTION
db1	NO
DROP DATABASE db1;
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=false;
`````````````````````````````````````````````````````````
# We expect failure because the encryption request is different from
# global 'default_table_encryption' setting.
# [ALTER DATABASE] Case 18 )
`````````````````````````````````````````````````````````
CREATE DATABASE db1 DEFAULT ENCRYPTION='y';
SHOW CREATE DATABASE db1;
Database	Create Database
db1	CREATE DATABASE `db1` /*!40100 DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci */ /*!80016 DEFAULT ENCRYPTION='Y' */
SET SESSION default_table_encryption=false;;
SET GLOBAL table_encryption_privilege_check=true;
ALTER DATABASE db1 DEFAULT ENCRYPTION='y';
ERROR HY000: Database default encryption differ from 'default_table_encryption' setting, and user doesn't have enough privilege.
SHOW WARNINGS;
Level	Code	Message
Error	3827	Database default encryption differ from 'default_table_encryption' setting, and user doesn't have enough privilege.
SHOW CREATE DATABASE db1;
Database	Create Database
db1	CREATE DATABASE `db1` /*!40100 DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci */ /*!80016 DEFAULT ENCRYPTION='Y' */
SELECT SCHEMA_NAME, DEFAULT_ENCRYPTION
FROM INFORMATION_SCHEMA.SCHEMATA WHERE SCHEMA_NAME='db1';
SCHEMA_NAME	DEFAULT_ENCRYPTION
db1	YES
DROP DATABASE db1;
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=false;
`````````````````````````````````````````````````````````
# We expect failure because the encryption request is different from
# global 'default_table_encryption' setting.
# [ALTER DATABASE] Case 19 )
`````````````````````````````````````````````````````````
CREATE DATABASE db1 DEFAULT ENCRYPTION='y';
SHOW CREATE DATABASE db1;
Database	Create Database
db1	CREATE DATABASE `db1` /*!40100 DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci */ /*!80016 DEFAULT ENCRYPTION='Y' */
SET SESSION default_table_encryption=true;;
SET GLOBAL table_encryption_privilege_check=true;
ALTER DATABASE db1 DEFAULT ENCRYPTION='n';
ERROR HY000: Database default encryption differ from 'default_table_encryption' setting, and user doesn't have enough privilege.
SHOW WARNINGS;
Level	Code	Message
Error	3827	Database default encryption differ from 'default_table_encryption' setting, and user doesn't have enough privilege.
SHOW CREATE DATABASE db1;
Database	Create Database
db1	CREATE DATABASE `db1` /*!40100 DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci */ /*!80016 DEFAULT ENCRYPTION='Y' */
SELECT SCHEMA_NAME, DEFAULT_ENCRYPTION
FROM INFORMATION_SCHEMA.SCHEMATA WHERE SCHEMA_NAME='db1';
SCHEMA_NAME	DEFAULT_ENCRYPTION
db1	YES
DROP DATABASE db1;
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=false;
# [ALTER DATABASE] Case 20 )
`````````````````````````````````````````````````````````
CREATE DATABASE db1 DEFAULT ENCRYPTION='y';
SHOW CREATE DATABASE db1;
Database	Create Database
db1	CREATE DATABASE `db1` /*!40100 DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci */ /*!80016 DEFAULT ENCRYPTION='Y' */
SET SESSION default_table_encryption=true;;
SET GLOBAL table_encryption_privilege_check=true;
ALTER DATABASE db1 DEFAULT ENCRYPTION='y';
SHOW WARNINGS;
Level	Code	Message
SHOW CREATE DATABASE db1;
Database	Create Database
db1	CREATE DATABASE `db1` /*!40100 DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci */ /*!80016 DEFAULT ENCRYPTION='Y' */
SELECT SCHEMA_NAME, DEFAULT_ENCRYPTION
FROM INFORMATION_SCHEMA.SCHEMATA WHERE SCHEMA_NAME='db1';
SCHEMA_NAME	DEFAULT_ENCRYPTION
db1	YES
DROP DATABASE db1;
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=false;
# [ALTER DATABASE] Case 21 )
`````````````````````````````````````````````````````````
CREATE DATABASE db1 DEFAULT ENCRYPTION='y';
SHOW CREATE DATABASE db1;
Database	Create Database
db1	CREATE DATABASE `db1` /*!40100 DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci */ /*!80016 DEFAULT ENCRYPTION='Y' */
# GRANT TABLE_ENCRYPTION_ADMIN
GRANT TABLE_ENCRYPTION_ADMIN ON *.* TO u1@localhost;
SET SESSION default_table_encryption=false;;
SET GLOBAL table_encryption_privilege_check=true;
ALTER DATABASE db1 DEFAULT ENCRYPTION='n';
SHOW WARNINGS;
Level	Code	Message
SHOW CREATE DATABASE db1;
Database	Create Database
db1	CREATE DATABASE `db1` /*!40100 DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci */ /*!80016 DEFAULT ENCRYPTION='N' */
SELECT SCHEMA_NAME, DEFAULT_ENCRYPTION
FROM INFORMATION_SCHEMA.SCHEMATA WHERE SCHEMA_NAME='db1';
SCHEMA_NAME	DEFAULT_ENCRYPTION
db1	NO
DROP DATABASE db1;
# REVOKE TABLE_ENCRYPTION_ADMIN
REVOKE TABLE_ENCRYPTION_ADMIN ON *.* FROM u1@localhost;
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=false;
# [ALTER DATABASE] Case 22 )
`````````````````````````````````````````````````````````
CREATE DATABASE db1 DEFAULT ENCRYPTION='y';
SHOW CREATE DATABASE db1;
Database	Create Database
db1	CREATE DATABASE `db1` /*!40100 DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci */ /*!80016 DEFAULT ENCRYPTION='Y' */
# GRANT TABLE_ENCRYPTION_ADMIN
GRANT TABLE_ENCRYPTION_ADMIN ON *.* TO u1@localhost;
SET SESSION default_table_encryption=false;;
SET GLOBAL table_encryption_privilege_check=true;
ALTER DATABASE db1 DEFAULT ENCRYPTION='y';
SHOW WARNINGS;
Level	Code	Message
SHOW CREATE DATABASE db1;
Database	Create Database
db1	CREATE DATABASE `db1` /*!40100 DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci */ /*!80016 DEFAULT ENCRYPTION='Y' */
SELECT SCHEMA_NAME, DEFAULT_ENCRYPTION
FROM INFORMATION_SCHEMA.SCHEMATA WHERE SCHEMA_NAME='db1';
SCHEMA_NAME	DEFAULT_ENCRYPTION
db1	YES
DROP DATABASE db1;
# REVOKE TABLE_ENCRYPTION_ADMIN
REVOKE TABLE_ENCRYPTION_ADMIN ON *.* FROM u1@localhost;
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=false;
# [ALTER DATABASE] Case 23 )
`````````````````````````````````````````````````````````
CREATE DATABASE db1 DEFAULT ENCRYPTION='y';
SHOW CREATE DATABASE db1;
Database	Create Database
db1	CREATE DATABASE `db1` /*!40100 DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci */ /*!80016 DEFAULT ENCRYPTION='Y' */
# GRANT TABLE_ENCRYPTION_ADMIN
GRANT TABLE_ENCRYPTION_ADMIN ON *.* TO u1@localhost;
SET SESSION default_table_encryption=true;;
SET GLOBAL table_encryption_privilege_check=true;
ALTER DATABASE db1 DEFAULT ENCRYPTION='n';
SHOW WARNINGS;
Level	Code	Message
SHOW CREATE DATABASE db1;
Database	Create Database
db1	CREATE DATABASE `db1` /*!40100 DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci */ /*!80016 DEFAULT ENCRYPTION='N' */
SELECT SCHEMA_NAME, DEFAULT_ENCRYPTION
FROM INFORMATION_SCHEMA.SCHEMATA WHERE SCHEMA_NAME='db1';
SCHEMA_NAME	DEFAULT_ENCRYPTION
db1	NO
DROP DATABASE db1;
# REVOKE TABLE_ENCRYPTION_ADMIN
REVOKE TABLE_ENCRYPTION_ADMIN ON *.* FROM u1@localhost;
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=false;
# [ALTER DATABASE] Case 24 )
`````````````````````````````````````````````````````````
CREATE DATABASE db1 DEFAULT ENCRYPTION='y';
SHOW CREATE DATABASE db1;
Database	Create Database
db1	CREATE DATABASE `db1` /*!40100 DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci */ /*!80016 DEFAULT ENCRYPTION='Y' */
# GRANT TABLE_ENCRYPTION_ADMIN
GRANT TABLE_ENCRYPTION_ADMIN ON *.* TO u1@localhost;
SET SESSION default_table_encryption=true;;
SET GLOBAL table_encryption_privilege_check=true;
ALTER DATABASE db1 DEFAULT ENCRYPTION='y';
SHOW WARNINGS;
Level	Code	Message
SHOW CREATE DATABASE db1;
Database	Create Database
db1	CREATE DATABASE `db1` /*!40100 DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci */ /*!80016 DEFAULT ENCRYPTION='Y' */
SELECT SCHEMA_NAME, DEFAULT_ENCRYPTION
FROM INFORMATION_SCHEMA.SCHEMATA WHERE SCHEMA_NAME='db1';
SCHEMA_NAME	DEFAULT_ENCRYPTION
db1	YES
DROP DATABASE db1;
# REVOKE TABLE_ENCRYPTION_ADMIN
REVOKE TABLE_ENCRYPTION_ADMIN ON *.* FROM u1@localhost;
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=false;
`````````````````````````````````````````````````````````
# Invalid encryption option.
# [ALTER DATABASE] Case 25 )
`````````````````````````````````````````````````````````
CREATE DATABASE db1 DEFAULT ENCRYPTION='y';
SHOW CREATE DATABASE db1;
Database	Create Database
db1	CREATE DATABASE `db1` /*!40100 DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci */ /*!80016 DEFAULT ENCRYPTION='Y' */
SET SESSION default_table_encryption=false;;
SET GLOBAL table_encryption_privilege_check=false;
ALTER DATABASE db1 DEFAULT ENCRYPTION='k';
ERROR HY000: Incorrect argument (should be Y or N) value: 'k'
SHOW WARNINGS;
Level	Code	Message
Error	1525	Incorrect argument (should be Y or N) value: 'k'
SHOW CREATE DATABASE db1;
Database	Create Database
db1	CREATE DATABASE `db1` /*!40100 DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci */ /*!80016 DEFAULT ENCRYPTION='Y' */
SELECT SCHEMA_NAME, DEFAULT_ENCRYPTION
FROM INFORMATION_SCHEMA.SCHEMATA WHERE SCHEMA_NAME='db1';
SCHEMA_NAME	DEFAULT_ENCRYPTION
db1	YES
DROP DATABASE db1;
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=false;
# Cleanup
DROP USER u1@localhost;
SET GLOBAL debug= '-d,skip_table_encryption_admin_check_for_set';
