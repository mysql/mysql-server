#
# Test Mixed Case Identifiers with --lower-case-table-name=0
#
#
# Show how Mixed Case identifiers are handled for each OS type
#
SHOW VARIABLES LIKE 'lower_case_table_names';
Variable_name	Value
lower_case_table_names	0
CREATE SCHEMA MixedCaseSchema;
CREATE TABLE MixedCaseSchema.MixedCaseTable (MixedCaseColumn INT);
INSERT INTO MixedCaseSchema.MixedCaseTable (MixedCaseColumn) VALUES (1), (2);
SELECT * FROM MixedCaseSchema.MixedCaseTable;
MixedCaseColumn
1
2
USE MixedCaseSchema;
SHOW TABLES;
Tables_in_MixedCaseSchema
MixedCaseTable
USE test;
SHOW CREATE TABLE MixedCaseSchema.MixedCaseTable;
Table	Create Table
MixedCaseTable	CREATE TABLE `MixedCaseTable` (
  `MixedCaseColumn` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SELECT TABLE_NAME, TABLE_SCHEMA, TABLE_TYPE, ENGINE FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_NAME LIKE '%ixed%';
TABLE_NAME	TABLE_SCHEMA	TABLE_TYPE	ENGINE
MixedCaseTable	MixedCaseSchema	BASE TABLE	InnoDB
SELECT FILE_NAME, FILE_TYPE, TABLESPACE_NAME, TABLE_SCHEMA, TABLE_NAME FROM INFORMATION_SCHEMA.FILES WHERE FILE_NAME LIKE '%ixed%';
FILE_NAME	FILE_TYPE	TABLESPACE_NAME	TABLE_SCHEMA	TABLE_NAME
./MixedCaseSchema/MixedCaseTable.ibd	TABLESPACE	MixedCaseSchema/MixedCaseTable	NULL	NULL
SELECT NAME, SPACE_TYPE FROM INFORMATION_SCHEMA.INNODB_TABLES WHERE NAME LIKE '%ixed%';
NAME	SPACE_TYPE
MixedCaseSchema/MixedCaseTable	Single
SELECT NAME, SPACE_TYPE FROM INFORMATION_SCHEMA.INNODB_TABLESPACES WHERE NAME LIKE '%ixed%';
NAME	SPACE_TYPE
MixedCaseSchema/MixedCaseTable	Single
# restart
INSERT INTO MixedCaseSchema.MixedCaseTable (MixedCaseColumn) VALUES (3), (4);
SELECT * FROM MixedCaseSchema.MixedCaseTable;
MixedCaseColumn
1
2
3
4
USE MixedCaseSchema;
SHOW TABLES;
Tables_in_MixedCaseSchema
MixedCaseTable
USE test;
SHOW CREATE TABLE MixedCaseSchema.MixedCaseTable;
Table	Create Table
MixedCaseTable	CREATE TABLE `MixedCaseTable` (
  `MixedCaseColumn` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SELECT TABLE_NAME, TABLE_SCHEMA, TABLE_TYPE, ENGINE FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_NAME LIKE '%ixed%';
TABLE_NAME	TABLE_SCHEMA	TABLE_TYPE	ENGINE
MixedCaseTable	MixedCaseSchema	BASE TABLE	InnoDB
SELECT FILE_NAME, FILE_TYPE, TABLESPACE_NAME, TABLE_SCHEMA, TABLE_NAME FROM INFORMATION_SCHEMA.FILES WHERE FILE_NAME LIKE '%ixed%';
FILE_NAME	FILE_TYPE	TABLESPACE_NAME	TABLE_SCHEMA	TABLE_NAME
./MixedCaseSchema/MixedCaseTable.ibd	TABLESPACE	MixedCaseSchema/MixedCaseTable	NULL	NULL
SELECT NAME, SPACE_TYPE FROM INFORMATION_SCHEMA.INNODB_TABLES WHERE NAME LIKE '%ixed%';
NAME	SPACE_TYPE
MixedCaseSchema/MixedCaseTable	Single
SELECT NAME, SPACE_TYPE FROM INFORMATION_SCHEMA.INNODB_TABLESPACES WHERE NAME LIKE '%ixed%';
NAME	SPACE_TYPE
MixedCaseSchema/MixedCaseTable	Single
DROP TABLE MixedCaseSchema.MixedCaseTable;
DROP SCHEMA MixedCaseSchema;
