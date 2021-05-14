# ----------------------------------------------------------------------
# Setup
# Creating local configuration file for keyring component: component_keyring_file
# Creating manifest file for current MySQL server instance
# Re-starting mysql server with manifest file
# ----------------------------------------------------------------------
#
# WL#12261 Control (enforce and disable) table encryption
#
# Pre-define user u1, which is used in different tests below.
CREATE USER u1@localhost;
GRANT ALL ON db1.* TO u1@localhost;
GRANT CREATE TABLESPACE, PROCESS, SYSTEM_VARIABLES_ADMIN ON *.* TO u1@localhost;
SET GLOBAL debug= '+d,skip_table_encryption_admin_check_for_set';
# The test cases run ALTER TABLESPACE to check its encryption mode.
# The importance of this test is to check the way ALTER TABLESPACE
# updates the ENCRYPTION clause of tables in it.
# We run this command in various configuration as,
#
# - Setting table_encryption_privilege_check to true/false.
# - Setting per database default encryption to true/false.
# - With and without user holding TABLE_ENCRYPTION_ADMIN privilege.
# - Check for warnings generated.
#
`````````````````````````````````````````````````````````
# Unencrypted TABLESPACE to Unencrypted TABLESPACE (Nop)
# [ALTER TABLESPACE] Case 1 )
`````````````````````````````````````````````````````````
# Create required schema to run ALTER TABLESPACE.
CREATE TABLESPACE ts1 ADD DATAFILE 'df_u.ibd' ENCRYPTION='n';
CREATE DATABASE db1 DEFAULT ENCRYPTION='n';
CREATE TABLE db1.t1 (f1 int) TABLESPACE=ts1 ENCRYPTION='n';
CREATE TABLE db1.t2 (f1 int) TABLESPACE=ts1 ENCRYPTION='n';
SHOW CREATE TABLE db1.t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `ts1` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SHOW CREATE TABLE db1.t2;
Table	Create Table
t2	CREATE TABLE `t2` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `ts1` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SET SESSION default_table_encryption=false;
# Run ALTER TABLESPACE
SET GLOBAL table_encryption_privilege_check=false;
ALTER TABLESPACE ts1 ENCRYPTION='n';
SHOW WARNINGS;
Level	Code	Message
SHOW CREATE TABLE db1.t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `ts1` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SHOW CREATE TABLE db1.t2;
Table	Create Table
t2	CREATE TABLE `t2` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `ts1` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=false;
# clean up
DROP DATABASE db1;
DROP TABLESPACE ts1;
#
# [ALTER TABLESPACE] Case 2 )
`````````````````````````````````````````````````````````
# Create required schema to run ALTER TABLESPACE.
CREATE TABLESPACE ts1 ADD DATAFILE 'df_u.ibd' ENCRYPTION='n';
CREATE DATABASE db1 DEFAULT ENCRYPTION='n';
CREATE TABLE db1.t1 (f1 int) TABLESPACE=ts1 ENCRYPTION='n';
CREATE TABLE db1.t2 (f1 int) TABLESPACE=ts1 ENCRYPTION='n';
SHOW CREATE TABLE db1.t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `ts1` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SHOW CREATE TABLE db1.t2;
Table	Create Table
t2	CREATE TABLE `t2` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `ts1` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SET SESSION default_table_encryption=false;
# Run ALTER TABLESPACE
SET GLOBAL table_encryption_privilege_check=true;
ALTER TABLESPACE ts1 ENCRYPTION='n';
SHOW WARNINGS;
Level	Code	Message
SHOW CREATE TABLE db1.t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `ts1` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SHOW CREATE TABLE db1.t2;
Table	Create Table
t2	CREATE TABLE `t2` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `ts1` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=false;
# clean up
DROP DATABASE db1;
DROP TABLESPACE ts1;
#
`````````````````````````````````````````````````````````
# Encrypted TABLESPACE to Encrypted TABLESPACE (Nop)
# [ALTER TABLESPACE] Case 3 )
`````````````````````````````````````````````````````````
# Create required schema to run ALTER TABLESPACE.
CREATE TABLESPACE ts1 ADD DATAFILE 'df_u.ibd' ENCRYPTION='y';
CREATE DATABASE db1 DEFAULT ENCRYPTION='y';
CREATE TABLE db1.t1 (f1 int) TABLESPACE=ts1 ENCRYPTION='y';
CREATE TABLE db1.t2 (f1 int) TABLESPACE=ts1 ENCRYPTION='y';
SHOW CREATE TABLE db1.t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `ts1` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci /*!80016 ENCRYPTION='y' */
SHOW CREATE TABLE db1.t2;
Table	Create Table
t2	CREATE TABLE `t2` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `ts1` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci /*!80016 ENCRYPTION='y' */
SET SESSION default_table_encryption=true;
# Run ALTER TABLESPACE
SET GLOBAL table_encryption_privilege_check=false;
ALTER TABLESPACE ts1 ENCRYPTION='y';
SHOW WARNINGS;
Level	Code	Message
SHOW CREATE TABLE db1.t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `ts1` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci /*!80016 ENCRYPTION='y' */
SHOW CREATE TABLE db1.t2;
Table	Create Table
t2	CREATE TABLE `t2` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `ts1` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci /*!80016 ENCRYPTION='y' */
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=false;
# clean up
DROP DATABASE db1;
DROP TABLESPACE ts1;
#
# [ALTER TABLESPACE] Case 4 )
`````````````````````````````````````````````````````````
# Create required schema to run ALTER TABLESPACE.
CREATE TABLESPACE ts1 ADD DATAFILE 'df_u.ibd' ENCRYPTION='y';
CREATE DATABASE db1 DEFAULT ENCRYPTION='y';
CREATE TABLE db1.t1 (f1 int) TABLESPACE=ts1 ENCRYPTION='y';
CREATE TABLE db1.t2 (f1 int) TABLESPACE=ts1 ENCRYPTION='y';
SHOW CREATE TABLE db1.t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `ts1` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci /*!80016 ENCRYPTION='y' */
SHOW CREATE TABLE db1.t2;
Table	Create Table
t2	CREATE TABLE `t2` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `ts1` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci /*!80016 ENCRYPTION='y' */
SET SESSION default_table_encryption=true;
# Run ALTER TABLESPACE
SET GLOBAL table_encryption_privilege_check=true;
ALTER TABLESPACE ts1 ENCRYPTION='y';
SHOW WARNINGS;
Level	Code	Message
SHOW CREATE TABLE db1.t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `ts1` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci /*!80016 ENCRYPTION='y' */
SHOW CREATE TABLE db1.t2;
Table	Create Table
t2	CREATE TABLE `t2` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `ts1` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci /*!80016 ENCRYPTION='y' */
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=false;
# clean up
DROP DATABASE db1;
DROP TABLESPACE ts1;
#
# [ALTER TABLESPACE] Case 5 )
`````````````````````````````````````````````````````````
# Grant user with TABLE_ENCRYPTION_ADMIN
GRANT TABLE_ENCRYPTION_ADMIN ON *.* TO u1@localhost;
# Create required schema to run ALTER TABLESPACE.
CREATE TABLESPACE ts1 ADD DATAFILE 'df_u.ibd' ENCRYPTION='y';
CREATE DATABASE db1 DEFAULT ENCRYPTION='y';
CREATE TABLE db1.t1 (f1 int) TABLESPACE=ts1 ENCRYPTION='y';
CREATE TABLE db1.t2 (f1 int) TABLESPACE=ts1 ENCRYPTION='y';
SHOW CREATE TABLE db1.t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `ts1` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci /*!80016 ENCRYPTION='y' */
SHOW CREATE TABLE db1.t2;
Table	Create Table
t2	CREATE TABLE `t2` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `ts1` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci /*!80016 ENCRYPTION='y' */
SET SESSION default_table_encryption=true;
# Run ALTER TABLESPACE
SET GLOBAL table_encryption_privilege_check=true;
ALTER TABLESPACE ts1 ENCRYPTION='y';
SHOW WARNINGS;
Level	Code	Message
SHOW CREATE TABLE db1.t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `ts1` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci /*!80016 ENCRYPTION='y' */
SHOW CREATE TABLE db1.t2;
Table	Create Table
t2	CREATE TABLE `t2` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `ts1` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci /*!80016 ENCRYPTION='y' */
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=false;
# clean up
# Revoke TABLE_ENCRYPTION_ADMIN from user
REVOKE TABLE_ENCRYPTION_ADMIN ON *.* FROM u1@localhost;
DROP DATABASE db1;
DROP TABLESPACE ts1;
#
`````````````````````````````````````````````````````````
# Unencrypted TABLESPACE to encrypted TABLESPACE
# with database encryption default 'n'
# [ALTER TABLESPACE] Case 6 )
`````````````````````````````````````````````````````````
# Create required schema to run ALTER TABLESPACE.
CREATE TABLESPACE ts1 ADD DATAFILE 'df_u.ibd' ENCRYPTION='n';
CREATE DATABASE db1 DEFAULT ENCRYPTION='n';
CREATE TABLE db1.t1 (f1 int) TABLESPACE=ts1 ENCRYPTION='n';
CREATE TABLE db1.t2 (f1 int) TABLESPACE=ts1 ENCRYPTION='n';
SHOW CREATE TABLE db1.t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `ts1` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SHOW CREATE TABLE db1.t2;
Table	Create Table
t2	CREATE TABLE `t2` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `ts1` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SET SESSION default_table_encryption=true;
# Run ALTER TABLESPACE
SET GLOBAL table_encryption_privilege_check=false;
ALTER TABLESPACE ts1 ENCRYPTION='y';
SHOW WARNINGS;
Level	Code	Message
SHOW CREATE TABLE db1.t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `ts1` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci /*!80016 ENCRYPTION='Y' */
SHOW CREATE TABLE db1.t2;
Table	Create Table
t2	CREATE TABLE `t2` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `ts1` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci /*!80016 ENCRYPTION='Y' */
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=false;
# clean up
DROP DATABASE db1;
DROP TABLESPACE ts1;
#
`````````````````````````````````````````````````````````
# Reject creating encrypted table in database with default encryption='n'
# [ALTER TABLESPACE] Case 7 )
`````````````````````````````````````````````````````````
# Create required schema to run ALTER TABLESPACE.
CREATE TABLESPACE ts1 ADD DATAFILE 'df_u.ibd' ENCRYPTION='n';
CREATE DATABASE db1 DEFAULT ENCRYPTION='n';
CREATE TABLE db1.t1 (f1 int) TABLESPACE=ts1 ENCRYPTION='n';
CREATE TABLE db1.t2 (f1 int) TABLESPACE=ts1 ENCRYPTION='n';
SHOW CREATE TABLE db1.t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `ts1` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SHOW CREATE TABLE db1.t2;
Table	Create Table
t2	CREATE TABLE `t2` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `ts1` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SET SESSION default_table_encryption=true;
# Run ALTER TABLESPACE
SET GLOBAL table_encryption_privilege_check=true;
ALTER TABLESPACE ts1 ENCRYPTION='y';
ERROR HY000: This tablespace can't be encrypted, because one of table's schema has default encryption OFF and user doesn't have enough privilege.
SHOW WARNINGS;
Level	Code	Message
Error	3829	This tablespace can't be encrypted, because one of table's schema has default encryption OFF and user doesn't have enough privilege.
SHOW CREATE TABLE db1.t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `ts1` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SHOW CREATE TABLE db1.t2;
Table	Create Table
t2	CREATE TABLE `t2` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `ts1` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=false;
# clean up
DROP DATABASE db1;
DROP TABLESPACE ts1;
#
# [ALTER TABLESPACE] Case 8 )
`````````````````````````````````````````````````````````
# Grant user with TABLE_ENCRYPTION_ADMIN
GRANT TABLE_ENCRYPTION_ADMIN ON *.* TO u1@localhost;
# Create required schema to run ALTER TABLESPACE.
CREATE TABLESPACE ts1 ADD DATAFILE 'df_u.ibd' ENCRYPTION='n';
CREATE DATABASE db1 DEFAULT ENCRYPTION='n';
CREATE TABLE db1.t1 (f1 int) TABLESPACE=ts1 ENCRYPTION='n';
CREATE TABLE db1.t2 (f1 int) TABLESPACE=ts1 ENCRYPTION='n';
SHOW CREATE TABLE db1.t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `ts1` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SHOW CREATE TABLE db1.t2;
Table	Create Table
t2	CREATE TABLE `t2` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `ts1` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SET SESSION default_table_encryption=true;
# Run ALTER TABLESPACE
SET GLOBAL table_encryption_privilege_check=true;
ALTER TABLESPACE ts1 ENCRYPTION='y';
SHOW WARNINGS;
Level	Code	Message
SHOW CREATE TABLE db1.t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `ts1` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci /*!80016 ENCRYPTION='Y' */
SHOW CREATE TABLE db1.t2;
Table	Create Table
t2	CREATE TABLE `t2` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `ts1` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci /*!80016 ENCRYPTION='Y' */
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=false;
# clean up
# Revoke TABLE_ENCRYPTION_ADMIN from user
REVOKE TABLE_ENCRYPTION_ADMIN ON *.* FROM u1@localhost;
DROP DATABASE db1;
DROP TABLESPACE ts1;
#
# With some tables without ENCRYPTION clause;
# [ALTER TABLESPACE] Case 9 )
`````````````````````````````````````````````````````````
# Create required schema to run ALTER TABLESPACE.
CREATE TABLESPACE ts1 ADD DATAFILE 'df_u.ibd' ENCRYPTION='n';
CREATE DATABASE db1 DEFAULT ENCRYPTION='n';
CREATE TABLE db1.t1 (f1 int) TABLESPACE=ts1 ENCRYPTION='n';
CREATE TABLE db1.t2 (f1 int) TABLESPACE=ts1;
SHOW CREATE TABLE db1.t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `ts1` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SHOW CREATE TABLE db1.t2;
Table	Create Table
t2	CREATE TABLE `t2` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `ts1` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SET SESSION default_table_encryption=true;
# Run ALTER TABLESPACE
SET GLOBAL table_encryption_privilege_check=false;
ALTER TABLESPACE ts1 ENCRYPTION='y';
SHOW WARNINGS;
Level	Code	Message
SHOW CREATE TABLE db1.t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `ts1` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci /*!80016 ENCRYPTION='Y' */
SHOW CREATE TABLE db1.t2;
Table	Create Table
t2	CREATE TABLE `t2` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `ts1` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci /*!80016 ENCRYPTION='Y' */
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=false;
# clean up
DROP DATABASE db1;
DROP TABLESPACE ts1;
#
`````````````````````````````````````````````````````````
# Reject creating encrypted table in database with default encryption='n'
# [ALTER TABLESPACE] Case 10 )
`````````````````````````````````````````````````````````
# Create required schema to run ALTER TABLESPACE.
CREATE TABLESPACE ts1 ADD DATAFILE 'df_u.ibd' ENCRYPTION='n';
CREATE DATABASE db1 DEFAULT ENCRYPTION='n';
CREATE TABLE db1.t1 (f1 int) TABLESPACE=ts1 ENCRYPTION='n';
CREATE TABLE db1.t2 (f1 int) TABLESPACE=ts1;
SHOW CREATE TABLE db1.t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `ts1` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SHOW CREATE TABLE db1.t2;
Table	Create Table
t2	CREATE TABLE `t2` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `ts1` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SET SESSION default_table_encryption=true;
# Run ALTER TABLESPACE
SET GLOBAL table_encryption_privilege_check=true;
ALTER TABLESPACE ts1 ENCRYPTION='y';
ERROR HY000: This tablespace can't be encrypted, because one of table's schema has default encryption OFF and user doesn't have enough privilege.
SHOW WARNINGS;
Level	Code	Message
Error	3829	This tablespace can't be encrypted, because one of table's schema has default encryption OFF and user doesn't have enough privilege.
SHOW CREATE TABLE db1.t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `ts1` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SHOW CREATE TABLE db1.t2;
Table	Create Table
t2	CREATE TABLE `t2` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `ts1` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=false;
# clean up
DROP DATABASE db1;
DROP TABLESPACE ts1;
#
# [ALTER TABLESPACE] Case 11 )
`````````````````````````````````````````````````````````
# Grant user with TABLE_ENCRYPTION_ADMIN
GRANT TABLE_ENCRYPTION_ADMIN ON *.* TO u1@localhost;
# Create required schema to run ALTER TABLESPACE.
CREATE TABLESPACE ts1 ADD DATAFILE 'df_u.ibd' ENCRYPTION='n';
CREATE DATABASE db1 DEFAULT ENCRYPTION='n';
CREATE TABLE db1.t1 (f1 int) TABLESPACE=ts1 ENCRYPTION='n';
CREATE TABLE db1.t2 (f1 int) TABLESPACE=ts1;
SHOW CREATE TABLE db1.t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `ts1` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SHOW CREATE TABLE db1.t2;
Table	Create Table
t2	CREATE TABLE `t2` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `ts1` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SET SESSION default_table_encryption=true;
# Run ALTER TABLESPACE
SET GLOBAL table_encryption_privilege_check=true;
ALTER TABLESPACE ts1 ENCRYPTION='y';
SHOW WARNINGS;
Level	Code	Message
SHOW CREATE TABLE db1.t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `ts1` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci /*!80016 ENCRYPTION='Y' */
SHOW CREATE TABLE db1.t2;
Table	Create Table
t2	CREATE TABLE `t2` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `ts1` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci /*!80016 ENCRYPTION='Y' */
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=false;
# clean up
# Revoke TABLE_ENCRYPTION_ADMIN from user
REVOKE TABLE_ENCRYPTION_ADMIN ON *.* FROM u1@localhost;
DROP DATABASE db1;
DROP TABLESPACE ts1;
#
# With all tables without ENCRYPTION clause;
# [ALTER TABLESPACE] Case 12 )
`````````````````````````````````````````````````````````
# Create required schema to run ALTER TABLESPACE.
CREATE TABLESPACE ts1 ADD DATAFILE 'df_u.ibd' ENCRYPTION='n';
CREATE DATABASE db1 DEFAULT ENCRYPTION='n';
CREATE TABLE db1.t1 (f1 int) TABLESPACE=ts1;
CREATE TABLE db1.t2 (f1 int) TABLESPACE=ts1;
SHOW CREATE TABLE db1.t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `ts1` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SHOW CREATE TABLE db1.t2;
Table	Create Table
t2	CREATE TABLE `t2` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `ts1` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SET SESSION default_table_encryption=true;
# Run ALTER TABLESPACE
SET GLOBAL table_encryption_privilege_check=false;
ALTER TABLESPACE ts1 ENCRYPTION='y';
SHOW WARNINGS;
Level	Code	Message
SHOW CREATE TABLE db1.t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `ts1` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci /*!80016 ENCRYPTION='Y' */
SHOW CREATE TABLE db1.t2;
Table	Create Table
t2	CREATE TABLE `t2` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `ts1` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci /*!80016 ENCRYPTION='Y' */
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=false;
# clean up
DROP DATABASE db1;
DROP TABLESPACE ts1;
#
`````````````````````````````````````````````````````````
# Request to create encrypted tablespace with default_table_encryption='n'
# [ALTER TABLESPACE] Case 13 )
`````````````````````````````````````````````````````````
# Create required schema to run ALTER TABLESPACE.
CREATE TABLESPACE ts1 ADD DATAFILE 'df_u.ibd' ENCRYPTION='n';
CREATE DATABASE db1 DEFAULT ENCRYPTION='n';
CREATE TABLE db1.t1 (f1 int) TABLESPACE=ts1;
CREATE TABLE db1.t2 (f1 int) TABLESPACE=ts1;
SHOW CREATE TABLE db1.t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `ts1` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SHOW CREATE TABLE db1.t2;
Table	Create Table
t2	CREATE TABLE `t2` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `ts1` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SET SESSION default_table_encryption=true;
# Run ALTER TABLESPACE
SET GLOBAL table_encryption_privilege_check=true;
ALTER TABLESPACE ts1 ENCRYPTION='y';
ERROR HY000: This tablespace can't be encrypted, because one of table's schema has default encryption OFF and user doesn't have enough privilege.
SHOW WARNINGS;
Level	Code	Message
Error	3829	This tablespace can't be encrypted, because one of table's schema has default encryption OFF and user doesn't have enough privilege.
SHOW CREATE TABLE db1.t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `ts1` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SHOW CREATE TABLE db1.t2;
Table	Create Table
t2	CREATE TABLE `t2` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `ts1` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=false;
# clean up
DROP DATABASE db1;
DROP TABLESPACE ts1;
#
# [ALTER TABLESPACE] Case 14 )
`````````````````````````````````````````````````````````
# Grant user with TABLE_ENCRYPTION_ADMIN
GRANT TABLE_ENCRYPTION_ADMIN ON *.* TO u1@localhost;
# Create required schema to run ALTER TABLESPACE.
CREATE TABLESPACE ts1 ADD DATAFILE 'df_u.ibd' ENCRYPTION='n';
CREATE DATABASE db1 DEFAULT ENCRYPTION='n';
CREATE TABLE db1.t1 (f1 int) TABLESPACE=ts1;
CREATE TABLE db1.t2 (f1 int) TABLESPACE=ts1;
SHOW CREATE TABLE db1.t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `ts1` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SHOW CREATE TABLE db1.t2;
Table	Create Table
t2	CREATE TABLE `t2` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `ts1` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SET SESSION default_table_encryption=true;
# Run ALTER TABLESPACE
SET GLOBAL table_encryption_privilege_check=true;
ALTER TABLESPACE ts1 ENCRYPTION='y';
SHOW WARNINGS;
Level	Code	Message
SHOW CREATE TABLE db1.t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `ts1` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci /*!80016 ENCRYPTION='Y' */
SHOW CREATE TABLE db1.t2;
Table	Create Table
t2	CREATE TABLE `t2` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `ts1` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci /*!80016 ENCRYPTION='Y' */
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=false;
# clean up
# Revoke TABLE_ENCRYPTION_ADMIN from user
REVOKE TABLE_ENCRYPTION_ADMIN ON *.* FROM u1@localhost;
DROP DATABASE db1;
DROP TABLESPACE ts1;
#
`````````````````````````````````````````````````````````
# Unencrypted TABLESPACE to encrypted TABLESPACE
# with database encryption 'y'
# [ALTER TABLESPACE] Case 15 )
`````````````````````````````````````````````````````````
# Create required schema to run ALTER TABLESPACE.
CREATE TABLESPACE ts1 ADD DATAFILE 'df_u.ibd' ENCRYPTION='n';
CREATE DATABASE db1 DEFAULT ENCRYPTION='y';
CREATE TABLE db1.t1 (f1 int) TABLESPACE=ts1 ENCRYPTION='n';
Warnings:
Warning	3824	Creating an unencrypted table in a database with default encryption enabled.
CREATE TABLE db1.t2 (f1 int) TABLESPACE=ts1 ENCRYPTION='n';
Warnings:
Warning	3824	Creating an unencrypted table in a database with default encryption enabled.
SHOW CREATE TABLE db1.t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `ts1` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci /*!80016 ENCRYPTION='n' */
SHOW CREATE TABLE db1.t2;
Table	Create Table
t2	CREATE TABLE `t2` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `ts1` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci /*!80016 ENCRYPTION='n' */
SET SESSION default_table_encryption=true;
# Run ALTER TABLESPACE
SET GLOBAL table_encryption_privilege_check=false;
ALTER TABLESPACE ts1 ENCRYPTION='y';
SHOW WARNINGS;
Level	Code	Message
SHOW CREATE TABLE db1.t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `ts1` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci /*!80016 ENCRYPTION='Y' */
SHOW CREATE TABLE db1.t2;
Table	Create Table
t2	CREATE TABLE `t2` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `ts1` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci /*!80016 ENCRYPTION='Y' */
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=false;
# clean up
DROP DATABASE db1;
DROP TABLESPACE ts1;
#
# [ALTER TABLESPACE] Case 16 )
`````````````````````````````````````````````````````````
# Create required schema to run ALTER TABLESPACE.
CREATE TABLESPACE ts1 ADD DATAFILE 'df_u.ibd' ENCRYPTION='n';
CREATE DATABASE db1 DEFAULT ENCRYPTION='y';
CREATE TABLE db1.t1 (f1 int) TABLESPACE=ts1 ENCRYPTION='n';
Warnings:
Warning	3824	Creating an unencrypted table in a database with default encryption enabled.
CREATE TABLE db1.t2 (f1 int) TABLESPACE=ts1 ENCRYPTION='n';
Warnings:
Warning	3824	Creating an unencrypted table in a database with default encryption enabled.
SHOW CREATE TABLE db1.t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `ts1` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci /*!80016 ENCRYPTION='n' */
SHOW CREATE TABLE db1.t2;
Table	Create Table
t2	CREATE TABLE `t2` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `ts1` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci /*!80016 ENCRYPTION='n' */
SET SESSION default_table_encryption=true;
# Run ALTER TABLESPACE
SET GLOBAL table_encryption_privilege_check=true;
ALTER TABLESPACE ts1 ENCRYPTION='y';
SHOW WARNINGS;
Level	Code	Message
SHOW CREATE TABLE db1.t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `ts1` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci /*!80016 ENCRYPTION='Y' */
SHOW CREATE TABLE db1.t2;
Table	Create Table
t2	CREATE TABLE `t2` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `ts1` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci /*!80016 ENCRYPTION='Y' */
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=false;
# clean up
DROP DATABASE db1;
DROP TABLESPACE ts1;
#
# [ALTER TABLESPACE] Case 17 )
`````````````````````````````````````````````````````````
# Grant user with TABLE_ENCRYPTION_ADMIN
GRANT TABLE_ENCRYPTION_ADMIN ON *.* TO u1@localhost;
# Create required schema to run ALTER TABLESPACE.
CREATE TABLESPACE ts1 ADD DATAFILE 'df_u.ibd' ENCRYPTION='n';
CREATE DATABASE db1 DEFAULT ENCRYPTION='y';
CREATE TABLE db1.t1 (f1 int) TABLESPACE=ts1 ENCRYPTION='n';
Warnings:
Warning	3824	Creating an unencrypted table in a database with default encryption enabled.
CREATE TABLE db1.t2 (f1 int) TABLESPACE=ts1 ENCRYPTION='n';
Warnings:
Warning	3824	Creating an unencrypted table in a database with default encryption enabled.
SHOW CREATE TABLE db1.t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `ts1` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci /*!80016 ENCRYPTION='n' */
SHOW CREATE TABLE db1.t2;
Table	Create Table
t2	CREATE TABLE `t2` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `ts1` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci /*!80016 ENCRYPTION='n' */
SET SESSION default_table_encryption=true;
# Run ALTER TABLESPACE
SET GLOBAL table_encryption_privilege_check=true;
ALTER TABLESPACE ts1 ENCRYPTION='y';
SHOW WARNINGS;
Level	Code	Message
SHOW CREATE TABLE db1.t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `ts1` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci /*!80016 ENCRYPTION='Y' */
SHOW CREATE TABLE db1.t2;
Table	Create Table
t2	CREATE TABLE `t2` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `ts1` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci /*!80016 ENCRYPTION='Y' */
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=false;
# clean up
# Revoke TABLE_ENCRYPTION_ADMIN from user
REVOKE TABLE_ENCRYPTION_ADMIN ON *.* FROM u1@localhost;
DROP DATABASE db1;
DROP TABLESPACE ts1;
#
# Case 17.1 With some tables without ENCRYPTION clause;
# CREATE table using unencrypted tablespace without
# ENCRYPTION clause would inherit ENCRYPTION from database.
# This makes CREATE TABLE fail because the ENCRYPTION clause
# and the tablespace encryption type mismatches. The test
# encryption.create_table does test this.
# Case 17.2 With all tables without ENCRYPTION clause;
# Behavior would be same as described in Case 17.1
`````````````````````````````````````````````````````````
# Encrypted TABLESPACE to unencrypted TABLESPACE
# with database encryption 'n'
# [ALTER TABLESPACE] Case 18 )
`````````````````````````````````````````````````````````
# Create required schema to run ALTER TABLESPACE.
CREATE TABLESPACE ts1 ADD DATAFILE 'df_u.ibd' ENCRYPTION='y';
CREATE DATABASE db1 DEFAULT ENCRYPTION='n';
CREATE TABLE db1.t1 (f1 int) TABLESPACE=ts1 ENCRYPTION='y';
CREATE TABLE db1.t2 (f1 int) TABLESPACE=ts1 ENCRYPTION='y';
SHOW CREATE TABLE db1.t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `ts1` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci /*!80016 ENCRYPTION='y' */
SHOW CREATE TABLE db1.t2;
Table	Create Table
t2	CREATE TABLE `t2` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `ts1` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci /*!80016 ENCRYPTION='y' */
SET SESSION default_table_encryption=false;
# Run ALTER TABLESPACE
SET GLOBAL table_encryption_privilege_check=false;
ALTER TABLESPACE ts1 ENCRYPTION='n';
SHOW WARNINGS;
Level	Code	Message
SHOW CREATE TABLE db1.t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `ts1` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SHOW CREATE TABLE db1.t2;
Table	Create Table
t2	CREATE TABLE `t2` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `ts1` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=false;
# clean up
DROP DATABASE db1;
DROP TABLESPACE ts1;
#
# [ALTER TABLESPACE] Case 19 )
`````````````````````````````````````````````````````````
# Create required schema to run ALTER TABLESPACE.
CREATE TABLESPACE ts1 ADD DATAFILE 'df_u.ibd' ENCRYPTION='y';
CREATE DATABASE db1 DEFAULT ENCRYPTION='n';
CREATE TABLE db1.t1 (f1 int) TABLESPACE=ts1 ENCRYPTION='y';
CREATE TABLE db1.t2 (f1 int) TABLESPACE=ts1 ENCRYPTION='y';
SHOW CREATE TABLE db1.t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `ts1` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci /*!80016 ENCRYPTION='y' */
SHOW CREATE TABLE db1.t2;
Table	Create Table
t2	CREATE TABLE `t2` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `ts1` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci /*!80016 ENCRYPTION='y' */
SET SESSION default_table_encryption=false;
# Run ALTER TABLESPACE
SET GLOBAL table_encryption_privilege_check=true;
ALTER TABLESPACE ts1 ENCRYPTION='n';
SHOW WARNINGS;
Level	Code	Message
SHOW CREATE TABLE db1.t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `ts1` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SHOW CREATE TABLE db1.t2;
Table	Create Table
t2	CREATE TABLE `t2` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `ts1` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=false;
# clean up
DROP DATABASE db1;
DROP TABLESPACE ts1;
#
# [ALTER TABLESPACE] Case 20 )
`````````````````````````````````````````````````````````
# Grant user with TABLE_ENCRYPTION_ADMIN
GRANT TABLE_ENCRYPTION_ADMIN ON *.* TO u1@localhost;
# Create required schema to run ALTER TABLESPACE.
CREATE TABLESPACE ts1 ADD DATAFILE 'df_u.ibd' ENCRYPTION='y';
CREATE DATABASE db1 DEFAULT ENCRYPTION='n';
CREATE TABLE db1.t1 (f1 int) TABLESPACE=ts1 ENCRYPTION='y';
CREATE TABLE db1.t2 (f1 int) TABLESPACE=ts1 ENCRYPTION='y';
SHOW CREATE TABLE db1.t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `ts1` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci /*!80016 ENCRYPTION='y' */
SHOW CREATE TABLE db1.t2;
Table	Create Table
t2	CREATE TABLE `t2` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `ts1` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci /*!80016 ENCRYPTION='y' */
SET SESSION default_table_encryption=false;
# Run ALTER TABLESPACE
SET GLOBAL table_encryption_privilege_check=true;
ALTER TABLESPACE ts1 ENCRYPTION='n';
SHOW WARNINGS;
Level	Code	Message
SHOW CREATE TABLE db1.t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `ts1` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SHOW CREATE TABLE db1.t2;
Table	Create Table
t2	CREATE TABLE `t2` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `ts1` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=false;
# clean up
# Revoke TABLE_ENCRYPTION_ADMIN from user
REVOKE TABLE_ENCRYPTION_ADMIN ON *.* FROM u1@localhost;
DROP DATABASE db1;
DROP TABLESPACE ts1;
#
`````````````````````````````````````````````````````````
# Encrypted TABLESPACE to unencrypted TABLESPACE
# with database encryption 'y'
# [ALTER TABLESPACE] Case 21 )
`````````````````````````````````````````````````````````
# Create required schema to run ALTER TABLESPACE.
CREATE TABLESPACE ts1 ADD DATAFILE 'df_u.ibd' ENCRYPTION='y';
CREATE DATABASE db1 DEFAULT ENCRYPTION='y';
CREATE TABLE db1.t1 (f1 int) TABLESPACE=ts1 ENCRYPTION='y';
CREATE TABLE db1.t2 (f1 int) TABLESPACE=ts1 ENCRYPTION='y';
SHOW CREATE TABLE db1.t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `ts1` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci /*!80016 ENCRYPTION='y' */
SHOW CREATE TABLE db1.t2;
Table	Create Table
t2	CREATE TABLE `t2` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `ts1` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci /*!80016 ENCRYPTION='y' */
SET SESSION default_table_encryption=false;
# Run ALTER TABLESPACE
SET GLOBAL table_encryption_privilege_check=false;
ALTER TABLESPACE ts1 ENCRYPTION='n';
Warnings:
Warning	3824	Creating an unencrypted table in a database with default encryption enabled.
SHOW WARNINGS;
Level	Code	Message
Warning	3824	Creating an unencrypted table in a database with default encryption enabled.
SHOW CREATE TABLE db1.t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `ts1` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci /*!80016 ENCRYPTION='N' */
SHOW CREATE TABLE db1.t2;
Table	Create Table
t2	CREATE TABLE `t2` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `ts1` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci /*!80016 ENCRYPTION='N' */
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=false;
# clean up
DROP DATABASE db1;
DROP TABLESPACE ts1;
#
`````````````````````````````````````````````````````````
# Request to create unencrypted tablespace with default_table_encryption='y'
# [ALTER TABLESPACE] Case 22 )
`````````````````````````````````````````````````````````
# Create required schema to run ALTER TABLESPACE.
CREATE TABLESPACE ts1 ADD DATAFILE 'df_u.ibd' ENCRYPTION='y';
CREATE DATABASE db1 DEFAULT ENCRYPTION='y';
CREATE TABLE db1.t1 (f1 int) TABLESPACE=ts1 ENCRYPTION='y';
CREATE TABLE db1.t2 (f1 int) TABLESPACE=ts1 ENCRYPTION='y';
SHOW CREATE TABLE db1.t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `ts1` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci /*!80016 ENCRYPTION='y' */
SHOW CREATE TABLE db1.t2;
Table	Create Table
t2	CREATE TABLE `t2` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `ts1` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci /*!80016 ENCRYPTION='y' */
SET SESSION default_table_encryption=false;
# Run ALTER TABLESPACE
SET GLOBAL table_encryption_privilege_check=true;
ALTER TABLESPACE ts1 ENCRYPTION='n';
ERROR HY000: This tablespace can't be decrypted, because one of table's schema has default encryption ON and user doesn't have enough privilege.
SHOW WARNINGS;
Level	Code	Message
Error	3830	This tablespace can't be decrypted, because one of table's schema has default encryption ON and user doesn't have enough privilege.
SHOW CREATE TABLE db1.t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `ts1` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci /*!80016 ENCRYPTION='y' */
SHOW CREATE TABLE db1.t2;
Table	Create Table
t2	CREATE TABLE `t2` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `ts1` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci /*!80016 ENCRYPTION='y' */
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=false;
# clean up
DROP DATABASE db1;
DROP TABLESPACE ts1;
#
# [ALTER TABLESPACE] Case 23 )
`````````````````````````````````````````````````````````
# Grant user with TABLE_ENCRYPTION_ADMIN
GRANT TABLE_ENCRYPTION_ADMIN ON *.* TO u1@localhost;
# Create required schema to run ALTER TABLESPACE.
CREATE TABLESPACE ts1 ADD DATAFILE 'df_u.ibd' ENCRYPTION='y';
CREATE DATABASE db1 DEFAULT ENCRYPTION='y';
CREATE TABLE db1.t1 (f1 int) TABLESPACE=ts1 ENCRYPTION='y';
CREATE TABLE db1.t2 (f1 int) TABLESPACE=ts1 ENCRYPTION='y';
SHOW CREATE TABLE db1.t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `ts1` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci /*!80016 ENCRYPTION='y' */
SHOW CREATE TABLE db1.t2;
Table	Create Table
t2	CREATE TABLE `t2` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `ts1` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci /*!80016 ENCRYPTION='y' */
SET SESSION default_table_encryption=false;
# Run ALTER TABLESPACE
SET GLOBAL table_encryption_privilege_check=true;
ALTER TABLESPACE ts1 ENCRYPTION='n';
Warnings:
Warning	3824	Creating an unencrypted table in a database with default encryption enabled.
SHOW WARNINGS;
Level	Code	Message
Warning	3824	Creating an unencrypted table in a database with default encryption enabled.
SHOW CREATE TABLE db1.t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `ts1` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci /*!80016 ENCRYPTION='N' */
SHOW CREATE TABLE db1.t2;
Table	Create Table
t2	CREATE TABLE `t2` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `ts1` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci /*!80016 ENCRYPTION='N' */
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=false;
# clean up
# Revoke TABLE_ENCRYPTION_ADMIN from user
REVOKE TABLE_ENCRYPTION_ADMIN ON *.* FROM u1@localhost;
DROP DATABASE db1;
DROP TABLESPACE ts1;
#
# Clean up.
DROP USER u1@localhost;
SET GLOBAL debug= '-d,skip_table_encryption_admin_check_for_set';
# ----------------------------------------------------------------------
# Teardown
# Removing manifest file for current MySQL server instance
# Removing local keyring file for keyring component: component_keyring_file
# Removing local configuration file for keyring component: component_keyring_file
# Restarting server without the manifest file
# ----------------------------------------------------------------------
