#
# CREATE TABLESPACE related tests for 32k page sizes.
#
SET DEFAULT_STORAGE_ENGINE=InnoDB;
# Strict-mode has no effect on CREATE TABLESPACE. But it does affect
# whether an invalid KEY_BLOCK_SIZE is rejected or adjusted.
SHOW VARIABLES LIKE 'innodb_strict_mode';
Variable_name	Value
innodb_strict_mode	ON
SHOW VARIABLES LIKE 'innodb_file_per_table';
Variable_name	Value
innodb_file_per_table	ON
#
# Create a tablespace with compressed page sizes that can match
# innodb-page-size.
#
CREATE TABLESPACE s_1k ADD DATAFILE 's_1k.ibd' FILE_BLOCK_SIZE=1k;
ERROR HY000: InnoDB: Cannot create a COMPRESSED tablespace when innodb_page_size > 16k.
SHOW WARNINGS;
Level	Code	Message
Error	1478	InnoDB: Cannot create a COMPRESSED tablespace when innodb_page_size > 16k.
Error	1528	Failed to create TABLESPACE s_1k
Error	1031	Table storage engine for 's_1k' doesn't have this option
CREATE TABLESPACE s_2k ADD DATAFILE 's_2k.ibd' FILE_BLOCK_SIZE=2k;
ERROR HY000: InnoDB: Cannot create a COMPRESSED tablespace when innodb_page_size > 16k.
SHOW WARNINGS;
Level	Code	Message
Error	1478	InnoDB: Cannot create a COMPRESSED tablespace when innodb_page_size > 16k.
Error	1528	Failed to create TABLESPACE s_2k
Error	1031	Table storage engine for 's_2k' doesn't have this option
CREATE TABLESPACE s_4k ADD DATAFILE 's_4k.ibd' FILE_BLOCK_SIZE=4k;
ERROR HY000: InnoDB: Cannot create a COMPRESSED tablespace when innodb_page_size > 16k.
SHOW WARNINGS;
Level	Code	Message
Error	1478	InnoDB: Cannot create a COMPRESSED tablespace when innodb_page_size > 16k.
Error	1528	Failed to create TABLESPACE s_4k
Error	1031	Table storage engine for 's_4k' doesn't have this option
CREATE TABLESPACE s_8k ADD DATAFILE 's_8k.ibd' FILE_BLOCK_SIZE=8k;
ERROR HY000: InnoDB: Cannot create a COMPRESSED tablespace when innodb_page_size > 16k.
SHOW WARNINGS;
Level	Code	Message
Error	1478	InnoDB: Cannot create a COMPRESSED tablespace when innodb_page_size > 16k.
Error	1528	Failed to create TABLESPACE s_8k
Error	1031	Table storage engine for 's_8k' doesn't have this option
CREATE TABLESPACE s_16k ADD DATAFILE 's_16k.ibd' FILE_BLOCK_SIZE=16k;
ERROR HY000: InnoDB: Cannot create a COMPRESSED tablespace when innodb_page_size > 16k.
SHOW WARNINGS;
Level	Code	Message
Error	1478	InnoDB: Cannot create a COMPRESSED tablespace when innodb_page_size > 16k.
Error	1528	Failed to create TABLESPACE s_16k
Error	1031	Table storage engine for 's_16k' doesn't have this option
CREATE TABLESPACE s_32k ADD DATAFILE 's_32k.ibd' FILE_BLOCK_SIZE=32k;
CREATE TABLESPACE s_64k ADD DATAFILE 's_64k.ibd' FILE_BLOCK_SIZE=64k;
ERROR HY000: InnoDB: Cannot create a tablespace with FILE_BLOCK_SIZE=65536 because INNODB_PAGE_SIZE=32768.
SHOW WARNINGS;
Level	Code	Message
Error	1478	InnoDB: Cannot create a tablespace with FILE_BLOCK_SIZE=65536 because INNODB_PAGE_SIZE=32768.
Error	1528	Failed to create TABLESPACE s_64k
Error	1031	Table storage engine for 's_64k' doesn't have this option
=== information_schema.innodb_tablespaces and innodb_datafiles ===
Space_Name	Space_Type	Page_Size	Zip_Size	Formats_Permitted	Path
mtr/global_suppressions	Single	DEFAULT	0	Dynamic	MYSQLD_DATADIR/mtr/global_suppressions.ibd
mtr/test_suppressions	Single	DEFAULT	0	Dynamic	MYSQLD_DATADIR/mtr/test_suppressions.ibd
s_32k	General	DEFAULT	0	Any	s_32k.ibd
=== information_schema.files ===
Space_Name	File_Type	Engine	Status	Tablespace_Name	Path
mtr/global_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/global_suppressions	MYSQLD_DATADIR/mtr/global_suppressions.ibd
mtr/test_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/test_suppressions	MYSQLD_DATADIR/mtr/test_suppressions.ibd
s_32k	TABLESPACE	InnoDB	NORMAL	s_32k	MYSQLD_DATADIR/s_32k.ibd
#
# Add tables to the tablespaces.
#
CREATE TABLE t_zip1k_in_321k (a int, b text) ROW_FORMAT=Compressed KEY_BLOCK_SIZE=1 TABLESPACE s_32k;
ERROR HY000: InnoDB: Tablespace `s_32k` cannot contain a COMPRESSED table
SHOW WARNINGS;
Level	Code	Message
Error	1478	InnoDB: Tablespace `s_32k` cannot contain a COMPRESSED table
Error	1031	Table storage engine for 't_zip1k_in_321k' doesn't have this option
CREATE TABLE t_zip2k_in_32k (a int, b text) ROW_FORMAT=Compressed KEY_BLOCK_SIZE=2 TABLESPACE s_32k;
ERROR HY000: InnoDB: Tablespace `s_32k` cannot contain a COMPRESSED table
SHOW WARNINGS;
Level	Code	Message
Error	1478	InnoDB: Tablespace `s_32k` cannot contain a COMPRESSED table
Error	1031	Table storage engine for 't_zip2k_in_32k' doesn't have this option
CREATE TABLE t_zip4k_in_32k (a int, b text) ROW_FORMAT=Compressed KEY_BLOCK_SIZE=4 TABLESPACE s_32k;
ERROR HY000: InnoDB: Tablespace `s_32k` cannot contain a COMPRESSED table
SHOW WARNINGS;
Level	Code	Message
Error	1478	InnoDB: Tablespace `s_32k` cannot contain a COMPRESSED table
Error	1031	Table storage engine for 't_zip4k_in_32k' doesn't have this option
CREATE TABLE t_zip8k_in_32k (a int, b text) ROW_FORMAT=Compressed KEY_BLOCK_SIZE=8 TABLESPACE s_32k;
ERROR HY000: InnoDB: Tablespace `s_32k` cannot contain a COMPRESSED table
SHOW WARNINGS;
Level	Code	Message
Error	1478	InnoDB: Tablespace `s_32k` cannot contain a COMPRESSED table
Error	1031	Table storage engine for 't_zip8k_in_32k' doesn't have this option
CREATE TABLE t_zip16k_in_32k (a int, b text) ROW_FORMAT=Compressed KEY_BLOCK_SIZE=16 TABLESPACE s_32k;
ERROR HY000: InnoDB: Tablespace `s_32k` cannot contain a COMPRESSED table
SHOW WARNINGS;
Level	Code	Message
Error	1478	InnoDB: Tablespace `s_32k` cannot contain a COMPRESSED table
Error	1031	Table storage engine for 't_zip16k_in_32k' doesn't have this option
CREATE TABLE t_zip32k_in_32k (a int, b text) ROW_FORMAT=Compressed KEY_BLOCK_SIZE=32 TABLESPACE s_32k;
ERROR HY000: InnoDB: Tablespace `s_32k` cannot contain a COMPRESSED table
SHOW WARNINGS;
Level	Code	Message
Error	1478	InnoDB: Tablespace `s_32k` cannot contain a COMPRESSED table
Error	1031	Table storage engine for 't_zip32k_in_32k' doesn't have this option
CREATE TABLE t_red_in_32k (a int, b text) ROW_FORMAT=redundant TABLESPACE s_32k;
CREATE TABLE t_com_in_32k (a int, b text) ROW_FORMAT=compact TABLESPACE s_32k;
CREATE TABLE t_dyn_in_32k (a int, b text) ROW_FORMAT=dynamic TABLESPACE s_32k;
# Add data to the existing Tables
INSERT INTO t_red_in_32k VALUES (1,'a'),(2,'b'),(3,'c');
INSERT INTO t_com_in_32k VALUES (1,'a'),(2,'b'),(3,'c');
INSERT INTO t_dyn_in_32k VALUES (1,'a'),(2,'b'),(3,'c');
# Restart mysqld
# restart
#
# Try to drop a tablespace which is not empty
#
DROP TABLESPACE s_32k;
ERROR HY000: Tablespace `s_32k` is not empty.
#
# Add more data to the existing Tables
#
INSERT INTO t_red_in_32k VALUES (4,'d');
INSERT INTO t_com_in_32k VALUES (4,'d');
INSERT INTO t_dyn_in_32k VALUES (4,'d');
#
# Restart the server and make sure that everything is OK.
#
# restart
=== information_schema.innodb_tablespaces and innodb_datafiles ===
Space_Name	Space_Type	Page_Size	Zip_Size	Formats_Permitted	Path
mtr/global_suppressions	Single	DEFAULT	0	Dynamic	MYSQLD_DATADIR/mtr/global_suppressions.ibd
mtr/test_suppressions	Single	DEFAULT	0	Dynamic	MYSQLD_DATADIR/mtr/test_suppressions.ibd
s_32k	General	DEFAULT	0	Any	s_32k.ibd
=== information_schema.files ===
Space_Name	File_Type	Engine	Status	Tablespace_Name	Path
mtr/global_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/global_suppressions	MYSQLD_DATADIR/mtr/global_suppressions.ibd
mtr/test_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/test_suppressions	MYSQLD_DATADIR/mtr/test_suppressions.ibd
s_32k	TABLESPACE	InnoDB	NORMAL	s_32k	MYSQLD_DATADIR/s_32k.ibd
=== information_schema.innodb_tables  and innodb_tablespaces ===
Table Name	Tablespace	Table Flags	Columns	Row Format	Zip Size	Space Type
mtr/global_suppressions	mtr/global_suppressions	33	4	Dynamic	0	Single
mtr/test_suppressions	mtr/test_suppressions	33	4	Dynamic	0	Single
test/t_com_in_32k	s_32k	129	5	Compact	0	General
test/t_dyn_in_32k	s_32k	161	5	Dynamic	0	General
test/t_red_in_32k	s_32k	128	5	Redundant	0	General
# Directory of MYSQLD_DATADIR/
mysql.ibd
s_32k.ibd
# Directory of MYSQLD_DATADIR/test/
#
# Clean-up.
#
CHECK TABLE t_red_in_32k;
Table	Op	Msg_type	Msg_text
test.t_red_in_32k	check	status	OK
CHECK TABLE t_com_in_32k;
Table	Op	Msg_type	Msg_text
test.t_com_in_32k	check	status	OK
CHECK TABLE t_dyn_in_32k;
Table	Op	Msg_type	Msg_text
test.t_dyn_in_32k	check	status	OK
DROP TABLE t_red_in_32k;
DROP TABLE t_com_in_32k;
DROP TABLE t_dyn_in_32k;
DROP TABLESPACE s_32k;
