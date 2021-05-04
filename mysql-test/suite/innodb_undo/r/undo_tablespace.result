#
# InnoDB supports CREATE/ALTER/DROP UNDO TABLESPACE
#
SET GLOBAL innodb_fast_shutdown = 0;
# restart
SET GLOBAL innodb_undo_log_truncate = OFF;
CREATE UNDO TABLESPACE undo_003 ADD DATAFILE 'undo_003.ibu';
CREATE UNDO TABLESPACE undo_004 ADD DATAFILE 'undo_004.ibu';
CREATE UNDO TABLESPACE undo_005 ADD DATAFILE '5.ibu';
SELECT NAME, SPACE_TYPE, STATE FROM INFORMATION_SCHEMA.INNODB_TABLESPACES
WHERE SPACE_TYPE = 'Undo' ORDER BY NAME;
NAME	SPACE_TYPE	STATE
innodb_undo_001	Undo	active
innodb_undo_002	Undo	active
undo_003	Undo	active
undo_004	Undo	active
undo_005	Undo	active
SELECT TABLESPACE_NAME, FILE_TYPE, FILE_NAME FROM INFORMATION_SCHEMA.FILES
WHERE FILE_NAME LIKE '%.ibu' ORDER BY TABLESPACE_NAME;
TABLESPACE_NAME	FILE_TYPE	FILE_NAME
undo_003	UNDO LOG	./undo_003.ibu
undo_004	UNDO LOG	./undo_004.ibu
undo_005	UNDO LOG	./5.ibu
CREATE TABLESPACE ts1 ADD DATAFILE 'ts1.ibd';
CREATE TABLE t1 (a int primary key) TABLESPACE ts1;
#
# Populate t1 with separate INSERTs so that all rsegs are used.
#
CREATE PROCEDURE populate_t1(IN BASE INT, IN SIZE INT)
BEGIN
DECLARE i INT DEFAULT BASE;
WHILE (i <= SIZE) DO
INSERT INTO t1 values (i);
SET i = i + 1;
END WHILE;
END|
CALL populate_t1(1, 1000);
#
# Show that the implicit undo tablespaces may be set inactive
# and that a minimum of 2 undo tablespaces must remain active.
#
ALTER UNDO TABLESPACE innodb_undo_001 SET INACTIVE;
ALTER UNDO TABLESPACE innodb_undo_002 SET INACTIVE;
ALTER UNDO TABLESPACE undo_003 SET INACTIVE;
ALTER UNDO TABLESPACE undo_004 SET INACTIVE;
ERROR HY000: Cannot set undo_004 inactive since there would be less than 2 undo tablespaces left active.
SHOW WARNINGS;
Level	Code	Message
Error	3655	Cannot set undo_004 inactive since there would be less than 2 undo tablespaces left active.
Error	1533	Failed to alter: UNDO TABLESPACE undo_004
Error	3655	ALTER UNDO TABLEPSPACE operation is disallowed on undo_004
ALTER UNDO TABLESPACE undo_005 SET INACTIVE;
ERROR HY000: Cannot set undo_005 inactive since there would be less than 2 undo tablespaces left active.
SHOW WARNINGS;
Level	Code	Message
Error	3655	Cannot set undo_005 inactive since there would be less than 2 undo tablespaces left active.
Error	1533	Failed to alter: UNDO TABLESPACE undo_005
Error	3655	ALTER UNDO TABLEPSPACE operation is disallowed on undo_005
SELECT NAME, SPACE_TYPE, STATE FROM INFORMATION_SCHEMA.INNODB_TABLESPACES
WHERE SPACE_TYPE = 'Undo' ORDER BY NAME;
NAME	SPACE_TYPE	STATE
innodb_undo_001	Undo	empty
innodb_undo_002	Undo	empty
undo_003	Undo	empty
undo_004	Undo	active
undo_005	Undo	active
SELECT TABLESPACE_NAME, FILE_TYPE, FILE_NAME FROM INFORMATION_SCHEMA.FILES
WHERE FILE_NAME LIKE '%.ibu' ORDER BY TABLESPACE_NAME;
TABLESPACE_NAME	FILE_TYPE	FILE_NAME
undo_003	UNDO LOG	./undo_003.ibu
undo_004	UNDO LOG	./undo_004.ibu
undo_005	UNDO LOG	./5.ibu
ALTER UNDO TABLESPACE innodb_undo_001 SET ACTIVE;
ALTER UNDO TABLESPACE innodb_undo_002 SET ACTIVE;
SELECT NAME, SPACE_TYPE, STATE FROM INFORMATION_SCHEMA.INNODB_TABLESPACES
WHERE SPACE_TYPE = 'Undo' ORDER BY NAME;
NAME	SPACE_TYPE	STATE
innodb_undo_001	Undo	active
innodb_undo_002	Undo	active
undo_003	Undo	empty
undo_004	Undo	active
undo_005	Undo	active
#
# Show that SET ACTIVE and SET INACTIVE are indempotent.
#
ALTER UNDO TABLESPACE undo_003 SET ACTIVE;
ALTER UNDO TABLESPACE undo_003 SET ACTIVE;
ALTER UNDO TABLESPACE undo_003 SET INACTIVE;
ALTER UNDO TABLESPACE undo_003 SET INACTIVE;
ALTER UNDO TABLESPACE undo_003 SET INACTIVE;
#
# SET the explicit tablespaces INACTIVE.
#
ALTER UNDO TABLESPACE undo_004 SET INACTIVE;
ALTER UNDO TABLESPACE undo_005 SET INACTIVE;
SHOW GLOBAL STATUS LIKE '%undo%';
Variable_name	Value
Innodb_undo_tablespaces_total	5
Innodb_undo_tablespaces_implicit	2
Innodb_undo_tablespaces_explicit	3
Innodb_undo_tablespaces_active	2
SELECT NAME, SPACE_TYPE, STATE FROM INFORMATION_SCHEMA.INNODB_TABLESPACES
WHERE SPACE_TYPE = 'Undo' ORDER BY NAME;
NAME	SPACE_TYPE	STATE
innodb_undo_001	Undo	active
innodb_undo_002	Undo	active
undo_003	Undo	empty
undo_004	Undo	empty
undo_005	Undo	empty
SELECT TABLESPACE_NAME, FILE_TYPE, FILE_NAME FROM INFORMATION_SCHEMA.FILES
WHERE FILE_NAME LIKE '%.ibu' ORDER BY TABLESPACE_NAME;
TABLESPACE_NAME	FILE_TYPE	FILE_NAME
undo_003	UNDO LOG	./undo_003.ibu
undo_004	UNDO LOG	./undo_004.ibu
undo_005	UNDO LOG	./5.ibu
#
# Drop undo_003
#
DROP UNDO TABLESPACE undo_003;
SELECT NAME, SPACE_TYPE, STATE FROM INFORMATION_SCHEMA.INNODB_TABLESPACES
WHERE SPACE_TYPE = 'Undo' ORDER BY NAME;
NAME	SPACE_TYPE	STATE
innodb_undo_001	Undo	active
innodb_undo_002	Undo	active
undo_004	Undo	empty
undo_005	Undo	empty
SELECT TABLESPACE_NAME, FILE_TYPE, FILE_NAME FROM INFORMATION_SCHEMA.FILES
WHERE FILE_NAME LIKE '%.ibu' ORDER BY TABLESPACE_NAME;
TABLESPACE_NAME	FILE_TYPE	FILE_NAME
undo_004	UNDO LOG	./undo_004.ibu
undo_005	UNDO LOG	./5.ibu
ALTER UNDO TABLESPACE undo_005 SET ACTIVE;
SELECT NAME, SPACE_TYPE, STATE FROM INFORMATION_SCHEMA.INNODB_TABLESPACES
WHERE SPACE_TYPE = 'Undo' ORDER BY NAME;
NAME	SPACE_TYPE	STATE
innodb_undo_001	Undo	active
innodb_undo_002	Undo	active
undo_004	Undo	empty
undo_005	Undo	active
#
# Try various bad CREATE UNDO TABLESPACE commands
#
CREATE UNDO TABLESPACE innodb_undo_001 ADD DATAFILE 'undo_001.ibu';
ERROR 42000: InnoDB: Tablespace names starting with `innodb_` are reserved.
SHOW WARNINGS;
Level	Code	Message
Error	3119	InnoDB: Tablespace names starting with `innodb_` are reserved.
Error	3119	Incorrect tablespace name `innodb_undo_001`
CREATE UNDO TABLESPACE undo_5 ADD DATAFILE '5.ibu';
ERROR HY000: Duplicate file name for tablespace 'undo_5'
SHOW WARNINGS;
Level	Code	Message
Error	3606	Duplicate file name for tablespace 'undo_5'
CREATE UNDO TABLESPACE undo_99 ADD DATAFILE 'undo_99.ibu';
ERROR HY000: The ADD DATAFILE filepath already exists.
SHOW WARNINGS;
Level	Code	Message
Error	3121	The ADD DATAFILE filepath already exists.
Error	1528	Failed to create UNDO TABLESPACE undo_99
Error	3121	Incorrect File Name 'undo_99.ibu'.
CREATE UNDO TABLESPACE 'undo_99' ADD DATAFILE 'undo_001.ibu';
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near ''undo_99' ADD DATAFILE 'undo_001.ibu'' at line 1
CREATE UNDO TABLESPACE `undo_99`;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near '' at line 1
CREATE UNDO TABLESPACE undo_99 ADD DATAFILE 'undo_99';
ERROR HY000: The ADD DATAFILE filepath must end with '.ibu'.
SHOW WARNINGS;
Level	Code	Message
Error	3121	The ADD DATAFILE filepath must end with '.ibu'.
Error	1528	Failed to create UNDO TABLESPACE undo_99
Error	3121	Incorrect File Name 'undo_99'.
CREATE UNDO TABLESPACE undo_99 ADD DATAFILE 'undo_99.ibd';
ERROR HY000: The ADD DATAFILE filepath must end with '.ibu'.
SHOW WARNINGS;
Level	Code	Message
Error	3121	The ADD DATAFILE filepath must end with '.ibu'.
Error	1528	Failed to create UNDO TABLESPACE undo_99
Error	3121	Incorrect File Name 'undo_99.ibd'.
CREATE UNDO TABLESPACE undo_99 ADD DATAFILE '/dir_does_not_exist/undo_99.ibu';
ERROR HY000: The directory does not exist or is incorrect.
SHOW WARNINGS;
Level	Code	Message
Error	3121	The directory does not exist or is incorrect.
Error	3121	The UNDO DATAFILE location must be in a known directory.
Error	1528	Failed to create UNDO TABLESPACE undo_99
Error	3121	Incorrect File Name '/dir_does_not_exist/undo_99.ibu'.
CREATE UNDO TABLESPACE undo_99 ADD DATAFILE '../undo_99.ibu';
ERROR HY000: The ADD DATAFILE filepath for an UNDO TABLESPACE cannot be a relative path.
SHOW WARNINGS;
Level	Code	Message
Error	3121	The ADD DATAFILE filepath for an UNDO TABLESPACE cannot be a relative path.
Error	3121	The UNDO DATAFILE location must be in a known directory.
Error	1528	Failed to create UNDO TABLESPACE undo_99
Error	3121	Incorrect File Name '../undo_99.ibu'.
#
# Try various bad ALTER UNDO TABLESPACE commands
#
ALTER UNDO TABLESPACE `undo_99`;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near '' at line 1
ALTER UNDO TABLESPACE `undo_99` SET INACTIVE;
ERROR HY000: Tablespace undo_99 doesn't exist.
SHOW WARNINGS;
Level	Code	Message
Error	3510	Tablespace undo_99 doesn't exist.
ALTER UNDO TABLESPACE `undo_99` SET ACTIVE;
ERROR HY000: Tablespace undo_99 doesn't exist.
SHOW WARNINGS;
Level	Code	Message
Error	3510	Tablespace undo_99 doesn't exist.
ALTER UNDO TABLESPACE `ts1` SET INACTIVE;
ERROR 42000: Cannot ALTER UNDO TABLESPACE `ts1` because it is a general tablespace.  Please use ALTER TABLESPACE.
SHOW WARNINGS;
Level	Code	Message
Error	3119	Cannot ALTER UNDO TABLESPACE `ts1` because it is a general tablespace.  Please use ALTER TABLESPACE.
Error	1533	Failed to alter: UNDO TABLESPACE ts1
Error	3655	ALTER UNDO TABLEPSPACE operation is disallowed on ts1
ALTER UNDO TABLESPACE `ts1` SET ACTIVE;
ERROR 42000: Cannot ALTER UNDO TABLESPACE `ts1` because it is a general tablespace.  Please use ALTER TABLESPACE.
SHOW WARNINGS;
Level	Code	Message
Error	3119	Cannot ALTER UNDO TABLESPACE `ts1` because it is a general tablespace.  Please use ALTER TABLESPACE.
Error	1533	Failed to alter: UNDO TABLESPACE ts1
Error	3655	ALTER UNDO TABLEPSPACE operation is disallowed on ts1
ALTER TABLESPACE undo_005 RENAME TO undo_5;
ERROR 42000: Cannot ALTER TABLESPACE `undo_005` because it is an undo tablespace.  Please use ALTER UNDO TABLESPACE.
SHOW WARNINGS;
Level	Code	Message
Error	3119	Cannot ALTER TABLESPACE `undo_005` because it is an undo tablespace.  Please use ALTER UNDO TABLESPACE.
Error	1533	Failed to alter: TABLESPACE undo_005
Error	3655	ALTER TABLESPACE ... RENAME TO operation is disallowed on undo_005
ALTER UNDO TABLESPACE undo_005 SET EMPTY;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'EMPTY' at line 1
#
# Try various bad DROP UNDO TABLESPACE commands
#
DROP UNDO TABLESPACE innodb_undo_001;
ERROR 42000: InnoDB: Tablespace names starting with `innodb_` are reserved.
SHOW WARNINGS;
Level	Code	Message
Error	3119	InnoDB: Tablespace names starting with `innodb_` are reserved.
Error	3119	Incorrect tablespace name `innodb_undo_001`
DROP UNDO TABLESPACE undo_99;
ERROR HY000: Tablespace undo_99 doesn't exist.
SHOW WARNINGS;
Level	Code	Message
Error	3510	Tablespace undo_99 doesn't exist.
DROP UNDO TABLESPACE undo_005;
ERROR HY000: Failed to drop UNDO TABLESPACE undo_005
SHOW WARNINGS;
Level	Code	Message
Error	1529	Failed to drop UNDO TABLESPACE undo_005
Error	3120	Tablespace `undo_005` is not empty.
DROP TABLESPACE undo_005;
ERROR 42000: Cannot DROP TABLESPACE `undo_005` because it is an undo tablespace.  Please use DROP UNDO TABLESPACE.
SHOW WARNINGS;
Level	Code	Message
Error	3119	Cannot DROP TABLESPACE `undo_005` because it is an undo tablespace.  Please use DROP UNDO TABLESPACE.
Error	1529	Failed to drop TABLESPACE undo_005
Error	3655	DROP TABLEPSPACE operation is disallowed on undo_005
DROP UNDO TABLESPACE ts1;
ERROR 42000: Cannot DROP UNDO TABLESPACE `ts1` because it is a general tablespace.  Please use DROP TABLESPACE.
SHOW WARNINGS;
Level	Code	Message
Error	3119	Cannot DROP UNDO TABLESPACE `ts1` because it is a general tablespace.  Please use DROP TABLESPACE.
Error	1529	Failed to drop UNDO TABLESPACE ts1
Error	3655	DROP UNDO TABLEPSPACE operation is disallowed on ts1
#
# Show that tables cannot be added to an undo tablespace.
#
CREATE TABLE t2 (a int primary key) TABLESPACE undo_004;
ERROR 42000: InnoDB: An undo tablespace cannot contain tables.
SHOW WARNINGS;
Level	Code	Message
Error	3119	InnoDB: An undo tablespace cannot contain tables.
Error	1031	Table storage engine for 't2' doesn't have this option
ALTER TABLE t1 TABLESPACE undo_004;
ERROR 42000: InnoDB: An undo tablespace cannot contain tables.
SHOW WARNINGS;
Level	Code	Message
Error	3119	InnoDB: An undo tablespace cannot contain tables.
Error	1478	Table storage engine 'InnoDB' does not support the create option 'TABLESPACE'
#
# Show that a missing undo tablespace can be dropped
#
# restart
SELECT NAME, SPACE_TYPE, STATE FROM INFORMATION_SCHEMA.INNODB_TABLESPACES
WHERE SPACE_TYPE = 'Undo' ORDER BY NAME;
NAME	SPACE_TYPE	STATE
innodb_undo_001	Undo	active
innodb_undo_002	Undo	active
undo_004	Undo	empty
undo_005	Undo	active
SELECT TABLESPACE_NAME, FILE_TYPE, FILE_NAME FROM INFORMATION_SCHEMA.FILES
WHERE FILE_NAME LIKE '%.ibu' ORDER BY TABLESPACE_NAME;
TABLESPACE_NAME	FILE_TYPE	FILE_NAME
undo_004	NULL	./undo_004.ibu
undo_005	UNDO LOG	./5.ibu
Warnings:
Warning	1812	Tablespace is missing for table undo_004.
DROP UNDO TABLESPACE undo_004;
#
# Show that the setting innodb_validate_tablespace_paths does not affect undo tablespaces.
#
CREATE UNDO TABLESPACE undo_006 ADD DATAFILE 'undo_006.ibu';
SHOW GLOBAL STATUS LIKE '%undo%';
Variable_name	Value
Innodb_undo_tablespaces_total	4
Innodb_undo_tablespaces_implicit	2
Innodb_undo_tablespaces_explicit	2
Innodb_undo_tablespaces_active	4
SELECT NAME, SPACE_TYPE, STATE FROM INFORMATION_SCHEMA.INNODB_TABLESPACES
WHERE SPACE_TYPE = 'Undo' ORDER BY NAME;
NAME	SPACE_TYPE	STATE
innodb_undo_001	Undo	active
innodb_undo_002	Undo	active
undo_005	Undo	active
undo_006	Undo	active
SELECT TABLESPACE_NAME, FILE_TYPE, FILE_NAME FROM INFORMATION_SCHEMA.FILES
WHERE FILE_NAME LIKE '%.ibu' ORDER BY TABLESPACE_NAME;
TABLESPACE_NAME	FILE_TYPE	FILE_NAME
undo_005	UNDO LOG	./5.ibu
undo_006	UNDO LOG	./undo_006.ibu
# Restart with validation turned OFF
# restart: --innodb_validate_tablespace_paths=0
SHOW GLOBAL STATUS LIKE '%undo%';
Variable_name	Value
Innodb_undo_tablespaces_total	4
Innodb_undo_tablespaces_implicit	2
Innodb_undo_tablespaces_explicit	2
Innodb_undo_tablespaces_active	4
SELECT NAME, SPACE_TYPE, STATE FROM INFORMATION_SCHEMA.INNODB_TABLESPACES
WHERE SPACE_TYPE = 'Undo' ORDER BY NAME;
NAME	SPACE_TYPE	STATE
innodb_undo_001	Undo	active
innodb_undo_002	Undo	active
undo_005	Undo	active
undo_006	Undo	active
SELECT TABLESPACE_NAME, FILE_TYPE, FILE_NAME FROM INFORMATION_SCHEMA.FILES
WHERE FILE_NAME LIKE '%.ibu' ORDER BY TABLESPACE_NAME;
TABLESPACE_NAME	FILE_TYPE	FILE_NAME
undo_005	UNDO LOG	./5.ibu
undo_006	UNDO LOG	./undo_006.ibu
# Kill and restart mysqld with validation turned OFF
# Kill and restart: --innodb_validate_tablespace_paths=0
SHOW GLOBAL STATUS LIKE '%undo%';
Variable_name	Value
Innodb_undo_tablespaces_total	4
Innodb_undo_tablespaces_implicit	2
Innodb_undo_tablespaces_explicit	2
Innodb_undo_tablespaces_active	4
SELECT NAME, SPACE_TYPE, STATE FROM INFORMATION_SCHEMA.INNODB_TABLESPACES
WHERE SPACE_TYPE = 'Undo' ORDER BY NAME;
NAME	SPACE_TYPE	STATE
innodb_undo_001	Undo	active
innodb_undo_002	Undo	active
undo_005	Undo	active
undo_006	Undo	active
SELECT TABLESPACE_NAME, FILE_TYPE, FILE_NAME FROM INFORMATION_SCHEMA.FILES
WHERE FILE_NAME LIKE '%.ibu' ORDER BY TABLESPACE_NAME;
TABLESPACE_NAME	FILE_TYPE	FILE_NAME
undo_005	UNDO LOG	./5.ibu
undo_006	UNDO LOG	./undo_006.ibu
# Restart mysqld with validation turned ON
# restart:
SHOW GLOBAL STATUS LIKE '%undo%';
Variable_name	Value
Innodb_undo_tablespaces_total	4
Innodb_undo_tablespaces_implicit	2
Innodb_undo_tablespaces_explicit	2
Innodb_undo_tablespaces_active	4
SELECT NAME, SPACE_TYPE, STATE FROM INFORMATION_SCHEMA.INNODB_TABLESPACES
WHERE SPACE_TYPE = 'Undo' ORDER BY NAME;
NAME	SPACE_TYPE	STATE
innodb_undo_001	Undo	active
innodb_undo_002	Undo	active
undo_005	Undo	active
undo_006	Undo	active
SELECT TABLESPACE_NAME, FILE_TYPE, FILE_NAME FROM INFORMATION_SCHEMA.FILES
WHERE FILE_NAME LIKE '%.ibu' ORDER BY TABLESPACE_NAME;
TABLESPACE_NAME	FILE_TYPE	FILE_NAME
undo_005	UNDO LOG	./5.ibu
undo_006	UNDO LOG	./undo_006.ibu
#
# Cleanup
#
DROP TABLE t1;
DROP TABLESPACE ts1;
DROP PROCEDURE populate_t1;
ALTER UNDO TABLESPACE undo_005 SET INACTIVE;
DROP UNDO TABLESPACE undo_005;
ALTER UNDO TABLESPACE undo_006 SET INACTIVE;
DROP UNDO TABLESPACE undo_006;
