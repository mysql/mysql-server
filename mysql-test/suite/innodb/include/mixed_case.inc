
--echo #
--echo # Show how Mixed Case identifiers are handled for each OS type
--echo #

SHOW VARIABLES LIKE 'lower_case_table_names';

CREATE SCHEMA MixedCaseSchema;
CREATE TABLE MixedCaseSchema.MixedCaseTable (MixedCaseColumn INT);
INSERT INTO MixedCaseSchema.MixedCaseTable (MixedCaseColumn) VALUES (1), (2);

CREATE TABLE MixedCaseSchema.MixedCaseSubParititionedTable (col1 INT, col2 DATE)
    PARTITION BY RANGE(YEAR(col2))
    SUBPARTITION BY HASH(col1) (
        PARTITION MixedCaseParititionName0 VALUES LESS THAN (2000) (
            SUBPARTITION MixedCaseSubParititionName0,
            SUBPARTITION MixedCaseSubParititionName1
        ),
        PARTITION MixedCaseParititionName1 VALUES LESS THAN MAXVALUE (
            SUBPARTITION MixedCaseSubParititionName2,
            SUBPARTITION MixedCaseSubParititionName3
        )
    );
INSERT INTO MixedCaseSchema.MixedCaseSubParititionedTable VALUES (1, '2025-12-15'), (2001, '2025-12-15');

SELECT * FROM MixedCaseSchema.MixedCaseTable;
SELECT * FROM MixedCaseSchema.MixedCaseSubParititionedTable;

USE MixedCaseSchema;
SHOW TABLES;
USE test;

SHOW CREATE TABLE MixedCaseSchema.MixedCaseTable;

--sorted_result
SELECT TABLE_NAME, TABLE_SCHEMA, TABLE_TYPE, ENGINE FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_NAME LIKE '%ixed%';
--sorted_result
SELECT FILE_NAME, FILE_TYPE, TABLESPACE_NAME, TABLE_SCHEMA, TABLE_NAME FROM INFORMATION_SCHEMA.FILES WHERE FILE_NAME LIKE '%ixed%';
--sorted_result
SELECT NAME, SPACE_TYPE FROM INFORMATION_SCHEMA.INNODB_TABLES WHERE NAME LIKE '%ixed%';
--sorted_result
SELECT NAME, SPACE_TYPE FROM INFORMATION_SCHEMA.INNODB_TABLESPACES WHERE NAME LIKE '%ixed%';

--source include/restart_mysqld.inc

INSERT INTO MixedCaseSchema.MixedCaseTable (MixedCaseColumn) VALUES (3), (4);
INSERT INTO MixedCaseSchema.MixedCaseSubParititionedTable VALUES (2, '2025-12-16'), (2002, '2025-12-16');
SELECT * FROM MixedCaseSchema.MixedCaseTable;
SELECT * FROM MixedCaseSchema.MixedCaseSubParititionedTable;

USE MixedCaseSchema;
SHOW TABLES;
USE test;

SHOW CREATE TABLE MixedCaseSchema.MixedCaseTable;

--sorted_result
SELECT TABLE_NAME, TABLE_SCHEMA, TABLE_TYPE, ENGINE FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_NAME LIKE '%ixed%';
--sorted_result
SELECT FILE_NAME, FILE_TYPE, TABLESPACE_NAME, TABLE_SCHEMA, TABLE_NAME FROM INFORMATION_SCHEMA.FILES WHERE FILE_NAME LIKE '%ixed%';
--sorted_result
SELECT NAME, SPACE_TYPE FROM INFORMATION_SCHEMA.INNODB_TABLES WHERE NAME LIKE '%ixed%';
--sorted_result
SELECT NAME, SPACE_TYPE FROM INFORMATION_SCHEMA.INNODB_TABLESPACES WHERE NAME LIKE '%ixed%';

DROP TABLE MixedCaseSchema.MixedCaseTable;
DROP TABLE MixedCaseSchema.MixedCaseSubParititionedTable;
DROP SCHEMA MixedCaseSchema;
