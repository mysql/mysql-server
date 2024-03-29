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
# This test run CREATE/ALTER TABLESPACE in different configurations,
#
# - Setting table_encryption_privilege_check to true/false.
# - Setting default_table_encryption to true/false.
# - With and without ENCRYPTION clause.
# - With and without user holding TABLE_ENCRYPTION_ADMIN privilege.
# - Test INFORMATION_SCHEMA.INNODB_TABLESPACES
# - Check for warnings generated.
#
`````````````````````````````````````````````````````````
# CREATE TABLESPACE with explicit ENCRYPTION clause
# table_encryption_privilege_check=false
# [CREATE TABLESPACE] Case 1 )
`````````````````````````````````````````````````````````
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=false;
CREATE TABLESPACE ts1 ADD DATAFILE 'ts1.ibd';
SHOW WARNINGS;
Level	Code	Message
SELECT NAME, ENCRYPTION FROM INFORMATION_SCHEMA.INNODB_TABLESPACES
WHERE NAME LIKE 'ts1';
NAME	ENCRYPTION
ts1	N
DROP TABLESPACE ts1;
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=false;
# [CREATE TABLESPACE] Case 2 )
`````````````````````````````````````````````````````````
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=true;
CREATE TABLESPACE ts1 ADD DATAFILE 'ts1.ibd';
SHOW WARNINGS;
Level	Code	Message
SELECT NAME, ENCRYPTION FROM INFORMATION_SCHEMA.INNODB_TABLESPACES
WHERE NAME LIKE 'ts1';
NAME	ENCRYPTION
ts1	Y
DROP TABLESPACE ts1;
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=false;
`````````````````````````````````````````````````````````
# table_encryption_privilege_check=true
# [CREATE TABLESPACE] Case 3 )
`````````````````````````````````````````````````````````
SET GLOBAL table_encryption_privilege_check=true;
SET SESSION default_table_encryption=false;
CREATE TABLESPACE ts1 ADD DATAFILE 'ts1.ibd';
SHOW WARNINGS;
Level	Code	Message
SELECT NAME, ENCRYPTION FROM INFORMATION_SCHEMA.INNODB_TABLESPACES
WHERE NAME LIKE 'ts1';
NAME	ENCRYPTION
ts1	N
DROP TABLESPACE ts1;
# Check that we never fail with TABLE_ENCRYPTION_ADMIN
# Grant user with TABLE_ENCRYPTION_ADMIN
GRANT TABLE_ENCRYPTION_ADMIN ON *.* TO u1@localhost;
CREATE TABLESPACE ts1 ADD DATAFILE 'ts1.ibd';
SHOW WARNINGS;
Level	Code	Message
SELECT NAME, ENCRYPTION FROM INFORMATION_SCHEMA.INNODB_TABLESPACES
WHERE NAME LIKE 'ts1';
NAME	ENCRYPTION
ts1	N
DROP TABLESPACE ts1;
# Revoke TABLE_ENCRYPTION_ADMIN from user.
REVOKE TABLE_ENCRYPTION_ADMIN ON *.* FROM u1@localhost;
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=false;
# [CREATE TABLESPACE] Case 4 )
`````````````````````````````````````````````````````````
SET GLOBAL table_encryption_privilege_check=true;
SET SESSION default_table_encryption=true;
CREATE TABLESPACE ts1 ADD DATAFILE 'ts1.ibd';
SHOW WARNINGS;
Level	Code	Message
SELECT NAME, ENCRYPTION FROM INFORMATION_SCHEMA.INNODB_TABLESPACES
WHERE NAME LIKE 'ts1';
NAME	ENCRYPTION
ts1	Y
DROP TABLESPACE ts1;
# Check that we never fail with TABLE_ENCRYPTION_ADMIN
# Grant user with TABLE_ENCRYPTION_ADMIN
GRANT TABLE_ENCRYPTION_ADMIN ON *.* TO u1@localhost;
CREATE TABLESPACE ts1 ADD DATAFILE 'ts1.ibd';
SHOW WARNINGS;
Level	Code	Message
SELECT NAME, ENCRYPTION FROM INFORMATION_SCHEMA.INNODB_TABLESPACES
WHERE NAME LIKE 'ts1';
NAME	ENCRYPTION
ts1	Y
DROP TABLESPACE ts1;
# Revoke TABLE_ENCRYPTION_ADMIN from user.
REVOKE TABLE_ENCRYPTION_ADMIN ON *.* FROM u1@localhost;
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=false;
`````````````````````````````````````````````````````````
# CREATE TABLESPACE with explicit ENCRYPTION clause
# table_encryption_privilege_check=false
# [CREATE TABLESPACE] Case 5 )
`````````````````````````````````````````````````````````
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=false;
CREATE TABLESPACE ts1 ADD DATAFILE 'ts1.ibd' ENCRYPTION='y';
SHOW WARNINGS;
Level	Code	Message
SELECT NAME, ENCRYPTION FROM INFORMATION_SCHEMA.INNODB_TABLESPACES
WHERE NAME LIKE 'ts1';
NAME	ENCRYPTION
ts1	Y
DROP TABLESPACE ts1;
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=false;
# [CREATE TABLESPACE] Case 6 )
`````````````````````````````````````````````````````````
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=false;
CREATE TABLESPACE ts1 ADD DATAFILE 'ts1.ibd' ENCRYPTION='n';
SHOW WARNINGS;
Level	Code	Message
SELECT NAME, ENCRYPTION FROM INFORMATION_SCHEMA.INNODB_TABLESPACES
WHERE NAME LIKE 'ts1';
NAME	ENCRYPTION
ts1	N
DROP TABLESPACE ts1;
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=false;
# [CREATE TABLESPACE] Case 7 )
`````````````````````````````````````````````````````````
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=true;
CREATE TABLESPACE ts1 ADD DATAFILE 'ts1.ibd' ENCRYPTION='y';
SHOW WARNINGS;
Level	Code	Message
SELECT NAME, ENCRYPTION FROM INFORMATION_SCHEMA.INNODB_TABLESPACES
WHERE NAME LIKE 'ts1';
NAME	ENCRYPTION
ts1	Y
DROP TABLESPACE ts1;
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=false;
# [CREATE TABLESPACE] Case 8 )
`````````````````````````````````````````````````````````
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=true;
CREATE TABLESPACE ts1 ADD DATAFILE 'ts1.ibd' ENCRYPTION='n';
SHOW WARNINGS;
Level	Code	Message
SELECT NAME, ENCRYPTION FROM INFORMATION_SCHEMA.INNODB_TABLESPACES
WHERE NAME LIKE 'ts1';
NAME	ENCRYPTION
ts1	N
DROP TABLESPACE ts1;
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=false;
`````````````````````````````````````````````````````````
# table_encryption_privilege_check=true
# [CREATE TABLESPACE] Case 9 )
`````````````````````````````````````````````````````````
SET GLOBAL table_encryption_privilege_check=true;
SET SESSION default_table_encryption=false;
CREATE TABLESPACE ts1 ADD DATAFILE 'ts1.ibd' ENCRYPTION='y';
SHOW WARNINGS;
ERROR HY000: Tablespace encryption differ from 'default_table_encryption' setting, and user doesn't have enough privilege.
# Check that we never fail with TABLE_ENCRYPTION_ADMIN
# Grant user with TABLE_ENCRYPTION_ADMIN
GRANT TABLE_ENCRYPTION_ADMIN ON *.* TO u1@localhost;
CREATE TABLESPACE ts1 ADD DATAFILE 'ts1.ibd' ENCRYPTION='y';
SHOW WARNINGS;
Level	Code	Message
SELECT NAME, ENCRYPTION FROM INFORMATION_SCHEMA.INNODB_TABLESPACES
WHERE NAME LIKE 'ts1';
NAME	ENCRYPTION
ts1	Y
DROP TABLESPACE ts1;
# Revoke TABLE_ENCRYPTION_ADMIN from user.
REVOKE TABLE_ENCRYPTION_ADMIN ON *.* FROM u1@localhost;
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=false;
# [CREATE TABLESPACE] Case 10 )
`````````````````````````````````````````````````````````
SET GLOBAL table_encryption_privilege_check=true;
SET SESSION default_table_encryption=false;
CREATE TABLESPACE ts1 ADD DATAFILE 'ts1.ibd' ENCRYPTION='n';
SHOW WARNINGS;
Level	Code	Message
SELECT NAME, ENCRYPTION FROM INFORMATION_SCHEMA.INNODB_TABLESPACES
WHERE NAME LIKE 'ts1';
NAME	ENCRYPTION
ts1	N
DROP TABLESPACE ts1;
# Check that we never fail with TABLE_ENCRYPTION_ADMIN
# Grant user with TABLE_ENCRYPTION_ADMIN
GRANT TABLE_ENCRYPTION_ADMIN ON *.* TO u1@localhost;
CREATE TABLESPACE ts1 ADD DATAFILE 'ts1.ibd' ENCRYPTION='n';
SHOW WARNINGS;
Level	Code	Message
SELECT NAME, ENCRYPTION FROM INFORMATION_SCHEMA.INNODB_TABLESPACES
WHERE NAME LIKE 'ts1';
NAME	ENCRYPTION
ts1	N
DROP TABLESPACE ts1;
# Revoke TABLE_ENCRYPTION_ADMIN from user.
REVOKE TABLE_ENCRYPTION_ADMIN ON *.* FROM u1@localhost;
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=false;
# [CREATE TABLESPACE] Case 11 )
`````````````````````````````````````````````````````````
SET GLOBAL table_encryption_privilege_check=true;
SET SESSION default_table_encryption=true;
CREATE TABLESPACE ts1 ADD DATAFILE 'ts1.ibd' ENCRYPTION='y';
SHOW WARNINGS;
Level	Code	Message
SELECT NAME, ENCRYPTION FROM INFORMATION_SCHEMA.INNODB_TABLESPACES
WHERE NAME LIKE 'ts1';
NAME	ENCRYPTION
ts1	Y
DROP TABLESPACE ts1;
# Check that we never fail with TABLE_ENCRYPTION_ADMIN
# Grant user with TABLE_ENCRYPTION_ADMIN
GRANT TABLE_ENCRYPTION_ADMIN ON *.* TO u1@localhost;
CREATE TABLESPACE ts1 ADD DATAFILE 'ts1.ibd' ENCRYPTION='y';
SHOW WARNINGS;
Level	Code	Message
SELECT NAME, ENCRYPTION FROM INFORMATION_SCHEMA.INNODB_TABLESPACES
WHERE NAME LIKE 'ts1';
NAME	ENCRYPTION
ts1	Y
DROP TABLESPACE ts1;
# Revoke TABLE_ENCRYPTION_ADMIN from user.
REVOKE TABLE_ENCRYPTION_ADMIN ON *.* FROM u1@localhost;
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=false;
# [CREATE TABLESPACE] Case 12 )
`````````````````````````````````````````````````````````
SET GLOBAL table_encryption_privilege_check=true;
SET SESSION default_table_encryption=true;
CREATE TABLESPACE ts1 ADD DATAFILE 'ts1.ibd' ENCRYPTION='n';
SHOW WARNINGS;
ERROR HY000: Tablespace encryption differ from 'default_table_encryption' setting, and user doesn't have enough privilege.
# Check that we never fail with TABLE_ENCRYPTION_ADMIN
# Grant user with TABLE_ENCRYPTION_ADMIN
GRANT TABLE_ENCRYPTION_ADMIN ON *.* TO u1@localhost;
CREATE TABLESPACE ts1 ADD DATAFILE 'ts1.ibd' ENCRYPTION='n';
SHOW WARNINGS;
Level	Code	Message
SELECT NAME, ENCRYPTION FROM INFORMATION_SCHEMA.INNODB_TABLESPACES
WHERE NAME LIKE 'ts1';
NAME	ENCRYPTION
ts1	N
DROP TABLESPACE ts1;
# Revoke TABLE_ENCRYPTION_ADMIN from user.
REVOKE TABLE_ENCRYPTION_ADMIN ON *.* FROM u1@localhost;
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=false;
`````````````````````````````````````````````````````````
# Check for invalid ENCRYPTION clause value.
# [CREATE TABLESPACE] Case 13 )
`````````````````````````````````````````````````````````
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=false;
CREATE TABLESPACE ts1 ADD DATAFILE 'ts1.ibd' ENCRYPTION='k';
SHOW WARNINGS;
ERROR HY000: Invalid encryption option.
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=false;
`````````````````````````````````````````````````````````
# The test cases run ALTER TABLESPACE (with no tables)
# See test tablespace_with_tables.test.
`````````````````````````````````````````````````````````
# Unencrypted TABLESPACE to Unencrypted TABLESPACE (Nop)
#
# [ALTER TABLESPACE] Case 1 )
`````````````````````````````````````````````````````````
CREATE TABLESPACE ts1 ADD DATAFILE 'ts1.ibd' ENCRYPTION='n';
SELECT NAME, ENCRYPTION FROM INFORMATION_SCHEMA.INNODB_TABLESPACES
WHERE NAME LIKE 'ts1';
NAME	ENCRYPTION
ts1	N
SET SESSION default_table_encryption=false;
SET GLOBAL table_encryption_privilege_check=false;
ALTER TABLESPACE ts1 ENCRYPTION='n';
SHOW WARNINGS;
Level	Code	Message
SELECT NAME, ENCRYPTION FROM INFORMATION_SCHEMA.INNODB_TABLESPACES
WHERE NAME LIKE 'ts1';
NAME	ENCRYPTION
ts1	N
DROP TABLESPACE ts1;
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=false;
# [ALTER TABLESPACE] Case 2 )
`````````````````````````````````````````````````````````
CREATE TABLESPACE ts1 ADD DATAFILE 'ts1.ibd' ENCRYPTION='n';
SELECT NAME, ENCRYPTION FROM INFORMATION_SCHEMA.INNODB_TABLESPACES
WHERE NAME LIKE 'ts1';
NAME	ENCRYPTION
ts1	N
SET SESSION default_table_encryption=false;
SET GLOBAL table_encryption_privilege_check=true;
ALTER TABLESPACE ts1 ENCRYPTION='n';
SHOW WARNINGS;
Level	Code	Message
SELECT NAME, ENCRYPTION FROM INFORMATION_SCHEMA.INNODB_TABLESPACES
WHERE NAME LIKE 'ts1';
NAME	ENCRYPTION
ts1	N
DROP TABLESPACE ts1;
# Check that we never fail with TABLE_ENCRYPTION_ADMIN
# Grant user with TABLE_ENCRYPTION_ADMIN
GRANT TABLE_ENCRYPTION_ADMIN ON *.* TO u1@localhost;
SET GLOBAL table_encryption_privilege_check=false;
CREATE TABLESPACE ts1 ADD DATAFILE 'ts1.ibd' ENCRYPTION='n';
SELECT NAME, ENCRYPTION FROM INFORMATION_SCHEMA.INNODB_TABLESPACES
WHERE NAME LIKE 'ts1';
NAME	ENCRYPTION
ts1	N
SET GLOBAL table_encryption_privilege_check=true;
ALTER TABLESPACE ts1 ENCRYPTION='n';
SHOW WARNINGS;
Level	Code	Message
SELECT NAME, ENCRYPTION FROM INFORMATION_SCHEMA.INNODB_TABLESPACES
WHERE NAME LIKE 'ts1';
NAME	ENCRYPTION
ts1	N
DROP TABLESPACE ts1;
# Revoke TABLE_ENCRYPTION_ADMIN from user.
REVOKE TABLE_ENCRYPTION_ADMIN ON *.* FROM u1@localhost;
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=false;
`````````````````````````````````````````````````````````
# Encrypted TABLESPACE to Encrypted TABLESPACE (Nop)
#
# [ALTER TABLESPACE] Case 3 )
`````````````````````````````````````````````````````````
CREATE TABLESPACE ts1 ADD DATAFILE 'ts1.ibd' ENCRYPTION='y';
SELECT NAME, ENCRYPTION FROM INFORMATION_SCHEMA.INNODB_TABLESPACES
WHERE NAME LIKE 'ts1';
NAME	ENCRYPTION
ts1	Y
SET SESSION default_table_encryption=true;
SET GLOBAL table_encryption_privilege_check=false;
ALTER TABLESPACE ts1 ENCRYPTION='y';
SHOW WARNINGS;
Level	Code	Message
SELECT NAME, ENCRYPTION FROM INFORMATION_SCHEMA.INNODB_TABLESPACES
WHERE NAME LIKE 'ts1';
NAME	ENCRYPTION
ts1	Y
DROP TABLESPACE ts1;
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=false;
# [ALTER TABLESPACE] Case 4 )
`````````````````````````````````````````````````````````
CREATE TABLESPACE ts1 ADD DATAFILE 'ts1.ibd' ENCRYPTION='y';
SELECT NAME, ENCRYPTION FROM INFORMATION_SCHEMA.INNODB_TABLESPACES
WHERE NAME LIKE 'ts1';
NAME	ENCRYPTION
ts1	Y
SET SESSION default_table_encryption=true;
SET GLOBAL table_encryption_privilege_check=true;
ALTER TABLESPACE ts1 ENCRYPTION='y';
SHOW WARNINGS;
Level	Code	Message
SELECT NAME, ENCRYPTION FROM INFORMATION_SCHEMA.INNODB_TABLESPACES
WHERE NAME LIKE 'ts1';
NAME	ENCRYPTION
ts1	Y
DROP TABLESPACE ts1;
# Check that we never fail with TABLE_ENCRYPTION_ADMIN
# Grant user with TABLE_ENCRYPTION_ADMIN
GRANT TABLE_ENCRYPTION_ADMIN ON *.* TO u1@localhost;
SET GLOBAL table_encryption_privilege_check=false;
CREATE TABLESPACE ts1 ADD DATAFILE 'ts1.ibd' ENCRYPTION='y';
SELECT NAME, ENCRYPTION FROM INFORMATION_SCHEMA.INNODB_TABLESPACES
WHERE NAME LIKE 'ts1';
NAME	ENCRYPTION
ts1	Y
SET GLOBAL table_encryption_privilege_check=true;
ALTER TABLESPACE ts1 ENCRYPTION='y';
SHOW WARNINGS;
Level	Code	Message
SELECT NAME, ENCRYPTION FROM INFORMATION_SCHEMA.INNODB_TABLESPACES
WHERE NAME LIKE 'ts1';
NAME	ENCRYPTION
ts1	Y
DROP TABLESPACE ts1;
# Revoke TABLE_ENCRYPTION_ADMIN from user.
REVOKE TABLE_ENCRYPTION_ADMIN ON *.* FROM u1@localhost;
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=false;
`````````````````````````````````````````````````````````
# Unencrypted TABLESPACE to encrypted TABLESPACE
# with database encryption default 'n'
#
# [ALTER TABLESPACE] Case 5 )
`````````````````````````````````````````````````````````
CREATE TABLESPACE ts1 ADD DATAFILE 'ts1.ibd' ENCRYPTION='n';
SELECT NAME, ENCRYPTION FROM INFORMATION_SCHEMA.INNODB_TABLESPACES
WHERE NAME LIKE 'ts1';
NAME	ENCRYPTION
ts1	N
SET SESSION default_table_encryption=false;
SET GLOBAL table_encryption_privilege_check=false;
ALTER TABLESPACE ts1 ENCRYPTION='y';
SHOW WARNINGS;
Level	Code	Message
SELECT NAME, ENCRYPTION FROM INFORMATION_SCHEMA.INNODB_TABLESPACES
WHERE NAME LIKE 'ts1';
NAME	ENCRYPTION
ts1	Y
DROP TABLESPACE ts1;
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=false;
# [ALTER TABLESPACE] Case 6 )
`````````````````````````````````````````````````````````
CREATE TABLESPACE ts1 ADD DATAFILE 'ts1.ibd' ENCRYPTION='n';
SELECT NAME, ENCRYPTION FROM INFORMATION_SCHEMA.INNODB_TABLESPACES
WHERE NAME LIKE 'ts1';
NAME	ENCRYPTION
ts1	N
SET SESSION default_table_encryption=false;
SET GLOBAL table_encryption_privilege_check=true;
ALTER TABLESPACE ts1 ENCRYPTION='y';
ERROR HY000: Tablespace encryption differ from 'default_table_encryption' setting, and user doesn't have enough privilege.
SHOW WARNINGS;
Level	Code	Message
Error	3828	Tablespace encryption differ from 'default_table_encryption' setting, and user doesn't have enough privilege.
SELECT NAME, ENCRYPTION FROM INFORMATION_SCHEMA.INNODB_TABLESPACES
WHERE NAME LIKE 'ts1';
NAME	ENCRYPTION
ts1	N
DROP TABLESPACE ts1;
# Check that we never fail with TABLE_ENCRYPTION_ADMIN
# Grant user with TABLE_ENCRYPTION_ADMIN
GRANT TABLE_ENCRYPTION_ADMIN ON *.* TO u1@localhost;
SET GLOBAL table_encryption_privilege_check=false;
CREATE TABLESPACE ts1 ADD DATAFILE 'ts1.ibd' ENCRYPTION='n';
SELECT NAME, ENCRYPTION FROM INFORMATION_SCHEMA.INNODB_TABLESPACES
WHERE NAME LIKE 'ts1';
NAME	ENCRYPTION
ts1	N
SET GLOBAL table_encryption_privilege_check=true;
ALTER TABLESPACE ts1 ENCRYPTION='y';
SHOW WARNINGS;
Level	Code	Message
SELECT NAME, ENCRYPTION FROM INFORMATION_SCHEMA.INNODB_TABLESPACES
WHERE NAME LIKE 'ts1';
NAME	ENCRYPTION
ts1	Y
DROP TABLESPACE ts1;
# Revoke TABLE_ENCRYPTION_ADMIN from user.
REVOKE TABLE_ENCRYPTION_ADMIN ON *.* FROM u1@localhost;
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=false;
`````````````````````````````````````````````````````````
# Unencrypted TABLESPACE to encrypted TABLESPACE
# with database encryption default 'y'
#
# [ALTER TABLESPACE] Case 7 )
`````````````````````````````````````````````````````````
CREATE TABLESPACE ts1 ADD DATAFILE 'ts1.ibd' ENCRYPTION='n';
SELECT NAME, ENCRYPTION FROM INFORMATION_SCHEMA.INNODB_TABLESPACES
WHERE NAME LIKE 'ts1';
NAME	ENCRYPTION
ts1	N
SET SESSION default_table_encryption=true;
SET GLOBAL table_encryption_privilege_check=false;
ALTER TABLESPACE ts1 ENCRYPTION='y';
SHOW WARNINGS;
Level	Code	Message
SELECT NAME, ENCRYPTION FROM INFORMATION_SCHEMA.INNODB_TABLESPACES
WHERE NAME LIKE 'ts1';
NAME	ENCRYPTION
ts1	Y
DROP TABLESPACE ts1;
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=false;
# [ALTER TABLESPACE] Case 8 )
`````````````````````````````````````````````````````````
CREATE TABLESPACE ts1 ADD DATAFILE 'ts1.ibd' ENCRYPTION='n';
SELECT NAME, ENCRYPTION FROM INFORMATION_SCHEMA.INNODB_TABLESPACES
WHERE NAME LIKE 'ts1';
NAME	ENCRYPTION
ts1	N
SET SESSION default_table_encryption=true;
SET GLOBAL table_encryption_privilege_check=true;
ALTER TABLESPACE ts1 ENCRYPTION='y';
SHOW WARNINGS;
Level	Code	Message
SELECT NAME, ENCRYPTION FROM INFORMATION_SCHEMA.INNODB_TABLESPACES
WHERE NAME LIKE 'ts1';
NAME	ENCRYPTION
ts1	Y
DROP TABLESPACE ts1;
# Check that we never fail with TABLE_ENCRYPTION_ADMIN
# Grant user with TABLE_ENCRYPTION_ADMIN
GRANT TABLE_ENCRYPTION_ADMIN ON *.* TO u1@localhost;
SET GLOBAL table_encryption_privilege_check=false;
CREATE TABLESPACE ts1 ADD DATAFILE 'ts1.ibd' ENCRYPTION='n';
SELECT NAME, ENCRYPTION FROM INFORMATION_SCHEMA.INNODB_TABLESPACES
WHERE NAME LIKE 'ts1';
NAME	ENCRYPTION
ts1	N
SET GLOBAL table_encryption_privilege_check=true;
ALTER TABLESPACE ts1 ENCRYPTION='y';
SHOW WARNINGS;
Level	Code	Message
SELECT NAME, ENCRYPTION FROM INFORMATION_SCHEMA.INNODB_TABLESPACES
WHERE NAME LIKE 'ts1';
NAME	ENCRYPTION
ts1	Y
DROP TABLESPACE ts1;
# Revoke TABLE_ENCRYPTION_ADMIN from user.
REVOKE TABLE_ENCRYPTION_ADMIN ON *.* FROM u1@localhost;
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=false;
`````````````````````````````````````````````````````````
# Encrypted TABLESPACE to unencrypted TABLESPACE
# with database encryption default 'n'
#
# [ALTER TABLESPACE] Case 9 )
`````````````````````````````````````````````````````````
CREATE TABLESPACE ts1 ADD DATAFILE 'ts1.ibd' ENCRYPTION='y';
SELECT NAME, ENCRYPTION FROM INFORMATION_SCHEMA.INNODB_TABLESPACES
WHERE NAME LIKE 'ts1';
NAME	ENCRYPTION
ts1	Y
SET SESSION default_table_encryption=false;
SET GLOBAL table_encryption_privilege_check=false;
ALTER TABLESPACE ts1 ENCRYPTION='n';
SHOW WARNINGS;
Level	Code	Message
SELECT NAME, ENCRYPTION FROM INFORMATION_SCHEMA.INNODB_TABLESPACES
WHERE NAME LIKE 'ts1';
NAME	ENCRYPTION
ts1	N
DROP TABLESPACE ts1;
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=false;
# [ALTER TABLESPACE] Case 10 )
`````````````````````````````````````````````````````````
CREATE TABLESPACE ts1 ADD DATAFILE 'ts1.ibd' ENCRYPTION='y';
SELECT NAME, ENCRYPTION FROM INFORMATION_SCHEMA.INNODB_TABLESPACES
WHERE NAME LIKE 'ts1';
NAME	ENCRYPTION
ts1	Y
SET SESSION default_table_encryption=false;
SET GLOBAL table_encryption_privilege_check=true;
ALTER TABLESPACE ts1 ENCRYPTION='n';
SHOW WARNINGS;
Level	Code	Message
SELECT NAME, ENCRYPTION FROM INFORMATION_SCHEMA.INNODB_TABLESPACES
WHERE NAME LIKE 'ts1';
NAME	ENCRYPTION
ts1	N
DROP TABLESPACE ts1;
# Check that we never fail with TABLE_ENCRYPTION_ADMIN
# Grant user with TABLE_ENCRYPTION_ADMIN
GRANT TABLE_ENCRYPTION_ADMIN ON *.* TO u1@localhost;
SET GLOBAL table_encryption_privilege_check=false;
CREATE TABLESPACE ts1 ADD DATAFILE 'ts1.ibd' ENCRYPTION='y';
SELECT NAME, ENCRYPTION FROM INFORMATION_SCHEMA.INNODB_TABLESPACES
WHERE NAME LIKE 'ts1';
NAME	ENCRYPTION
ts1	Y
SET GLOBAL table_encryption_privilege_check=true;
ALTER TABLESPACE ts1 ENCRYPTION='n';
SHOW WARNINGS;
Level	Code	Message
SELECT NAME, ENCRYPTION FROM INFORMATION_SCHEMA.INNODB_TABLESPACES
WHERE NAME LIKE 'ts1';
NAME	ENCRYPTION
ts1	N
DROP TABLESPACE ts1;
# Revoke TABLE_ENCRYPTION_ADMIN from user.
REVOKE TABLE_ENCRYPTION_ADMIN ON *.* FROM u1@localhost;
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=false;
`````````````````````````````````````````````````````````
# Encrypted TABLESPACE to unencrypted TABLESPACE
# with database encryption default 'y'
#
# [ALTER TABLESPACE] Case 11 )
`````````````````````````````````````````````````````````
CREATE TABLESPACE ts1 ADD DATAFILE 'ts1.ibd' ENCRYPTION='y';
SELECT NAME, ENCRYPTION FROM INFORMATION_SCHEMA.INNODB_TABLESPACES
WHERE NAME LIKE 'ts1';
NAME	ENCRYPTION
ts1	Y
SET SESSION default_table_encryption=true;
SET GLOBAL table_encryption_privilege_check=false;
ALTER TABLESPACE ts1 ENCRYPTION='n';
SHOW WARNINGS;
Level	Code	Message
SELECT NAME, ENCRYPTION FROM INFORMATION_SCHEMA.INNODB_TABLESPACES
WHERE NAME LIKE 'ts1';
NAME	ENCRYPTION
ts1	N
DROP TABLESPACE ts1;
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=false;
# [ALTER TABLESPACE] Case 12 )
`````````````````````````````````````````````````````````
CREATE TABLESPACE ts1 ADD DATAFILE 'ts1.ibd' ENCRYPTION='y';
SELECT NAME, ENCRYPTION FROM INFORMATION_SCHEMA.INNODB_TABLESPACES
WHERE NAME LIKE 'ts1';
NAME	ENCRYPTION
ts1	Y
SET SESSION default_table_encryption=true;
SET GLOBAL table_encryption_privilege_check=true;
ALTER TABLESPACE ts1 ENCRYPTION='n';
ERROR HY000: Tablespace encryption differ from 'default_table_encryption' setting, and user doesn't have enough privilege.
SHOW WARNINGS;
Level	Code	Message
Error	3828	Tablespace encryption differ from 'default_table_encryption' setting, and user doesn't have enough privilege.
SELECT NAME, ENCRYPTION FROM INFORMATION_SCHEMA.INNODB_TABLESPACES
WHERE NAME LIKE 'ts1';
NAME	ENCRYPTION
ts1	Y
DROP TABLESPACE ts1;
# Check that we never fail with TABLE_ENCRYPTION_ADMIN
# Grant user with TABLE_ENCRYPTION_ADMIN
GRANT TABLE_ENCRYPTION_ADMIN ON *.* TO u1@localhost;
SET GLOBAL table_encryption_privilege_check=false;
CREATE TABLESPACE ts1 ADD DATAFILE 'ts1.ibd' ENCRYPTION='y';
SELECT NAME, ENCRYPTION FROM INFORMATION_SCHEMA.INNODB_TABLESPACES
WHERE NAME LIKE 'ts1';
NAME	ENCRYPTION
ts1	Y
SET GLOBAL table_encryption_privilege_check=true;
ALTER TABLESPACE ts1 ENCRYPTION='n';
SHOW WARNINGS;
Level	Code	Message
SELECT NAME, ENCRYPTION FROM INFORMATION_SCHEMA.INNODB_TABLESPACES
WHERE NAME LIKE 'ts1';
NAME	ENCRYPTION
ts1	N
DROP TABLESPACE ts1;
# Revoke TABLE_ENCRYPTION_ADMIN from user.
REVOKE TABLE_ENCRYPTION_ADMIN ON *.* FROM u1@localhost;
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=false;
`````````````````````````````````````````````````````````
# Check for Invalid encryption option.
# [ALTER TABLESPACE] Case 13 )
`````````````````````````````````````````````````````````
CREATE TABLESPACE ts1 ADD DATAFILE 'ts1.ibd' ENCRYPTION='y';
SELECT NAME, ENCRYPTION FROM INFORMATION_SCHEMA.INNODB_TABLESPACES
WHERE NAME LIKE 'ts1';
NAME	ENCRYPTION
ts1	Y
SET SESSION default_table_encryption=false;
SET GLOBAL table_encryption_privilege_check=false;
ALTER TABLESPACE ts1 ENCRYPTION='k';
ERROR HY000: Invalid encryption option.
SHOW WARNINGS;
Level	Code	Message
Error	3184	Invalid encryption option.
Error	1533	Failed to alter: TABLESPACE ts1
Error	1030	Got error 168 - 'Unknown (generic) error from engine' from storage engine
SELECT NAME, ENCRYPTION FROM INFORMATION_SCHEMA.INNODB_TABLESPACES
WHERE NAME LIKE 'ts1';
NAME	ENCRYPTION
ts1	Y
DROP TABLESPACE ts1;
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=false;
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
