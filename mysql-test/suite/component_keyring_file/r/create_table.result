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
# This test run CREATE TABLE in different configurations,
#
# - Setting table_encryption_privilege_check to true/false.
# - Setting default_table_encryption to true/false.
# - With and without ENCRYPTION clause.
# - With and without user holding TABLE_ENCRYPTION_ADMIN privilege.
# - Test SHOW CREATE TABLE
# - Test INFORMATION_SCHEMA.TABLES.CREATE_OPTIONS
# - Check for warnings generated.
#
`````````````````````````````````````````````````````````
# Test CREATE TABLE on DATABASE with ENCRYPTION 'y/n'
# and with different values for system variable
# 'table_encryption_privilege_check' and 'default_table_encryption'
`````````````````````````````````````````````````````````
# CREATE TABLE without ENCRYPTION clause
# [CREATE TABLE] Case 1 )
`````````````````````````````````````````````````````````
CREATE DATABASE db1 DEFAULT ENCRYPTION='n';
SET GLOBAL table_encryption_privilege_check=false;
CREATE TABLE db1.t1 (f1 int);
SHOW CREATE TABLE db1.t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SELECT TABLE_NAME, CREATE_OPTIONS FROM INFORMATION_SCHEMA.TABLES
WHERE TABLE_NAME='t1';
TABLE_NAME	CREATE_OPTIONS
t1	
DROP TABLE db1.t1;
DROP DATABASE db1;
SET GLOBAL table_encryption_privilege_check=false;
# [CREATE TABLE] Case 2 )
`````````````````````````````````````````````````````````
CREATE DATABASE db1 DEFAULT ENCRYPTION='y';
SET GLOBAL table_encryption_privilege_check=false;
CREATE TABLE db1.t1 (f1 int);
SHOW CREATE TABLE db1.t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci ENCRYPTION='Y'
SELECT TABLE_NAME, CREATE_OPTIONS FROM INFORMATION_SCHEMA.TABLES
WHERE TABLE_NAME='t1';
TABLE_NAME	CREATE_OPTIONS
t1	ENCRYPTION='Y'
DROP TABLE db1.t1;
DROP DATABASE db1;
SET GLOBAL table_encryption_privilege_check=false;
# [CREATE TABLE] Case 3 )
`````````````````````````````````````````````````````````
CREATE DATABASE db1 DEFAULT ENCRYPTION='n';
SET GLOBAL table_encryption_privilege_check=true;
CREATE TABLE db1.t1 (f1 int);
SHOW CREATE TABLE db1.t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SELECT TABLE_NAME, CREATE_OPTIONS FROM INFORMATION_SCHEMA.TABLES
WHERE TABLE_NAME='t1';
TABLE_NAME	CREATE_OPTIONS
t1	
DROP TABLE db1.t1;
DROP DATABASE db1;
SET GLOBAL table_encryption_privilege_check=false;
# [CREATE TABLE] Case 4 )
`````````````````````````````````````````````````````````
CREATE DATABASE db1 DEFAULT ENCRYPTION='y';
SET GLOBAL table_encryption_privilege_check=true;
CREATE TABLE db1.t1 (f1 int);
SHOW CREATE TABLE db1.t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci ENCRYPTION='Y'
SELECT TABLE_NAME, CREATE_OPTIONS FROM INFORMATION_SCHEMA.TABLES
WHERE TABLE_NAME='t1';
TABLE_NAME	CREATE_OPTIONS
t1	ENCRYPTION='Y'
DROP TABLE db1.t1;
DROP DATABASE db1;
SET GLOBAL table_encryption_privilege_check=false;
`````````````````````````````````````````````````````````
# CREATE TABLE with ENCRYPTION clause
# [CREATE TABLE] Case 5 )
`````````````````````````````````````````````````````````
CREATE DATABASE db1 DEFAULT ENCRYPTION='n';
SET GLOBAL table_encryption_privilege_check=false;
CREATE TABLE db1.t1 (f1 int) ENCRYPTION='y';
SHOW CREATE TABLE db1.t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci ENCRYPTION='y'
SELECT TABLE_NAME, CREATE_OPTIONS FROM INFORMATION_SCHEMA.TABLES
WHERE TABLE_NAME='t1';
TABLE_NAME	CREATE_OPTIONS
t1	ENCRYPTION='y'
DROP TABLE db1.t1;
DROP DATABASE db1;
SET GLOBAL table_encryption_privilege_check=false;
# [CREATE TABLE] Case 6 )
`````````````````````````````````````````````````````````
CREATE DATABASE db1 DEFAULT ENCRYPTION='n';
SET GLOBAL table_encryption_privilege_check=false;
CREATE TABLE db1.t1 (f1 int) ENCRYPTION='n';
SHOW CREATE TABLE db1.t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SELECT TABLE_NAME, CREATE_OPTIONS FROM INFORMATION_SCHEMA.TABLES
WHERE TABLE_NAME='t1';
TABLE_NAME	CREATE_OPTIONS
t1	
DROP TABLE db1.t1;
DROP DATABASE db1;
SET GLOBAL table_encryption_privilege_check=false;
# [CREATE TABLE] Case 7 )
`````````````````````````````````````````````````````````
CREATE DATABASE db1 DEFAULT ENCRYPTION='y';
SET GLOBAL table_encryption_privilege_check=false;
CREATE TABLE db1.t1 (f1 int) ENCRYPTION='y';
SHOW CREATE TABLE db1.t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci ENCRYPTION='y'
SELECT TABLE_NAME, CREATE_OPTIONS FROM INFORMATION_SCHEMA.TABLES
WHERE TABLE_NAME='t1';
TABLE_NAME	CREATE_OPTIONS
t1	ENCRYPTION='y'
DROP TABLE db1.t1;
DROP DATABASE db1;
SET GLOBAL table_encryption_privilege_check=false;
# [CREATE TABLE] Case 8 )
`````````````````````````````````````````````````````````
CREATE DATABASE db1 DEFAULT ENCRYPTION='y';
SET GLOBAL table_encryption_privilege_check=false;
CREATE TABLE db1.t1 (f1 int) ENCRYPTION='n';
Warnings:
Warning	3824	Creating an unencrypted table in a database with default encryption enabled.
SHOW CREATE TABLE db1.t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci ENCRYPTION='n'
SELECT TABLE_NAME, CREATE_OPTIONS FROM INFORMATION_SCHEMA.TABLES
WHERE TABLE_NAME='t1';
TABLE_NAME	CREATE_OPTIONS
t1	ENCRYPTION='n'
DROP TABLE db1.t1;
DROP DATABASE db1;
SET GLOBAL table_encryption_privilege_check=false;
`````````````````````````````````````````````````````````
# Fails as there is mismatch between table and database encryption.
# [CREATE TABLE] Case 9 )
`````````````````````````````````````````````````````````
CREATE DATABASE db1 DEFAULT ENCRYPTION='n';
SET GLOBAL table_encryption_privilege_check=true;
CREATE TABLE db1.t1 (f1 int) ENCRYPTION='y';
ERROR HY000: Table encryption differ from its database default encryption, and user doesn't have enough privilege.
# GRANT user the TABLE_ENCRYPTION_ADMIN
GRANT TABLE_ENCRYPTION_ADMIN ON *.* TO u1@localhost;
CREATE TABLE db1.t1 (f1 int) ENCRYPTION='y';
# REVOKE TABLE_ENCRYPTION_ADMIN from user.
REVOKE TABLE_ENCRYPTION_ADMIN ON *.* FROM u1@localhost;
DROP DATABASE db1;
SET GLOBAL table_encryption_privilege_check=false;
# [CREATE TABLE] Case 10 )
`````````````````````````````````````````````````````````
CREATE DATABASE db1 DEFAULT ENCRYPTION='n';
SET GLOBAL table_encryption_privilege_check=true;
CREATE TABLE db1.t1 (f1 int) ENCRYPTION='n';
SHOW CREATE TABLE db1.t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SELECT TABLE_NAME, CREATE_OPTIONS FROM INFORMATION_SCHEMA.TABLES
WHERE TABLE_NAME='t1';
TABLE_NAME	CREATE_OPTIONS
t1	
DROP TABLE db1.t1;
DROP DATABASE db1;
SET GLOBAL table_encryption_privilege_check=false;
# [CREATE TABLE] Case 11 )
`````````````````````````````````````````````````````````
CREATE DATABASE db1 DEFAULT ENCRYPTION='y';
SET GLOBAL table_encryption_privilege_check=true;
CREATE TABLE db1.t1 (f1 int) ENCRYPTION='y';
SHOW CREATE TABLE db1.t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci ENCRYPTION='y'
SELECT TABLE_NAME, CREATE_OPTIONS FROM INFORMATION_SCHEMA.TABLES
WHERE TABLE_NAME='t1';
TABLE_NAME	CREATE_OPTIONS
t1	ENCRYPTION='y'
DROP TABLE db1.t1;
DROP DATABASE db1;
SET GLOBAL table_encryption_privilege_check=false;
`````````````````````````````````````````````````````````
# Fails as there is mismatch between table and database encryption.
# [CREATE TABLE] Case 12 )
`````````````````````````````````````````````````````````
CREATE DATABASE db1 DEFAULT ENCRYPTION='y';
SET GLOBAL table_encryption_privilege_check=true;
CREATE TABLE db1.t1 (f1 int) ENCRYPTION='n';
ERROR HY000: Table encryption differ from its database default encryption, and user doesn't have enough privilege.
# GRANT user the TABLE_ENCRYPTION_ADMIN
GRANT TABLE_ENCRYPTION_ADMIN ON *.* TO u1@localhost;
CREATE TABLE db1.t1 (f1 int) ENCRYPTION='n';
# REVOKE TABLE_ENCRYPTION_ADMIN from user.
REVOKE TABLE_ENCRYPTION_ADMIN ON *.* FROM u1@localhost;
DROP DATABASE db1;
SET GLOBAL table_encryption_privilege_check=false;
`````````````````````````````````````````````````````````
# Check with invalid ENCRYPTION value
# [CREATE TABLE] Case 13 )
`````````````````````````````````````````````````````````
CREATE DATABASE db1 DEFAULT ENCRYPTION='n';
SET GLOBAL table_encryption_privilege_check=false;
CREATE TABLE db1.t1 (f1 int) ENCRYPTION='k';
ERROR HY000: Invalid encryption option.
DROP DATABASE db1;
SET GLOBAL table_encryption_privilege_check=false;
`````````````````````````````````````````````````````````
# With innodb_file_per_table=off and
# Test CREATE TABLE without ENCRYPTION clause.
SET GLOBAL innodb_file_per_table = OFF;
# [CREATE TABLE] Case 14 )
`````````````````````````````````````````````````````````
CREATE DATABASE db1 DEFAULT ENCRYPTION='n';
SET GLOBAL table_encryption_privilege_check=false;
CREATE TABLE db1.t1 (f1 int);
SHOW CREATE TABLE db1.t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SELECT TABLE_NAME, CREATE_OPTIONS FROM INFORMATION_SCHEMA.TABLES
WHERE TABLE_NAME='t1';
TABLE_NAME	CREATE_OPTIONS
t1	
DROP TABLE db1.t1;
DROP DATABASE db1;
SET GLOBAL table_encryption_privilege_check=false;
`````````````````````````````````````````````````````````
# Fails because we cannot created a encrypted table in system tablespace.
# [CREATE TABLE] Case 15 )
`````````````````````````````````````````````````````````
CREATE DATABASE db1 DEFAULT ENCRYPTION='y';
SET GLOBAL table_encryption_privilege_check=false;
CREATE TABLE db1.t1 (f1 int);
ERROR HY000: Request to create 'encrypted' table while using an 'unencrypted' tablespace.
DROP DATABASE db1;
SET GLOBAL table_encryption_privilege_check=false;
# [CREATE TABLE] Case 16 )
`````````````````````````````````````````````````````````
CREATE DATABASE db1 DEFAULT ENCRYPTION='n';
SET GLOBAL table_encryption_privilege_check=true;
CREATE TABLE db1.t1 (f1 int);
SHOW CREATE TABLE db1.t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SELECT TABLE_NAME, CREATE_OPTIONS FROM INFORMATION_SCHEMA.TABLES
WHERE TABLE_NAME='t1';
TABLE_NAME	CREATE_OPTIONS
t1	
DROP TABLE db1.t1;
DROP DATABASE db1;
SET GLOBAL table_encryption_privilege_check=false;
`````````````````````````````````````````````````````````
# Fails because we cannot created a encrypted table in system tablespace.
# [CREATE TABLE] Case 17 )
`````````````````````````````````````````````````````````
CREATE DATABASE db1 DEFAULT ENCRYPTION='y';
SET GLOBAL table_encryption_privilege_check=true;
CREATE TABLE db1.t1 (f1 int);
ERROR HY000: Request to create 'encrypted' table while using an 'unencrypted' tablespace.
DROP DATABASE db1;
SET GLOBAL table_encryption_privilege_check=false;
`````````````````````````````````````````````````````````
# With explicit ENCRYPTION clause
`````````````````````````````````````````````````````````
# Fails because we cannot created a encrypted table in system tablespace.
# [CREATE TABLE] Case 18 )
`````````````````````````````````````````````````````````
CREATE DATABASE db1 DEFAULT ENCRYPTION='n';
SET GLOBAL table_encryption_privilege_check=false;
CREATE TABLE db1.t1 (f1 int) ENCRYPTION='y';
ERROR HY000: Request to create 'encrypted' table while using an 'unencrypted' tablespace.
DROP DATABASE db1;
SET GLOBAL table_encryption_privilege_check=false;
# [CREATE TABLE] Case 19 )
`````````````````````````````````````````````````````````
CREATE DATABASE db1 DEFAULT ENCRYPTION='n';
SET GLOBAL table_encryption_privilege_check=false;
CREATE TABLE db1.t1 (f1 int) ENCRYPTION='n';
SHOW CREATE TABLE db1.t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SELECT TABLE_NAME, CREATE_OPTIONS FROM INFORMATION_SCHEMA.TABLES
WHERE TABLE_NAME='t1';
TABLE_NAME	CREATE_OPTIONS
t1	
DROP TABLE db1.t1;
DROP DATABASE db1;
SET GLOBAL table_encryption_privilege_check=false;
`````````````````````````````````````````````````````````
# Fails because we cannot created a encrypted table in system tablespace.
# [CREATE TABLE] Case 20 )
`````````````````````````````````````````````````````````
CREATE DATABASE db1 DEFAULT ENCRYPTION='y';
SET GLOBAL table_encryption_privilege_check=false;
CREATE TABLE db1.t1 (f1 int) ENCRYPTION='y';
ERROR HY000: Request to create 'encrypted' table while using an 'unencrypted' tablespace.
DROP DATABASE db1;
SET GLOBAL table_encryption_privilege_check=false;
# [CREATE TABLE] Case 21 )
`````````````````````````````````````````````````````````
CREATE DATABASE db1 DEFAULT ENCRYPTION='y';
SET GLOBAL table_encryption_privilege_check=false;
CREATE TABLE db1.t1 (f1 int) ENCRYPTION='n';
Warnings:
Warning	3824	Creating an unencrypted table in a database with default encryption enabled.
SHOW CREATE TABLE db1.t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci ENCRYPTION='n'
SELECT TABLE_NAME, CREATE_OPTIONS FROM INFORMATION_SCHEMA.TABLES
WHERE TABLE_NAME='t1';
TABLE_NAME	CREATE_OPTIONS
t1	ENCRYPTION='n'
DROP TABLE db1.t1;
DROP DATABASE db1;
SET GLOBAL table_encryption_privilege_check=false;
`````````````````````````````````````````````````````````
# Fails because we cannot created a encrypted table in system tablespace.
# [CREATE TABLE] Case 22 )
`````````````````````````````````````````````````````````
CREATE DATABASE db1 DEFAULT ENCRYPTION='n';
SET GLOBAL table_encryption_privilege_check=true;
CREATE TABLE db1.t1 (f1 int) ENCRYPTION='y';
ERROR HY000: Request to create 'encrypted' table while using an 'unencrypted' tablespace.
DROP DATABASE db1;
SET GLOBAL table_encryption_privilege_check=false;
# [CREATE TABLE] Case 23 )
`````````````````````````````````````````````````````````
CREATE DATABASE db1 DEFAULT ENCRYPTION='n';
SET GLOBAL table_encryption_privilege_check=true;
CREATE TABLE db1.t1 (f1 int) ENCRYPTION='n';
SHOW CREATE TABLE db1.t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SELECT TABLE_NAME, CREATE_OPTIONS FROM INFORMATION_SCHEMA.TABLES
WHERE TABLE_NAME='t1';
TABLE_NAME	CREATE_OPTIONS
t1	
DROP TABLE db1.t1;
DROP DATABASE db1;
SET GLOBAL table_encryption_privilege_check=false;
`````````````````````````````````````````````````````````
# Fails because we cannot created a encrypted table in system tablespace.
# [CREATE TABLE] Case 24 )
`````````````````````````````````````````````````````````
CREATE DATABASE db1 DEFAULT ENCRYPTION='y';
SET GLOBAL table_encryption_privilege_check=true;
CREATE TABLE db1.t1 (f1 int) ENCRYPTION='y';
ERROR HY000: Request to create 'encrypted' table while using an 'unencrypted' tablespace.
DROP DATABASE db1;
SET GLOBAL table_encryption_privilege_check=false;
# [CREATE TABLE] Case 25 )
`````````````````````````````````````````````````````````
CREATE DATABASE db1 DEFAULT ENCRYPTION='y';
SET GLOBAL table_encryption_privilege_check=true;
CREATE TABLE db1.t1 (f1 int) ENCRYPTION='n';
ERROR HY000: Table encryption differ from its database default encryption, and user doesn't have enough privilege.
# GRANT user the TABLE_ENCRYPTION_ADMIN
GRANT TABLE_ENCRYPTION_ADMIN ON *.* TO u1@localhost;
CREATE TABLE db1.t1 (f1 int) ENCRYPTION='n';
# REVOKE TABLE_ENCRYPTION_ADMIN from user.
REVOKE TABLE_ENCRYPTION_ADMIN ON *.* FROM u1@localhost;
DROP DATABASE db1;
SET GLOBAL table_encryption_privilege_check=false;
`````````````````````````````````````````````````````````
# With innodb_file_per_table = ON.
# Test CREATE TABLE with explicit TABLESPACE=innodb_file_per_table
SET GLOBAL innodb_file_per_table = ON;
`````````````````````````````````````````````````````````
# Without ENCRYPTION clause
# [CREATE TABLE] Case 26 )
`````````````````````````````````````````````````````````
CREATE DATABASE db1 DEFAULT ENCRYPTION='n';
SET GLOBAL table_encryption_privilege_check=false;
CREATE TABLE db1.t1 (f1 int) TABLESPACE=innodb_file_per_table;
SHOW CREATE TABLE db1.t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `innodb_file_per_table` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SELECT TABLE_NAME, CREATE_OPTIONS FROM INFORMATION_SCHEMA.TABLES
WHERE TABLE_NAME='t1';
TABLE_NAME	CREATE_OPTIONS
t1	
DROP TABLE db1.t1;
DROP DATABASE db1;
SET GLOBAL table_encryption_privilege_check=false;
# [CREATE TABLE] Case 27 )
`````````````````````````````````````````````````````````
CREATE DATABASE db1 DEFAULT ENCRYPTION='y';
SET GLOBAL table_encryption_privilege_check=false;
CREATE TABLE db1.t1 (f1 int) TABLESPACE=innodb_file_per_table;
SHOW CREATE TABLE db1.t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `innodb_file_per_table` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci ENCRYPTION='Y'
SELECT TABLE_NAME, CREATE_OPTIONS FROM INFORMATION_SCHEMA.TABLES
WHERE TABLE_NAME='t1';
TABLE_NAME	CREATE_OPTIONS
t1	ENCRYPTION='Y'
DROP TABLE db1.t1;
DROP DATABASE db1;
SET GLOBAL table_encryption_privilege_check=false;
# [CREATE TABLE] Case 28 )
`````````````````````````````````````````````````````````
CREATE DATABASE db1 DEFAULT ENCRYPTION='n';
SET GLOBAL table_encryption_privilege_check=true;
CREATE TABLE db1.t1 (f1 int) TABLESPACE=innodb_file_per_table;
SHOW CREATE TABLE db1.t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `innodb_file_per_table` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SELECT TABLE_NAME, CREATE_OPTIONS FROM INFORMATION_SCHEMA.TABLES
WHERE TABLE_NAME='t1';
TABLE_NAME	CREATE_OPTIONS
t1	
DROP TABLE db1.t1;
DROP DATABASE db1;
SET GLOBAL table_encryption_privilege_check=false;
# [CREATE TABLE] Case 29 )
`````````````````````````````````````````````````````````
CREATE DATABASE db1 DEFAULT ENCRYPTION='y';
SET GLOBAL table_encryption_privilege_check=true;
CREATE TABLE db1.t1 (f1 int) TABLESPACE=innodb_file_per_table;
SHOW CREATE TABLE db1.t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `innodb_file_per_table` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci ENCRYPTION='Y'
SELECT TABLE_NAME, CREATE_OPTIONS FROM INFORMATION_SCHEMA.TABLES
WHERE TABLE_NAME='t1';
TABLE_NAME	CREATE_OPTIONS
t1	ENCRYPTION='Y'
DROP TABLE db1.t1;
DROP DATABASE db1;
SET GLOBAL table_encryption_privilege_check=false;
`````````````````````````````````````````````````````````
# With ENCRYPTION clause
# [CREATE TABLE] Case 30 )
`````````````````````````````````````````````````````````
CREATE DATABASE db1 DEFAULT ENCRYPTION='n';
SET GLOBAL table_encryption_privilege_check=false;
CREATE TABLE db1.t1 (f1 int) TABLESPACE=innodb_file_per_table ENCRYPTION='y';
SHOW CREATE TABLE db1.t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `innodb_file_per_table` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci ENCRYPTION='y'
SELECT TABLE_NAME, CREATE_OPTIONS FROM INFORMATION_SCHEMA.TABLES
WHERE TABLE_NAME='t1';
TABLE_NAME	CREATE_OPTIONS
t1	ENCRYPTION='y'
DROP TABLE db1.t1;
DROP DATABASE db1;
SET GLOBAL table_encryption_privilege_check=false;
# [CREATE TABLE] Case 31 )
`````````````````````````````````````````````````````````
CREATE DATABASE db1 DEFAULT ENCRYPTION='n';
SET GLOBAL table_encryption_privilege_check=false;
CREATE TABLE db1.t1 (f1 int) TABLESPACE=innodb_file_per_table ENCRYPTION='n';
SHOW CREATE TABLE db1.t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `innodb_file_per_table` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SELECT TABLE_NAME, CREATE_OPTIONS FROM INFORMATION_SCHEMA.TABLES
WHERE TABLE_NAME='t1';
TABLE_NAME	CREATE_OPTIONS
t1	
DROP TABLE db1.t1;
DROP DATABASE db1;
SET GLOBAL table_encryption_privilege_check=false;
# [CREATE TABLE] Case 32 )
`````````````````````````````````````````````````````````
CREATE DATABASE db1 DEFAULT ENCRYPTION='y';
SET GLOBAL table_encryption_privilege_check=false;
CREATE TABLE db1.t1 (f1 int) TABLESPACE=innodb_file_per_table ENCRYPTION='y';
SHOW CREATE TABLE db1.t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `innodb_file_per_table` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci ENCRYPTION='y'
SELECT TABLE_NAME, CREATE_OPTIONS FROM INFORMATION_SCHEMA.TABLES
WHERE TABLE_NAME='t1';
TABLE_NAME	CREATE_OPTIONS
t1	ENCRYPTION='y'
DROP TABLE db1.t1;
DROP DATABASE db1;
SET GLOBAL table_encryption_privilege_check=false;
# [CREATE TABLE] Case 33 )
`````````````````````````````````````````````````````````
CREATE DATABASE db1 DEFAULT ENCRYPTION='y';
SET GLOBAL table_encryption_privilege_check=false;
CREATE TABLE db1.t1 (f1 int) TABLESPACE=innodb_file_per_table ENCRYPTION='n';
Warnings:
Warning	3824	Creating an unencrypted table in a database with default encryption enabled.
SHOW CREATE TABLE db1.t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `innodb_file_per_table` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci ENCRYPTION='n'
SELECT TABLE_NAME, CREATE_OPTIONS FROM INFORMATION_SCHEMA.TABLES
WHERE TABLE_NAME='t1';
TABLE_NAME	CREATE_OPTIONS
t1	ENCRYPTION='n'
DROP TABLE db1.t1;
DROP DATABASE db1;
SET GLOBAL table_encryption_privilege_check=false;
`````````````````````````````````````````````````````````
# Fails as there is mismatch between table and database encryption.
# [CREATE TABLE] Case 34 )
`````````````````````````````````````````````````````````
CREATE DATABASE db1 DEFAULT ENCRYPTION='n';
SET GLOBAL table_encryption_privilege_check=true;
CREATE TABLE db1.t1 (f1 int) TABLESPACE=innodb_file_per_table ENCRYPTION='y';
ERROR HY000: Table encryption differ from its database default encryption, and user doesn't have enough privilege.
# GRANT user the TABLE_ENCRYPTION_ADMIN
GRANT TABLE_ENCRYPTION_ADMIN ON *.* TO u1@localhost;
CREATE TABLE db1.t1 (f1 int) TABLESPACE=innodb_file_per_table ENCRYPTION='y';
# REVOKE TABLE_ENCRYPTION_ADMIN from user.
REVOKE TABLE_ENCRYPTION_ADMIN ON *.* FROM u1@localhost;
DROP DATABASE db1;
SET GLOBAL table_encryption_privilege_check=false;
# [CREATE TABLE] Case 35 )
`````````````````````````````````````````````````````````
CREATE DATABASE db1 DEFAULT ENCRYPTION='n';
SET GLOBAL table_encryption_privilege_check=true;
CREATE TABLE db1.t1 (f1 int) TABLESPACE=innodb_file_per_table ENCRYPTION='n';
SHOW CREATE TABLE db1.t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `innodb_file_per_table` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SELECT TABLE_NAME, CREATE_OPTIONS FROM INFORMATION_SCHEMA.TABLES
WHERE TABLE_NAME='t1';
TABLE_NAME	CREATE_OPTIONS
t1	
DROP TABLE db1.t1;
DROP DATABASE db1;
SET GLOBAL table_encryption_privilege_check=false;
# [CREATE TABLE] Case 36 )
`````````````````````````````````````````````````````````
CREATE DATABASE db1 DEFAULT ENCRYPTION='y';
SET GLOBAL table_encryption_privilege_check=true;
CREATE TABLE db1.t1 (f1 int) TABLESPACE=innodb_file_per_table ENCRYPTION='y';
SHOW CREATE TABLE db1.t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `innodb_file_per_table` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci ENCRYPTION='y'
SELECT TABLE_NAME, CREATE_OPTIONS FROM INFORMATION_SCHEMA.TABLES
WHERE TABLE_NAME='t1';
TABLE_NAME	CREATE_OPTIONS
t1	ENCRYPTION='y'
DROP TABLE db1.t1;
DROP DATABASE db1;
SET GLOBAL table_encryption_privilege_check=false;
`````````````````````````````````````````````````````````
# Fails as there is mismatch between table and database encryption.
# [CREATE TABLE] Case 37 )
`````````````````````````````````````````````````````````
CREATE DATABASE db1 DEFAULT ENCRYPTION='y';
SET GLOBAL table_encryption_privilege_check=true;
CREATE TABLE db1.t1 (f1 int) TABLESPACE=innodb_file_per_table ENCRYPTION='n';
ERROR HY000: Table encryption differ from its database default encryption, and user doesn't have enough privilege.
# GRANT user the TABLE_ENCRYPTION_ADMIN
GRANT TABLE_ENCRYPTION_ADMIN ON *.* TO u1@localhost;
CREATE TABLE db1.t1 (f1 int) TABLESPACE=innodb_file_per_table ENCRYPTION='n';
# REVOKE TABLE_ENCRYPTION_ADMIN from user.
REVOKE TABLE_ENCRYPTION_ADMIN ON *.* FROM u1@localhost;
DROP DATABASE db1;
SET GLOBAL table_encryption_privilege_check=false;
`````````````````````````````````````````````````````````
# Test CREATE TABLE with TABLESPACE=innodb_system
`````````````````````````````````````````````````````````
# Without ENCRYPTION clause
# [CREATE TABLE] Case 38 )
`````````````````````````````````````````````````````````
CREATE DATABASE db1 DEFAULT ENCRYPTION='n';
SET GLOBAL table_encryption_privilege_check=false;
CREATE TABLE db1.t1 (f1 int) TABLESPACE=innodb_system;
SHOW CREATE TABLE db1.t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `innodb_system` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SELECT TABLE_NAME, CREATE_OPTIONS FROM INFORMATION_SCHEMA.TABLES
WHERE TABLE_NAME='t1';
TABLE_NAME	CREATE_OPTIONS
t1	
DROP TABLE db1.t1;
DROP DATABASE db1;
SET GLOBAL table_encryption_privilege_check=false;
`````````````````````````````````````````````````````````
# Fails because we cannot created a encrypted table in system tablespace.
# [CREATE TABLE] Case 39 )
`````````````````````````````````````````````````````````
CREATE DATABASE db1 DEFAULT ENCRYPTION='y';
SET GLOBAL table_encryption_privilege_check=false;
CREATE TABLE db1.t1 (f1 int) TABLESPACE=innodb_system;
ERROR HY000: Request to create 'encrypted' table while using an 'unencrypted' tablespace.
DROP DATABASE db1;
SET GLOBAL table_encryption_privilege_check=false;
# [CREATE TABLE] Case 40 )
`````````````````````````````````````````````````````````
CREATE DATABASE db1 DEFAULT ENCRYPTION='n';
SET GLOBAL table_encryption_privilege_check=true;
CREATE TABLE db1.t1 (f1 int) TABLESPACE=innodb_system;
SHOW CREATE TABLE db1.t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `innodb_system` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SELECT TABLE_NAME, CREATE_OPTIONS FROM INFORMATION_SCHEMA.TABLES
WHERE TABLE_NAME='t1';
TABLE_NAME	CREATE_OPTIONS
t1	
DROP TABLE db1.t1;
DROP DATABASE db1;
SET GLOBAL table_encryption_privilege_check=false;
`````````````````````````````````````````````````````````
# Fails because we cannot created a encrypted table in system tablespace.
# [CREATE TABLE] Case 41 )
`````````````````````````````````````````````````````````
CREATE DATABASE db1 DEFAULT ENCRYPTION='y';
SET GLOBAL table_encryption_privilege_check=true;
CREATE TABLE db1.t1 (f1 int) TABLESPACE=innodb_system;
ERROR HY000: Request to create 'encrypted' table while using an 'unencrypted' tablespace.
DROP DATABASE db1;
SET GLOBAL table_encryption_privilege_check=false;
`````````````````````````````````````````````````````````
# With ENCRYPTION clause
`````````````````````````````````````````````````````````
# Fails because we cannot created a encrypted table in system tablespace.
# [CREATE TABLE] Case 42 )
`````````````````````````````````````````````````````````
CREATE DATABASE db1 DEFAULT ENCRYPTION='n';
SET GLOBAL table_encryption_privilege_check=false;
CREATE TABLE db1.t1 (f1 int) TABLESPACE=innodb_system ENCRYPTION='y';
ERROR HY000: Request to create 'encrypted' table while using an 'unencrypted' tablespace.
DROP DATABASE db1;
SET GLOBAL table_encryption_privilege_check=false;
# [CREATE TABLE] Case 43 )
`````````````````````````````````````````````````````````
CREATE DATABASE db1 DEFAULT ENCRYPTION='n';
SET GLOBAL table_encryption_privilege_check=false;
CREATE TABLE db1.t1 (f1 int) TABLESPACE=innodb_system ENCRYPTION='n';
SHOW CREATE TABLE db1.t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `innodb_system` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SELECT TABLE_NAME, CREATE_OPTIONS FROM INFORMATION_SCHEMA.TABLES
WHERE TABLE_NAME='t1';
TABLE_NAME	CREATE_OPTIONS
t1	
DROP TABLE db1.t1;
DROP DATABASE db1;
SET GLOBAL table_encryption_privilege_check=false;
`````````````````````````````````````````````````````````
# Fails because we cannot created a encrypted table in system tablespace.
# [CREATE TABLE] Case 44 )
`````````````````````````````````````````````````````````
CREATE DATABASE db1 DEFAULT ENCRYPTION='y';
SET GLOBAL table_encryption_privilege_check=false;
CREATE TABLE db1.t1 (f1 int) TABLESPACE=innodb_system ENCRYPTION='y';
ERROR HY000: Request to create 'encrypted' table while using an 'unencrypted' tablespace.
DROP DATABASE db1;
SET GLOBAL table_encryption_privilege_check=false;
# [CREATE TABLE] Case 45 )
`````````````````````````````````````````````````````````
CREATE DATABASE db1 DEFAULT ENCRYPTION='y';
SET GLOBAL table_encryption_privilege_check=false;
CREATE TABLE db1.t1 (f1 int) TABLESPACE=innodb_system ENCRYPTION='n';
Warnings:
Warning	3824	Creating an unencrypted table in a database with default encryption enabled.
SHOW CREATE TABLE db1.t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `innodb_system` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci /*!80016 ENCRYPTION='n' */
SELECT TABLE_NAME, CREATE_OPTIONS FROM INFORMATION_SCHEMA.TABLES
WHERE TABLE_NAME='t1';
TABLE_NAME	CREATE_OPTIONS
t1	ENCRYPTION='n'
DROP TABLE db1.t1;
DROP DATABASE db1;
SET GLOBAL table_encryption_privilege_check=false;
`````````````````````````````````````````````````````````
# Fails as there is mismatch between table and database encryption.
# The statement would fail even with user owning TABLE_ENCRYPTION_ADMIN
# with ER_ILLEGAL_HA_CREATE_OPTION because one cannot create
# encrypted table in system tablespace.
# [CREATE TABLE] Case 46 )
`````````````````````````````````````````````````````````
CREATE DATABASE db1 DEFAULT ENCRYPTION='n';
SET GLOBAL table_encryption_privilege_check=true;
CREATE TABLE db1.t1 (f1 int) TABLESPACE=innodb_system ENCRYPTION='y';
ERROR HY000: Request to create 'encrypted' table while using an 'unencrypted' tablespace.
DROP DATABASE db1;
SET GLOBAL table_encryption_privilege_check=false;
# [CREATE TABLE] Case 47 )
`````````````````````````````````````````````````````````
CREATE DATABASE db1 DEFAULT ENCRYPTION='n';
SET GLOBAL table_encryption_privilege_check=true;
CREATE TABLE db1.t1 (f1 int) TABLESPACE=innodb_system ENCRYPTION='n';
SHOW CREATE TABLE db1.t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `innodb_system` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SELECT TABLE_NAME, CREATE_OPTIONS FROM INFORMATION_SCHEMA.TABLES
WHERE TABLE_NAME='t1';
TABLE_NAME	CREATE_OPTIONS
t1	
DROP TABLE db1.t1;
DROP DATABASE db1;
SET GLOBAL table_encryption_privilege_check=false;
`````````````````````````````````````````````````````````
# Fails because we cannot created a encrypted table in system tablespace.
# [CREATE TABLE] Case 48 )
`````````````````````````````````````````````````````````
CREATE DATABASE db1 DEFAULT ENCRYPTION='y';
SET GLOBAL table_encryption_privilege_check=true;
CREATE TABLE db1.t1 (f1 int) TABLESPACE=innodb_system ENCRYPTION='y';
ERROR HY000: Request to create 'encrypted' table while using an 'unencrypted' tablespace.
DROP DATABASE db1;
SET GLOBAL table_encryption_privilege_check=false;
`````````````````````````````````````````````````````````
# Fails as there is mismatch between table and database encryption.
# [CREATE TABLE] Case 49 )
`````````````````````````````````````````````````````````
CREATE DATABASE db1 DEFAULT ENCRYPTION='y';
SET GLOBAL table_encryption_privilege_check=true;
CREATE TABLE db1.t1 (f1 int) TABLESPACE=innodb_system ENCRYPTION='n';
ERROR HY000: Table encryption differ from its database default encryption, and user doesn't have enough privilege.
# GRANT user the TABLE_ENCRYPTION_ADMIN
GRANT TABLE_ENCRYPTION_ADMIN ON *.* TO u1@localhost;
CREATE TABLE db1.t1 (f1 int) TABLESPACE=innodb_system ENCRYPTION='n';
# REVOKE TABLE_ENCRYPTION_ADMIN from user.
REVOKE TABLE_ENCRYPTION_ADMIN ON *.* FROM u1@localhost;
DROP DATABASE db1;
SET GLOBAL table_encryption_privilege_check=false;
`````````````````````````````````````````````````````````
# Test CREATE TABLE with general TABLESPACE
`````````````````````````````````````````````````````````
# With unencrypted general tablespace and without ENCRYPTION clause.
# [CREATE TABLE] Case 50 )
`````````````````````````````````````````````````````````
CREATE TABLESPACE ts1 ADD DATAFILE 'ts1.ibd' ENCRYPTION='n';
CREATE DATABASE db1 DEFAULT ENCRYPTION='n';
SET GLOBAL table_encryption_privilege_check=false;
CREATE TABLE db1.t1 (f1 int) TABLESPACE=ts1;
SHOW CREATE TABLE db1.t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `ts1` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SELECT TABLE_NAME, CREATE_OPTIONS FROM INFORMATION_SCHEMA.TABLES
WHERE TABLE_NAME='t1';
TABLE_NAME	CREATE_OPTIONS
t1	
DROP TABLE db1.t1;
DROP DATABASE db1;
DROP TABLESPACE ts1;
SET GLOBAL table_encryption_privilege_check=false;
`````````````````````````````````````````````````````````
# Fails as there is mismatch between table and database encryption.
# The table ENCRYPTION clause is inherited from database and this
# does not match tablespace encryption type.
# [CREATE TABLE] Case 51 )
`````````````````````````````````````````````````````````
CREATE TABLESPACE ts1 ADD DATAFILE 'ts1.ibd' ENCRYPTION='n';
CREATE DATABASE db1 DEFAULT ENCRYPTION='y';
SET GLOBAL table_encryption_privilege_check=false;
CREATE TABLE db1.t1 (f1 int) TABLESPACE=ts1;
ERROR HY000: Request to create 'encrypted' table while using an 'unencrypted' tablespace.
DROP DATABASE db1;
DROP TABLESPACE ts1;
SET GLOBAL table_encryption_privilege_check=false;
# [CREATE TABLE] Case 52 )
`````````````````````````````````````````````````````````
CREATE TABLESPACE ts1 ADD DATAFILE 'ts1.ibd' ENCRYPTION='n';
CREATE DATABASE db1 DEFAULT ENCRYPTION='n';
SET GLOBAL table_encryption_privilege_check=true;
CREATE TABLE db1.t1 (f1 int) TABLESPACE=ts1;
SHOW CREATE TABLE db1.t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `ts1` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SELECT TABLE_NAME, CREATE_OPTIONS FROM INFORMATION_SCHEMA.TABLES
WHERE TABLE_NAME='t1';
TABLE_NAME	CREATE_OPTIONS
t1	
DROP TABLE db1.t1;
DROP DATABASE db1;
DROP TABLESPACE ts1;
SET GLOBAL table_encryption_privilege_check=false;
`````````````````````````````````````````````````````````
# Fails as there is mismatch between table and database encryption.
# The table ENCRYPTION clause is inherited from database and this
# does not match tablespace encryption type.
# [CREATE TABLE] Case 53 )
`````````````````````````````````````````````````````````
CREATE TABLESPACE ts1 ADD DATAFILE 'ts1.ibd' ENCRYPTION='n';
CREATE DATABASE db1 DEFAULT ENCRYPTION='y';
SET GLOBAL table_encryption_privilege_check=true;
CREATE TABLE db1.t1 (f1 int) TABLESPACE=ts1;
ERROR HY000: Request to create 'encrypted' table while using an 'unencrypted' tablespace.
DROP DATABASE db1;
DROP TABLESPACE ts1;
SET GLOBAL table_encryption_privilege_check=false;
`````````````````````````````````````````````````````````
# With ENCRYPTION clause
# [CREATE TABLE] Case 54 )
`````````````````````````````````````````````````````````
CREATE TABLESPACE ts1 ADD DATAFILE 'ts1.ibd' ENCRYPTION='y';
CREATE DATABASE db1 DEFAULT ENCRYPTION='n';
SET GLOBAL table_encryption_privilege_check=false;
CREATE TABLE db1.t1 (f1 int) TABLESPACE=ts1 ENCRYPTION='y';
SHOW CREATE TABLE db1.t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `ts1` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci /*!80016 ENCRYPTION='y' */
SELECT TABLE_NAME, CREATE_OPTIONS FROM INFORMATION_SCHEMA.TABLES
WHERE TABLE_NAME='t1';
TABLE_NAME	CREATE_OPTIONS
t1	ENCRYPTION='y'
DROP TABLE db1.t1;
DROP DATABASE db1;
DROP TABLESPACE ts1;
SET GLOBAL table_encryption_privilege_check=false;
# [CREATE TABLE] Case 55 )
`````````````````````````````````````````````````````````
CREATE TABLESPACE ts1 ADD DATAFILE 'ts1.ibd' ENCRYPTION='n';
CREATE DATABASE db1 DEFAULT ENCRYPTION='n';
SET GLOBAL table_encryption_privilege_check=false;
CREATE TABLE db1.t1 (f1 int) TABLESPACE=ts1 ENCRYPTION='n';
SHOW CREATE TABLE db1.t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `ts1` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SELECT TABLE_NAME, CREATE_OPTIONS FROM INFORMATION_SCHEMA.TABLES
WHERE TABLE_NAME='t1';
TABLE_NAME	CREATE_OPTIONS
t1	
DROP TABLE db1.t1;
DROP DATABASE db1;
DROP TABLESPACE ts1;
SET GLOBAL table_encryption_privilege_check=false;
# [CREATE TABLE] Case 56 )
`````````````````````````````````````````````````````````
CREATE TABLESPACE ts1 ADD DATAFILE 'ts1.ibd' ENCRYPTION='y';
CREATE DATABASE db1 DEFAULT ENCRYPTION='y';
SET GLOBAL table_encryption_privilege_check=false;
CREATE TABLE db1.t1 (f1 int) TABLESPACE=ts1 ENCRYPTION='y';
SHOW CREATE TABLE db1.t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `ts1` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci /*!80016 ENCRYPTION='y' */
SELECT TABLE_NAME, CREATE_OPTIONS FROM INFORMATION_SCHEMA.TABLES
WHERE TABLE_NAME='t1';
TABLE_NAME	CREATE_OPTIONS
t1	ENCRYPTION='y'
DROP TABLE db1.t1;
DROP DATABASE db1;
DROP TABLESPACE ts1;
SET GLOBAL table_encryption_privilege_check=false;
# [CREATE TABLE] Case 57 )
`````````````````````````````````````````````````````````
CREATE TABLESPACE ts1 ADD DATAFILE 'ts1.ibd' ENCRYPTION='n';
CREATE DATABASE db1 DEFAULT ENCRYPTION='y';
SET GLOBAL table_encryption_privilege_check=false;
CREATE TABLE db1.t1 (f1 int) TABLESPACE=ts1 ENCRYPTION='n';
Warnings:
Warning	3824	Creating an unencrypted table in a database with default encryption enabled.
SHOW CREATE TABLE db1.t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `ts1` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci /*!80016 ENCRYPTION='n' */
SELECT TABLE_NAME, CREATE_OPTIONS FROM INFORMATION_SCHEMA.TABLES
WHERE TABLE_NAME='t1';
TABLE_NAME	CREATE_OPTIONS
t1	ENCRYPTION='n'
DROP TABLE db1.t1;
DROP DATABASE db1;
DROP TABLESPACE ts1;
SET GLOBAL table_encryption_privilege_check=false;
`````````````````````````````````````````````````````````
# Fails as there is mismatch between table and database encryption.
# [CREATE TABLE] Case 58 )
`````````````````````````````````````````````````````````
CREATE TABLESPACE ts1 ADD DATAFILE 'ts1.ibd' ENCRYPTION='y';
CREATE DATABASE db1 DEFAULT ENCRYPTION='n';
SET GLOBAL table_encryption_privilege_check=true;
CREATE TABLE db1.t1 (f1 int) TABLESPACE=ts1 ENCRYPTION='y';
ERROR HY000: Table encryption differ from its database default encryption, and user doesn't have enough privilege.
# GRANT user the TABLE_ENCRYPTION_ADMIN
GRANT TABLE_ENCRYPTION_ADMIN ON *.* TO u1@localhost;
CREATE TABLE db1.t1 (f1 int) TABLESPACE=ts1 ENCRYPTION='y';
# REVOKE TABLE_ENCRYPTION_ADMIN from user.
REVOKE TABLE_ENCRYPTION_ADMIN ON *.* FROM u1@localhost;
DROP DATABASE db1;
DROP TABLESPACE ts1;
SET GLOBAL table_encryption_privilege_check=false;
# [CREATE TABLE] Case 59 )
`````````````````````````````````````````````````````````
CREATE TABLESPACE ts1 ADD DATAFILE 'ts1.ibd' ENCRYPTION='n';
CREATE DATABASE db1 DEFAULT ENCRYPTION='n';
SET GLOBAL table_encryption_privilege_check=true;
CREATE TABLE db1.t1 (f1 int) TABLESPACE=ts1 ENCRYPTION='n';
SHOW CREATE TABLE db1.t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `ts1` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SELECT TABLE_NAME, CREATE_OPTIONS FROM INFORMATION_SCHEMA.TABLES
WHERE TABLE_NAME='t1';
TABLE_NAME	CREATE_OPTIONS
t1	
DROP TABLE db1.t1;
DROP DATABASE db1;
DROP TABLESPACE ts1;
SET GLOBAL table_encryption_privilege_check=false;
# [CREATE TABLE] Case 60 )
`````````````````````````````````````````````````````````
CREATE TABLESPACE ts1 ADD DATAFILE 'ts1.ibd' ENCRYPTION='y';
CREATE DATABASE db1 DEFAULT ENCRYPTION='y';
SET GLOBAL table_encryption_privilege_check=true;
CREATE TABLE db1.t1 (f1 int) TABLESPACE=ts1 ENCRYPTION='y';
SHOW CREATE TABLE db1.t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int DEFAULT NULL
) /*!50100 TABLESPACE `ts1` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci /*!80016 ENCRYPTION='y' */
SELECT TABLE_NAME, CREATE_OPTIONS FROM INFORMATION_SCHEMA.TABLES
WHERE TABLE_NAME='t1';
TABLE_NAME	CREATE_OPTIONS
t1	ENCRYPTION='y'
DROP TABLE db1.t1;
DROP DATABASE db1;
DROP TABLESPACE ts1;
SET GLOBAL table_encryption_privilege_check=false;
`````````````````````````````````````````````````````````
# Fails as there is mismatch between table and database encryption.
# [CREATE TABLE] Case 61 )
`````````````````````````````````````````````````````````
CREATE TABLESPACE ts1 ADD DATAFILE 'ts1.ibd' ENCRYPTION='n';
CREATE DATABASE db1 DEFAULT ENCRYPTION='y';
SET GLOBAL table_encryption_privilege_check=true;
CREATE TABLE db1.t1 (f1 int) TABLESPACE=ts1 ENCRYPTION='n';
ERROR HY000: Table encryption differ from its database default encryption, and user doesn't have enough privilege.
# GRANT user the TABLE_ENCRYPTION_ADMIN
GRANT TABLE_ENCRYPTION_ADMIN ON *.* TO u1@localhost;
CREATE TABLE db1.t1 (f1 int) TABLESPACE=ts1 ENCRYPTION='n';
# REVOKE TABLE_ENCRYPTION_ADMIN from user.
REVOKE TABLE_ENCRYPTION_ADMIN ON *.* FROM u1@localhost;
DROP DATABASE db1;
DROP TABLESPACE ts1;
SET GLOBAL table_encryption_privilege_check=false;
# Cleanup
DROP USER u1@localhost;
SET GLOBAL debug= '-d,skip_table_encryption_admin_check_for_set';
# ----------------------------------------------------------------------
# Teardown
# Removing manifest file for current MySQL server instance
# Removing local keyring file for keyring component: component_keyring_file
# Removing local configuration file for keyring component: component_keyring_file
# Restarting server without the manifest file
# ----------------------------------------------------------------------
