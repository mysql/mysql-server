#
# A series of tests to show the correct file-per-table tablespace behavior.
# See also; parts.partition_basic_symlink_innodb.test for
# partition related tests with remote tablespaces.
# See innodb.innodb-restart for tablespace migration tests.
#
SET default_storage_engine=InnoDB;
#
# TABLESPACE related tests.
#
#
# CREATE TABLE ... INDEX DIRECTORY
# In non-strict mode INDEX DIRECTORY is ignored.
# In strict mode INDEX DIRECTORY is rejected.
#
SET SESSION innodb_strict_mode = OFF;
CREATE TABLE t1 (a int KEY, b text) INDEX DIRECTORY='MYSQL_TMP_DIR/alt_dir';
Warnings:
Warning	1618	<INDEX DIRECTORY> option ignored
SHOW WARNINGS;
Level	Code	Message
Warning	1618	<INDEX DIRECTORY> option ignored
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` int NOT NULL,
  `b` text,
  PRIMARY KEY (`a`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
DROP TABLE t1;
SET SESSION innodb_strict_mode = ON;
CREATE TABLE t1 (a int KEY, b text) INDEX DIRECTORY='MYSQL_TMP_DIR/alt_dir';
ERROR HY000: Table storage engine for 't1' doesn't have this option
SHOW WARNINGS;
Level	Code	Message
Warning	1478	InnoDB: INDEX DIRECTORY is not supported
Error	1031	Table storage engine for 't1' doesn't have this option
#
# Innodb does not support DATA DIRECTORY without innodb_file_per_table=ON.
# In non-strict mode DATA DIRECTORY is ignored.
# In strict mode DATA DIRECTORY without innodb_file_per_table=ON is rejected.
# TABLESPACE='innodb_file_per_table' overrides the innodb_file_per_table=OFF setting.
#
SET GLOBAL innodb_file_per_table=OFF;
SET SESSION innodb_strict_mode = OFF;
CREATE TABLE t1 (a int KEY, b text) DATA DIRECTORY='MYSQL_TMP_DIR/alt_dir';
Warnings:
Warning	1478	InnoDB: DATA DIRECTORY requires innodb_file_per_table.
Warning	1618	<DATA DIRECTORY> option ignored
SHOW WARNINGS;
Level	Code	Message
Warning	1478	InnoDB: DATA DIRECTORY requires innodb_file_per_table.
Warning	1618	<DATA DIRECTORY> option ignored
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` int NOT NULL,
  `b` text,
  PRIMARY KEY (`a`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
DROP TABLE t1;
SET SESSION innodb_strict_mode = ON;
CREATE TABLE t1 (a int KEY, b text) DATA DIRECTORY='MYSQL_TMP_DIR/alt_dir';
ERROR HY000: Table storage engine for 't1' doesn't have this option
SHOW WARNINGS;
Level	Code	Message
Warning	1478	InnoDB: DATA DIRECTORY requires innodb_file_per_table.
Error	1031	Table storage engine for 't1' doesn't have this option
CREATE TABLE t1 (a int KEY, b text) TABLESPACE=`innodb_file_per_table` DATA DIRECTORY='MYSQL_TMP_DIR/alt_dir';
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` int NOT NULL,
  `b` text,
  PRIMARY KEY (`a`)
) /*!50100 TABLESPACE `innodb_file_per_table` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci DATA DIRECTORY='MYSQL_TMP_DIR/alt_dir/'
DROP TABLE t1;
# Now set innodb_file_per_table so that DATA DIRECTORY can be tested.
SET GLOBAL innodb_file_per_table=ON;
#
# Create the tablespace in MYSQL_TMP_DIR/alt_dir
# InnoDB will create the sub-directories if needed.
#
CREATE TABLE t1 (a int KEY, b text) DATA DIRECTORY='MYSQL_TMP_DIR/alt_dir';
SHOW WARNINGS;
Level	Code	Message
INSERT INTO t1 VALUES (1, 'Create the tablespace');
SELECT * FROM t1;
a	b
1	Create the tablespace
#
# Check if link file exists  in MYSQLD_DATADIR
#
---- MYSQLD_DATADIR/test
# Check if tablespace file exists where we specified in DATA DIRECTORY
---- MYSQL_TMP_DIR/alt_dir/test
t1.ibd
#
# Check that DATA DIRECTORY shows up in the SHOW CREATE TABLE results.
#
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` int NOT NULL,
  `b` text,
  PRIMARY KEY (`a`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci DATA DIRECTORY='MYSQL_TMP_DIR/alt_dir/'
# Show that the system tables have this tablespace in them correctly
=== information_schema.innodb_tables  and innodb_tablespaces ===
Table Name	Tablespace	Table Flags	Columns	Row Format	Zip Size	Space Type
mtr/asserted_test_suppressions	mtr/asserted_test_suppressions	33	4	Dynamic	0	Single
mtr/global_suppressions	mtr/global_suppressions	33	4	Dynamic	0	Single
mtr/test_ignored_global_suppressions	mtr/test_ignored_global_suppressions	33	4	Dynamic	0	Single
mtr/test_suppressions	mtr/test_suppressions	33	4	Dynamic	0	Single
test/t1	test/t1	97	5	Dynamic	0	Single
=== information_schema.innodb_tablespaces and innodb_datafiles ===
Space_Name	Space_Type	Page_Size	Zip_Size	BlockSize!=0	FileSize!=0	Formats_Permitted	Path
mtr/asserted_test_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/asserted_test_suppressions.ibd
mtr/global_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/global_suppressions.ibd
mtr/test_ignored_global_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/test_ignored_global_suppressions.ibd
mtr/test_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/test_suppressions.ibd
test/t1	Single	DEFAULT	0	1	1	Dynamic	MYSQL_TMP_DIR/alt_dir/test/t1.ibd
=== information_schema.files ===
Space_Name	File_Type	Engine	Status	Tablespace_Name	Path
mtr/asserted_test_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/asserted_test_suppressions	MYSQLD_DATADIR/mtr/asserted_test_suppressions.ibd
mtr/global_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/global_suppressions	MYSQLD_DATADIR/mtr/global_suppressions.ibd
mtr/test_ignored_global_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/test_ignored_global_suppressions	MYSQLD_DATADIR/mtr/test_ignored_global_suppressions.ibd
mtr/test_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/test_suppressions	MYSQLD_DATADIR/mtr/test_suppressions.ibd
test/t1	TABLESPACE	InnoDB	NORMAL	test/t1	MYSQL_TMP_DIR/alt_dir/test/t1.ibd
#
# Show that the system tables are updated on drop table
#
DROP TABLE t1;
=== information_schema.innodb_tables  and innodb_tablespaces ===
Table Name	Tablespace	Table Flags	Columns	Row Format	Zip Size	Space Type
mtr/asserted_test_suppressions	mtr/asserted_test_suppressions	33	4	Dynamic	0	Single
mtr/global_suppressions	mtr/global_suppressions	33	4	Dynamic	0	Single
mtr/test_ignored_global_suppressions	mtr/test_ignored_global_suppressions	33	4	Dynamic	0	Single
mtr/test_suppressions	mtr/test_suppressions	33	4	Dynamic	0	Single
=== information_schema.innodb_tablespaces and innodb_datafiles ===
Space_Name	Space_Type	Page_Size	Zip_Size	BlockSize!=0	FileSize!=0	Formats_Permitted	Path
mtr/asserted_test_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/asserted_test_suppressions.ibd
mtr/global_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/global_suppressions.ibd
mtr/test_ignored_global_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/test_ignored_global_suppressions.ibd
mtr/test_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/test_suppressions.ibd
=== information_schema.files ===
Space_Name	File_Type	Engine	Status	Tablespace_Name	Path
mtr/asserted_test_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/asserted_test_suppressions	MYSQLD_DATADIR/mtr/asserted_test_suppressions.ibd
mtr/global_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/global_suppressions	MYSQLD_DATADIR/mtr/global_suppressions.ibd
mtr/test_ignored_global_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/test_ignored_global_suppressions	MYSQLD_DATADIR/mtr/test_ignored_global_suppressions.ibd
mtr/test_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/test_suppressions	MYSQLD_DATADIR/mtr/test_suppressions.ibd
#
# Create the same table a second time in the same place
#
CREATE TABLE t1 (a int KEY, b text) DATA DIRECTORY='MYSQL_TMP_DIR/alt_dir';
INSERT INTO t1 VALUES
(2, 'Create the same table a second time in the same place');
SELECT * FROM t1;
a	b
2	Create the same table a second time in the same place
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` int NOT NULL,
  `b` text,
  PRIMARY KEY (`a`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci DATA DIRECTORY='MYSQL_TMP_DIR/alt_dir/'
=== information_schema.innodb_tables  and innodb_tablespaces ===
Table Name	Tablespace	Table Flags	Columns	Row Format	Zip Size	Space Type
mtr/asserted_test_suppressions	mtr/asserted_test_suppressions	33	4	Dynamic	0	Single
mtr/global_suppressions	mtr/global_suppressions	33	4	Dynamic	0	Single
mtr/test_ignored_global_suppressions	mtr/test_ignored_global_suppressions	33	4	Dynamic	0	Single
mtr/test_suppressions	mtr/test_suppressions	33	4	Dynamic	0	Single
test/t1	test/t1	97	5	Dynamic	0	Single
=== information_schema.innodb_tablespaces and innodb_datafiles ===
Space_Name	Space_Type	Page_Size	Zip_Size	BlockSize!=0	FileSize!=0	Formats_Permitted	Path
mtr/asserted_test_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/asserted_test_suppressions.ibd
mtr/global_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/global_suppressions.ibd
mtr/test_ignored_global_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/test_ignored_global_suppressions.ibd
mtr/test_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/test_suppressions.ibd
test/t1	Single	DEFAULT	0	1	1	Dynamic	MYSQL_TMP_DIR/alt_dir/test/t1.ibd
=== information_schema.files ===
Space_Name	File_Type	Engine	Status	Tablespace_Name	Path
mtr/asserted_test_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/asserted_test_suppressions	MYSQLD_DATADIR/mtr/asserted_test_suppressions.ibd
mtr/global_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/global_suppressions	MYSQLD_DATADIR/mtr/global_suppressions.ibd
mtr/test_ignored_global_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/test_ignored_global_suppressions	MYSQLD_DATADIR/mtr/test_ignored_global_suppressions.ibd
mtr/test_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/test_suppressions	MYSQLD_DATADIR/mtr/test_suppressions.ibd
test/t1	TABLESPACE	InnoDB	NORMAL	test/t1	MYSQL_TMP_DIR/alt_dir/test/t1.ibd
---- MYSQLD_DATADIR/test
---- MYSQL_TMP_DIR/alt_dir/test
t1.ibd
#
# Truncate the table, then insert and verify
#
TRUNCATE TABLE t1;
INSERT INTO t1 VALUES (3, 'Truncate the table, then insert');
SELECT * FROM t1;
a	b
3	Truncate the table, then insert
=== information_schema.innodb_tables  and innodb_tablespaces ===
Table Name	Tablespace	Table Flags	Columns	Row Format	Zip Size	Space Type
mtr/asserted_test_suppressions	mtr/asserted_test_suppressions	33	4	Dynamic	0	Single
mtr/global_suppressions	mtr/global_suppressions	33	4	Dynamic	0	Single
mtr/test_ignored_global_suppressions	mtr/test_ignored_global_suppressions	33	4	Dynamic	0	Single
mtr/test_suppressions	mtr/test_suppressions	33	4	Dynamic	0	Single
test/t1	test/t1	97	5	Dynamic	0	Single
=== information_schema.innodb_tablespaces and innodb_datafiles ===
Space_Name	Space_Type	Page_Size	Zip_Size	BlockSize!=0	FileSize!=0	Formats_Permitted	Path
mtr/asserted_test_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/asserted_test_suppressions.ibd
mtr/global_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/global_suppressions.ibd
mtr/test_ignored_global_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/test_ignored_global_suppressions.ibd
mtr/test_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/test_suppressions.ibd
test/t1	Single	DEFAULT	0	1	1	Dynamic	MYSQL_TMP_DIR/alt_dir/test/t1.ibd
=== information_schema.files ===
Space_Name	File_Type	Engine	Status	Tablespace_Name	Path
mtr/asserted_test_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/asserted_test_suppressions	MYSQLD_DATADIR/mtr/asserted_test_suppressions.ibd
mtr/global_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/global_suppressions	MYSQLD_DATADIR/mtr/global_suppressions.ibd
mtr/test_ignored_global_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/test_ignored_global_suppressions	MYSQLD_DATADIR/mtr/test_ignored_global_suppressions.ibd
mtr/test_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/test_suppressions	MYSQLD_DATADIR/mtr/test_suppressions.ibd
test/t1	TABLESPACE	InnoDB	NORMAL	test/t1	MYSQL_TMP_DIR/alt_dir/test/t1.ibd
---- MYSQLD_DATADIR/test
---- MYSQL_TMP_DIR/alt_dir/test
t1.ibd
#
# Rename the table, then insert and verify
#
RENAME TABLE t1 TO t2;
INSERT INTO t2 VALUES (4, 'Rename the table, then insert');
SELECT * FROM t2;
a	b
3	Truncate the table, then insert
4	Rename the table, then insert
=== information_schema.innodb_tables  and innodb_tablespaces ===
Table Name	Tablespace	Table Flags	Columns	Row Format	Zip Size	Space Type
mtr/asserted_test_suppressions	mtr/asserted_test_suppressions	33	4	Dynamic	0	Single
mtr/global_suppressions	mtr/global_suppressions	33	4	Dynamic	0	Single
mtr/test_ignored_global_suppressions	mtr/test_ignored_global_suppressions	33	4	Dynamic	0	Single
mtr/test_suppressions	mtr/test_suppressions	33	4	Dynamic	0	Single
test/t2	test/t2	97	5	Dynamic	0	Single
=== information_schema.innodb_tablespaces and innodb_datafiles ===
Space_Name	Space_Type	Page_Size	Zip_Size	BlockSize!=0	FileSize!=0	Formats_Permitted	Path
mtr/asserted_test_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/asserted_test_suppressions.ibd
mtr/global_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/global_suppressions.ibd
mtr/test_ignored_global_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/test_ignored_global_suppressions.ibd
mtr/test_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/test_suppressions.ibd
test/t2	Single	DEFAULT	0	1	1	Dynamic	MYSQL_TMP_DIR/alt_dir/test/t2.ibd
=== information_schema.files ===
Space_Name	File_Type	Engine	Status	Tablespace_Name	Path
mtr/asserted_test_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/asserted_test_suppressions	MYSQLD_DATADIR/mtr/asserted_test_suppressions.ibd
mtr/global_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/global_suppressions	MYSQLD_DATADIR/mtr/global_suppressions.ibd
mtr/test_ignored_global_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/test_ignored_global_suppressions	MYSQLD_DATADIR/mtr/test_ignored_global_suppressions.ibd
mtr/test_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/test_suppressions	MYSQLD_DATADIR/mtr/test_suppressions.ibd
test/t2	TABLESPACE	InnoDB	NORMAL	test/t2	MYSQL_TMP_DIR/alt_dir/test/t2.ibd
---- MYSQLD_DATADIR/test
---- MYSQL_TMP_DIR/alt_dir/test
t2.ibd
#
# CREATE TABLE LIKE does not retain DATA DIRECTORY automatically.
#
CREATE TABLE t3 LIKE t2;
INSERT INTO t3 VALUES (5, 'CREATE TABLE LIKE');
SELECT * FROM t3;
a	b
5	CREATE TABLE LIKE
=== information_schema.innodb_tables  and innodb_tablespaces ===
Table Name	Tablespace	Table Flags	Columns	Row Format	Zip Size	Space Type
mtr/asserted_test_suppressions	mtr/asserted_test_suppressions	33	4	Dynamic	0	Single
mtr/global_suppressions	mtr/global_suppressions	33	4	Dynamic	0	Single
mtr/test_ignored_global_suppressions	mtr/test_ignored_global_suppressions	33	4	Dynamic	0	Single
mtr/test_suppressions	mtr/test_suppressions	33	4	Dynamic	0	Single
test/t2	test/t2	97	5	Dynamic	0	Single
test/t3	test/t3	33	5	Dynamic	0	Single
=== information_schema.innodb_tablespaces and innodb_datafiles ===
Space_Name	Space_Type	Page_Size	Zip_Size	BlockSize!=0	FileSize!=0	Formats_Permitted	Path
mtr/asserted_test_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/asserted_test_suppressions.ibd
mtr/global_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/global_suppressions.ibd
mtr/test_ignored_global_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/test_ignored_global_suppressions.ibd
mtr/test_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/test_suppressions.ibd
test/t2	Single	DEFAULT	0	1	1	Dynamic	MYSQL_TMP_DIR/alt_dir/test/t2.ibd
test/t3	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/test/t3.ibd
=== information_schema.files ===
Space_Name	File_Type	Engine	Status	Tablespace_Name	Path
mtr/asserted_test_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/asserted_test_suppressions	MYSQLD_DATADIR/mtr/asserted_test_suppressions.ibd
mtr/global_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/global_suppressions	MYSQLD_DATADIR/mtr/global_suppressions.ibd
mtr/test_ignored_global_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/test_ignored_global_suppressions	MYSQLD_DATADIR/mtr/test_ignored_global_suppressions.ibd
mtr/test_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/test_suppressions	MYSQLD_DATADIR/mtr/test_suppressions.ibd
test/t2	TABLESPACE	InnoDB	NORMAL	test/t2	MYSQL_TMP_DIR/alt_dir/test/t2.ibd
test/t3	TABLESPACE	InnoDB	NORMAL	test/t3	MYSQLD_DATADIR/test/t3.ibd
---- MYSQLD_DATADIR/test
t3.ibd
#
# Now make sure the tables can be fully dropped.
#
DROP TABLE t2, t3;
=== information_schema.innodb_tables  and innodb_tablespaces ===
Table Name	Tablespace	Table Flags	Columns	Row Format	Zip Size	Space Type
mtr/asserted_test_suppressions	mtr/asserted_test_suppressions	33	4	Dynamic	0	Single
mtr/global_suppressions	mtr/global_suppressions	33	4	Dynamic	0	Single
mtr/test_ignored_global_suppressions	mtr/test_ignored_global_suppressions	33	4	Dynamic	0	Single
mtr/test_suppressions	mtr/test_suppressions	33	4	Dynamic	0	Single
=== information_schema.innodb_tablespaces and innodb_datafiles ===
Space_Name	Space_Type	Page_Size	Zip_Size	BlockSize!=0	FileSize!=0	Formats_Permitted	Path
mtr/asserted_test_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/asserted_test_suppressions.ibd
mtr/global_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/global_suppressions.ibd
mtr/test_ignored_global_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/test_ignored_global_suppressions.ibd
mtr/test_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/test_suppressions.ibd
=== information_schema.files ===
Space_Name	File_Type	Engine	Status	Tablespace_Name	Path
mtr/asserted_test_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/asserted_test_suppressions	MYSQLD_DATADIR/mtr/asserted_test_suppressions.ibd
mtr/global_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/global_suppressions	MYSQLD_DATADIR/mtr/global_suppressions.ibd
mtr/test_ignored_global_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/test_ignored_global_suppressions	MYSQLD_DATADIR/mtr/test_ignored_global_suppressions.ibd
mtr/test_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/test_suppressions	MYSQLD_DATADIR/mtr/test_suppressions.ibd
---- MYSQLD_DATADIR/test
---- MYSQL_TMP_DIR/alt_dir/test
#
# Be sure SQL MODE 'NO_DIR_IN_CREATE' prevents the use of DATA DIRECTORY
#
SET @org_mode=@@sql_mode;
SET @@sql_mode='NO_DIR_IN_CREATE';
SELECT @@sql_mode;
@@sql_mode
NO_DIR_IN_CREATE
CREATE TABLE t1 (a int, b text) DATA DIRECTORY='MYSQL_TMP_DIR/alt_dir';
Warnings:
Warning	1618	<DATA DIRECTORY> option ignored
SHOW WARNINGS;
Level	Code	Message
Warning	1618	<DATA DIRECTORY> option ignored
INSERT INTO t1 VALUES (6, 'SQL MODE NO_DIR_IN_CREATE prevents DATA DIRECTORY');
DROP TABLE t1;
set @@sql_mode=@org_mode;
#
# The server does not allow any DATA DIRECTORY name to be a relative path
# or to be located in or under MYSQLD_DATADIR. 
#
CREATE TABLE t1 (a int KEY, b text) DATA DIRECTORY './';
ERROR HY000: Incorrect path value: './'
SHOW WARNINGS;
Level	Code	Message
Error	1525	Incorrect path value: './'
CREATE TABLE t1 (a int KEY, b text) DATA DIRECTORY '../';
ERROR HY000: Incorrect path value: '../'
SHOW WARNINGS;
Level	Code	Message
Error	1525	Incorrect path value: '../'
CREATE TABLE t1 (a int KEY, b text) DATA DIRECTORY 'MYSQLD_DATADIR';
ERROR HY000: Incorrect arguments to DATA DIRECTORY
SHOW WARNINGS;
Level	Code	Message
Error	1210	Incorrect arguments to DATA DIRECTORY
CREATE TABLE t1 (a int KEY, b text) DATA DIRECTORY 'MYSQLD_DATADIR/under';
ERROR HY000: Incorrect arguments to DATA DIRECTORY
SHOW WARNINGS;
Level	Code	Message
Error	1210	Incorrect arguments to DATA DIRECTORY
#
# TEMPORARY tables are incompatible with DATA DIRECTORY
# In strict mode DATA DIRECTORY is rejected. In non-strict mode it
# is ignored.
#
SET SESSION innodb_strict_mode = OFF;
CREATE TEMPORARY TABLE t1 (a int KEY, b text) engine=InnoDB DATA DIRECTORY='MYSQL_TMP_DIR/alt_dir';
Warnings:
Warning	1478	InnoDB: DATA DIRECTORY cannot be used for TEMPORARY tables.
Warning	1618	<DATA DIRECTORY> option ignored
SHOW WARNINGS;
Level	Code	Message
Warning	1478	InnoDB: DATA DIRECTORY cannot be used for TEMPORARY tables.
Warning	1618	<DATA DIRECTORY> option ignored
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TEMPORARY TABLE `t1` (
  `a` int NOT NULL,
  `b` text,
  PRIMARY KEY (`a`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
DROP TABLE t1;
SET SESSION innodb_strict_mode = ON;
CREATE TEMPORARY TABLE t1 (a int KEY, b text) engine=InnoDB DATA DIRECTORY='MYSQL_TMP_DIR/alt_dir';
ERROR HY000: Table storage engine for 't1' doesn't have this option
SHOW WARNINGS;
Level	Code	Message
Warning	1478	InnoDB: DATA DIRECTORY cannot be used for TEMPORARY tables.
Error	1031	Table storage engine for 't1' doesn't have this option
#
# Create the remote table via static DDL statements in a stored procedure
#
CREATE PROCEDURE static_proc() BEGIN CREATE TABLE t1 (a int KEY, b text) DATA DIRECTORY='MYSQL_TMP_DIR/alt_dir'; END |
CALL static_proc;
=== information_schema.innodb_tablespaces and innodb_datafiles ===
Space_Name	Space_Type	Page_Size	Zip_Size	BlockSize!=0	FileSize!=0	Formats_Permitted	Path
mtr/asserted_test_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/asserted_test_suppressions.ibd
mtr/global_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/global_suppressions.ibd
mtr/test_ignored_global_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/test_ignored_global_suppressions.ibd
mtr/test_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/test_suppressions.ibd
test/t1	Single	DEFAULT	0	1	1	Dynamic	MYSQL_TMP_DIR/alt_dir/test/t1.ibd
=== information_schema.files ===
Space_Name	File_Type	Engine	Status	Tablespace_Name	Path
mtr/asserted_test_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/asserted_test_suppressions	MYSQLD_DATADIR/mtr/asserted_test_suppressions.ibd
mtr/global_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/global_suppressions	MYSQLD_DATADIR/mtr/global_suppressions.ibd
mtr/test_ignored_global_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/test_ignored_global_suppressions	MYSQLD_DATADIR/mtr/test_ignored_global_suppressions.ibd
mtr/test_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/test_suppressions	MYSQLD_DATADIR/mtr/test_suppressions.ibd
test/t1	TABLESPACE	InnoDB	NORMAL	test/t1	MYSQL_TMP_DIR/alt_dir/test/t1.ibd
INSERT INTO t1 VALUES (7, 'Create the remote table via static DDL statements');
SELECT * FROM t1;
a	b
7	Create the remote table via static DDL statements
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` int NOT NULL,
  `b` text,
  PRIMARY KEY (`a`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci DATA DIRECTORY='MYSQL_TMP_DIR/alt_dir/'
---- MYSQLD_DATADIR/test
---- MYSQL_TMP_DIR/alt_dir/test
t1.ibd
DROP PROCEDURE static_proc;
DROP TABLE t1;
#
# Create the remote table via dynamic DDL statements in a stored procedure
#
CREATE PROCEDURE dynamic_proc()
BEGIN
PREPARE stmt1 FROM
"CREATE TABLE t1 (a int KEY, b text) DATA DIRECTORY='MYSQL_TMP_DIR/alt_dir'";
EXECUTE stmt1;
END |
CALL dynamic_proc;
INSERT INTO t1 VALUES (8, 'Create the remote table via dynamic DDL statements');
SELECT * FROM t1;
a	b
8	Create the remote table via dynamic DDL statements
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` int NOT NULL,
  `b` text,
  PRIMARY KEY (`a`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci DATA DIRECTORY='MYSQL_TMP_DIR/alt_dir/'
=== information_schema.innodb_tablespaces and innodb_datafiles ===
Space_Name	Space_Type	Page_Size	Zip_Size	BlockSize!=0	FileSize!=0	Formats_Permitted	Path
mtr/asserted_test_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/asserted_test_suppressions.ibd
mtr/global_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/global_suppressions.ibd
mtr/test_ignored_global_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/test_ignored_global_suppressions.ibd
mtr/test_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/test_suppressions.ibd
test/t1	Single	DEFAULT	0	1	1	Dynamic	MYSQL_TMP_DIR/alt_dir/test/t1.ibd
=== information_schema.files ===
Space_Name	File_Type	Engine	Status	Tablespace_Name	Path
mtr/asserted_test_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/asserted_test_suppressions	MYSQLD_DATADIR/mtr/asserted_test_suppressions.ibd
mtr/global_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/global_suppressions	MYSQLD_DATADIR/mtr/global_suppressions.ibd
mtr/test_ignored_global_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/test_ignored_global_suppressions	MYSQLD_DATADIR/mtr/test_ignored_global_suppressions.ibd
mtr/test_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/test_suppressions	MYSQLD_DATADIR/mtr/test_suppressions.ibd
test/t1	TABLESPACE	InnoDB	NORMAL	test/t1	MYSQL_TMP_DIR/alt_dir/test/t1.ibd
---- MYSQLD_DATADIR/test
---- MYSQL_TMP_DIR/alt_dir/test
t1.ibd
DROP PROCEDURE dynamic_proc;
DEALLOCATE PREPARE stmt1;
DROP TABLE t1;
#
# CREATE, DROP, ADD and TRUNCATE PARTITION with DATA DIRECTORY
#
CREATE TABLE emp (
id INT NOT NULL,
store_name VARCHAR(30),
parts VARCHAR(30),
store_id INT
)
PARTITION BY LIST(store_id) (
PARTITION east VALUES IN (10,20,30)
DATA DIRECTORY = 'MYSQL_TMP_DIR/alt_dir_east',
PARTITION north VALUES IN (40,50,60)
DATA DIRECTORY = 'MYSQL_TMP_DIR/alt_dir_north',
PARTITION west VALUES IN (70,80,100)
DATA DIRECTORY = 'MYSQL_TMP_DIR/alt_dir_west'
);
INSERT INTO emp values(1,'Oracle','NUTT',10);
INSERT INTO emp values(2,'HUAWEI','BOLT',40);
INSERT INTO emp values(3,'IBM','NAIL',70);
SHOW CREATE TABLE emp;
Table	Create Table
emp	CREATE TABLE `emp` (
  `id` int NOT NULL,
  `store_name` varchar(30) DEFAULT NULL,
  `parts` varchar(30) DEFAULT NULL,
  `store_id` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
/*!50100 PARTITION BY LIST (`store_id`)
(PARTITION east VALUES IN (10,20,30) DATA DIRECTORY = 'MYSQL_TMP_DIR/alt_dir_east/' ENGINE = InnoDB,
 PARTITION north VALUES IN (40,50,60) DATA DIRECTORY = 'MYSQL_TMP_DIR/alt_dir_north/' ENGINE = InnoDB,
 PARTITION west VALUES IN (70,80,100) DATA DIRECTORY = 'MYSQL_TMP_DIR/alt_dir_west/' ENGINE = InnoDB) */
SELECT * FROM emp;
id	store_name	parts	store_id
1	Oracle	NUTT	10
2	HUAWEI	BOLT	40
3	IBM	NAIL	70
=== information_schema.innodb_tables  and innodb_tablespaces ===
Table Name	Tablespace	Table Flags	Columns	Row Format	Zip Size	Space Type
mtr/asserted_test_suppressions	mtr/asserted_test_suppressions	33	4	Dynamic	0	Single
mtr/global_suppressions	mtr/global_suppressions	33	4	Dynamic	0	Single
mtr/test_ignored_global_suppressions	mtr/test_ignored_global_suppressions	33	4	Dynamic	0	Single
mtr/test_suppressions	mtr/test_suppressions	33	4	Dynamic	0	Single
test/emp#p#east	test/emp#p#east	97	7	Dynamic	0	Single
test/emp#p#north	test/emp#p#north	97	7	Dynamic	0	Single
test/emp#p#west	test/emp#p#west	97	7	Dynamic	0	Single
=== information_schema.innodb_tablespaces and innodb_datafiles ===
Space_Name	Space_Type	Page_Size	Zip_Size	BlockSize!=0	FileSize!=0	Formats_Permitted	Path
mtr/asserted_test_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/asserted_test_suppressions.ibd
mtr/global_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/global_suppressions.ibd
mtr/test_ignored_global_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/test_ignored_global_suppressions.ibd
mtr/test_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/test_suppressions.ibd
test/emp#p#east	Single	DEFAULT	0	1	1	Dynamic	MYSQL_TMP_DIR/alt_dir_east/test/emp#p#east.ibd
test/emp#p#north	Single	DEFAULT	0	1	1	Dynamic	MYSQL_TMP_DIR/alt_dir_north/test/emp#p#north.ibd
test/emp#p#west	Single	DEFAULT	0	1	1	Dynamic	MYSQL_TMP_DIR/alt_dir_west/test/emp#p#west.ibd
=== information_schema.files ===
Space_Name	File_Type	Engine	Status	Tablespace_Name	Path
mtr/asserted_test_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/asserted_test_suppressions	MYSQLD_DATADIR/mtr/asserted_test_suppressions.ibd
mtr/global_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/global_suppressions	MYSQLD_DATADIR/mtr/global_suppressions.ibd
mtr/test_ignored_global_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/test_ignored_global_suppressions	MYSQLD_DATADIR/mtr/test_ignored_global_suppressions.ibd
mtr/test_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/test_suppressions	MYSQLD_DATADIR/mtr/test_suppressions.ibd
test/emp#p#east	TABLESPACE	InnoDB	NORMAL	test/emp#p#east	MYSQL_TMP_DIR/alt_dir_east/test/emp#p#east.ibd
test/emp#p#north	TABLESPACE	InnoDB	NORMAL	test/emp#p#north	MYSQL_TMP_DIR/alt_dir_north/test/emp#p#north.ibd
test/emp#p#west	TABLESPACE	InnoDB	NORMAL	test/emp#p#west	MYSQL_TMP_DIR/alt_dir_west/test/emp#p#west.ibd
---- MYSQLD_DATADIR/test
---- MYSQL_TMP_DIR/alt_dir_east/test
emp#p#east.ibd
---- MYSQL_TMP_DIR/alt_dir_north/test
emp#p#north.ibd
---- MYSQL_TMP_DIR/alt_dir_west/test
emp#p#west.ibd
#
# DROP one PARTITION.
#
ALTER TABLE emp DROP PARTITION west;
SHOW CREATE TABLE emp;
Table	Create Table
emp	CREATE TABLE `emp` (
  `id` int NOT NULL,
  `store_name` varchar(30) DEFAULT NULL,
  `parts` varchar(30) DEFAULT NULL,
  `store_id` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
/*!50100 PARTITION BY LIST (`store_id`)
(PARTITION east VALUES IN (10,20,30) DATA DIRECTORY = 'MYSQL_TMP_DIR/alt_dir_east' ENGINE = InnoDB,
 PARTITION north VALUES IN (40,50,60) DATA DIRECTORY = 'MYSQL_TMP_DIR/alt_dir_north' ENGINE = InnoDB) */
SELECT * FROM emp;
id	store_name	parts	store_id
1	Oracle	NUTT	10
2	HUAWEI	BOLT	40
=== information_schema.innodb_tables  and innodb_tablespaces ===
Table Name	Tablespace	Table Flags	Columns	Row Format	Zip Size	Space Type
mtr/asserted_test_suppressions	mtr/asserted_test_suppressions	33	4	Dynamic	0	Single
mtr/global_suppressions	mtr/global_suppressions	33	4	Dynamic	0	Single
mtr/test_ignored_global_suppressions	mtr/test_ignored_global_suppressions	33	4	Dynamic	0	Single
mtr/test_suppressions	mtr/test_suppressions	33	4	Dynamic	0	Single
test/emp#p#east	test/emp#p#east	97	7	Dynamic	0	Single
test/emp#p#north	test/emp#p#north	97	7	Dynamic	0	Single
=== information_schema.innodb_tablespaces and innodb_datafiles ===
Space_Name	Space_Type	Page_Size	Zip_Size	BlockSize!=0	FileSize!=0	Formats_Permitted	Path
mtr/asserted_test_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/asserted_test_suppressions.ibd
mtr/global_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/global_suppressions.ibd
mtr/test_ignored_global_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/test_ignored_global_suppressions.ibd
mtr/test_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/test_suppressions.ibd
test/emp#p#east	Single	DEFAULT	0	1	1	Dynamic	MYSQL_TMP_DIR/alt_dir_east/test/emp#p#east.ibd
test/emp#p#north	Single	DEFAULT	0	1	1	Dynamic	MYSQL_TMP_DIR/alt_dir_north/test/emp#p#north.ibd
=== information_schema.files ===
Space_Name	File_Type	Engine	Status	Tablespace_Name	Path
mtr/asserted_test_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/asserted_test_suppressions	MYSQLD_DATADIR/mtr/asserted_test_suppressions.ibd
mtr/global_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/global_suppressions	MYSQLD_DATADIR/mtr/global_suppressions.ibd
mtr/test_ignored_global_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/test_ignored_global_suppressions	MYSQLD_DATADIR/mtr/test_ignored_global_suppressions.ibd
mtr/test_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/test_suppressions	MYSQLD_DATADIR/mtr/test_suppressions.ibd
test/emp#p#east	TABLESPACE	InnoDB	NORMAL	test/emp#p#east	MYSQL_TMP_DIR/alt_dir_east/test/emp#p#east.ibd
test/emp#p#north	TABLESPACE	InnoDB	NORMAL	test/emp#p#north	MYSQL_TMP_DIR/alt_dir_north/test/emp#p#north.ibd
---- MYSQLD_DATADIR/test
---- MYSQL_TMP_DIR/alt_dir_east/test
emp#p#east.ibd
---- MYSQL_TMP_DIR/alt_dir_north/test
emp#p#north.ibd
---- MYSQL_TMP_DIR/alt_dir_west/test
#
# ADD the PARTITION back.
#
ALTER TABLE emp ADD PARTITION (
PARTITION west VALUES IN (70,80,100)
DATA DIRECTORY = 'MYSQL_TMP_DIR/alt_dir_west');
INSERT INTO emp VALUES(3,'IBM','NAIL',70);
SELECT * FROM emp;
id	store_name	parts	store_id
1	Oracle	NUTT	10
2	HUAWEI	BOLT	40
3	IBM	NAIL	70
=== information_schema.innodb_tables  and innodb_tablespaces ===
Table Name	Tablespace	Table Flags	Columns	Row Format	Zip Size	Space Type
mtr/asserted_test_suppressions	mtr/asserted_test_suppressions	33	4	Dynamic	0	Single
mtr/global_suppressions	mtr/global_suppressions	33	4	Dynamic	0	Single
mtr/test_ignored_global_suppressions	mtr/test_ignored_global_suppressions	33	4	Dynamic	0	Single
mtr/test_suppressions	mtr/test_suppressions	33	4	Dynamic	0	Single
test/emp#p#east	test/emp#p#east	97	7	Dynamic	0	Single
test/emp#p#north	test/emp#p#north	97	7	Dynamic	0	Single
test/emp#p#west	test/emp#p#west	97	7	Dynamic	0	Single
=== information_schema.innodb_tablespaces and innodb_datafiles ===
Space_Name	Space_Type	Page_Size	Zip_Size	BlockSize!=0	FileSize!=0	Formats_Permitted	Path
mtr/asserted_test_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/asserted_test_suppressions.ibd
mtr/global_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/global_suppressions.ibd
mtr/test_ignored_global_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/test_ignored_global_suppressions.ibd
mtr/test_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/test_suppressions.ibd
test/emp#p#east	Single	DEFAULT	0	1	1	Dynamic	MYSQL_TMP_DIR/alt_dir_east/test/emp#p#east.ibd
test/emp#p#north	Single	DEFAULT	0	1	1	Dynamic	MYSQL_TMP_DIR/alt_dir_north/test/emp#p#north.ibd
test/emp#p#west	Single	DEFAULT	0	1	1	Dynamic	MYSQL_TMP_DIR/alt_dir_west/test/emp#p#west.ibd
=== information_schema.files ===
Space_Name	File_Type	Engine	Status	Tablespace_Name	Path
mtr/asserted_test_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/asserted_test_suppressions	MYSQLD_DATADIR/mtr/asserted_test_suppressions.ibd
mtr/global_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/global_suppressions	MYSQLD_DATADIR/mtr/global_suppressions.ibd
mtr/test_ignored_global_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/test_ignored_global_suppressions	MYSQLD_DATADIR/mtr/test_ignored_global_suppressions.ibd
mtr/test_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/test_suppressions	MYSQLD_DATADIR/mtr/test_suppressions.ibd
test/emp#p#east	TABLESPACE	InnoDB	NORMAL	test/emp#p#east	MYSQL_TMP_DIR/alt_dir_east/test/emp#p#east.ibd
test/emp#p#north	TABLESPACE	InnoDB	NORMAL	test/emp#p#north	MYSQL_TMP_DIR/alt_dir_north/test/emp#p#north.ibd
test/emp#p#west	TABLESPACE	InnoDB	NORMAL	test/emp#p#west	MYSQL_TMP_DIR/alt_dir_west/test/emp#p#west.ibd
---- MYSQLD_DATADIR/test
---- MYSQL_TMP_DIR/alt_dir_east/test
emp#p#east.ibd
---- MYSQL_TMP_DIR/alt_dir_north/test
emp#p#north.ibd
---- MYSQL_TMP_DIR/alt_dir_west/test
emp#p#west.ibd
#
# TRUNCATE one PARTITION.
#
ALTER TABLE emp TRUNCATE PARTITION west;
SHOW CREATE TABLE emp;
Table	Create Table
emp	CREATE TABLE `emp` (
  `id` int NOT NULL,
  `store_name` varchar(30) DEFAULT NULL,
  `parts` varchar(30) DEFAULT NULL,
  `store_id` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
/*!50100 PARTITION BY LIST (`store_id`)
(PARTITION east VALUES IN (10,20,30) DATA DIRECTORY = 'MYSQL_TMP_DIR/alt_dir_east' ENGINE = InnoDB,
 PARTITION north VALUES IN (40,50,60) DATA DIRECTORY = 'MYSQL_TMP_DIR/alt_dir_north' ENGINE = InnoDB,
 PARTITION west VALUES IN (70,80,100) DATA DIRECTORY = 'MYSQL_TMP_DIR/alt_dir_west' ENGINE = InnoDB) */
INSERT INTO emp VALUES(3,'IBM','NAIL',70);
SELECT * FROM emp;
id	store_name	parts	store_id
1	Oracle	NUTT	10
2	HUAWEI	BOLT	40
3	IBM	NAIL	70
=== information_schema.innodb_tables  and innodb_tablespaces ===
Table Name	Tablespace	Table Flags	Columns	Row Format	Zip Size	Space Type
mtr/asserted_test_suppressions	mtr/asserted_test_suppressions	33	4	Dynamic	0	Single
mtr/global_suppressions	mtr/global_suppressions	33	4	Dynamic	0	Single
mtr/test_ignored_global_suppressions	mtr/test_ignored_global_suppressions	33	4	Dynamic	0	Single
mtr/test_suppressions	mtr/test_suppressions	33	4	Dynamic	0	Single
test/emp#p#east	test/emp#p#east	97	7	Dynamic	0	Single
test/emp#p#north	test/emp#p#north	97	7	Dynamic	0	Single
test/emp#p#west	test/emp#p#west	97	7	Dynamic	0	Single
=== information_schema.innodb_tablespaces and innodb_datafiles ===
Space_Name	Space_Type	Page_Size	Zip_Size	BlockSize!=0	FileSize!=0	Formats_Permitted	Path
mtr/asserted_test_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/asserted_test_suppressions.ibd
mtr/global_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/global_suppressions.ibd
mtr/test_ignored_global_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/test_ignored_global_suppressions.ibd
mtr/test_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/test_suppressions.ibd
test/emp#p#east	Single	DEFAULT	0	1	1	Dynamic	MYSQL_TMP_DIR/alt_dir_east/test/emp#p#east.ibd
test/emp#p#north	Single	DEFAULT	0	1	1	Dynamic	MYSQL_TMP_DIR/alt_dir_north/test/emp#p#north.ibd
test/emp#p#west	Single	DEFAULT	0	1	1	Dynamic	MYSQL_TMP_DIR/alt_dir_west/test/emp#p#west.ibd
=== information_schema.files ===
Space_Name	File_Type	Engine	Status	Tablespace_Name	Path
mtr/asserted_test_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/asserted_test_suppressions	MYSQLD_DATADIR/mtr/asserted_test_suppressions.ibd
mtr/global_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/global_suppressions	MYSQLD_DATADIR/mtr/global_suppressions.ibd
mtr/test_ignored_global_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/test_ignored_global_suppressions	MYSQLD_DATADIR/mtr/test_ignored_global_suppressions.ibd
mtr/test_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/test_suppressions	MYSQLD_DATADIR/mtr/test_suppressions.ibd
test/emp#p#east	TABLESPACE	InnoDB	NORMAL	test/emp#p#east	MYSQL_TMP_DIR/alt_dir_east/test/emp#p#east.ibd
test/emp#p#north	TABLESPACE	InnoDB	NORMAL	test/emp#p#north	MYSQL_TMP_DIR/alt_dir_north/test/emp#p#north.ibd
test/emp#p#west	TABLESPACE	InnoDB	NORMAL	test/emp#p#west	MYSQL_TMP_DIR/alt_dir_west/test/emp#p#west.ibd
---- MYSQLD_DATADIR/test
---- MYSQL_TMP_DIR/alt_dir_east/test
emp#p#east.ibd
---- MYSQL_TMP_DIR/alt_dir_north/test
emp#p#north.ibd
---- MYSQL_TMP_DIR/alt_dir_west/test
emp#p#west.ibd
DROP TABLE emp;
#
# Cleanup
#
