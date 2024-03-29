#
# Bug #29793800 ASSERT AFTER DISCARD TABLESPACE, RENAME TABLE AND
# CREATE TABLE USING SAME NAME.
#
# Test Case 1:
CREATE TABLE t1(b INT)ENGINE=innodb;
=== information_schema.innodb_tables  and innodb_tablespaces ===
Table Name	Tablespace	Table Flags	Columns	Row Format	Zip Size	Space Type
mtr/asserted_test_suppressions	mtr/asserted_test_suppressions	33	4	Dynamic	0	Single
mtr/global_suppressions	mtr/global_suppressions	33	4	Dynamic	0	Single
mtr/test_ignored_global_suppressions	mtr/test_ignored_global_suppressions	33	4	Dynamic	0	Single
mtr/test_suppressions	mtr/test_suppressions	33	4	Dynamic	0	Single
test/t1	test/t1	33	4	Dynamic	0	Single
=== information_schema.innodb_tablespaces and innodb_datafiles ===
Space_Name	Space_Type	Page_Size	Zip_Size	BlockSize!=0	FileSize!=0	Formats_Permitted	Path
mtr/asserted_test_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/asserted_test_suppressions.ibd
mtr/global_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/global_suppressions.ibd
mtr/test_ignored_global_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/test_ignored_global_suppressions.ibd
mtr/test_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/test_suppressions.ibd
test/t1	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/test/t1.ibd
=== information_schema.files ===
Space_Name	File_Type	Engine	Status	Tablespace_Name	Path
mtr/asserted_test_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/asserted_test_suppressions	MYSQLD_DATADIR/mtr/asserted_test_suppressions.ibd
mtr/global_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/global_suppressions	MYSQLD_DATADIR/mtr/global_suppressions.ibd
mtr/test_ignored_global_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/test_ignored_global_suppressions	MYSQLD_DATADIR/mtr/test_ignored_global_suppressions.ibd
mtr/test_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/test_suppressions	MYSQLD_DATADIR/mtr/test_suppressions.ibd
test/t1	TABLESPACE	InnoDB	NORMAL	test/t1	MYSQLD_DATADIR/test/t1.ibd
ALTER TABLE t1 DISCARD TABLESPACE;
=== information_schema.innodb_tables  and innodb_tablespaces ===
Table Name	Tablespace	Table Flags	Columns	Row Format	Zip Size	Space Type
mtr/asserted_test_suppressions	mtr/asserted_test_suppressions	33	4	Dynamic	0	Single
mtr/global_suppressions	mtr/global_suppressions	33	4	Dynamic	0	Single
mtr/test_ignored_global_suppressions	mtr/test_ignored_global_suppressions	33	4	Dynamic	0	Single
mtr/test_suppressions	mtr/test_suppressions	33	4	Dynamic	0	Single
test/t1	test/t1	33	4	Dynamic	0	Single
=== information_schema.innodb_tablespaces and innodb_datafiles ===
Space_Name	Space_Type	Page_Size	Zip_Size	BlockSize!=0	FileSize!=0	Formats_Permitted	Path
mtr/asserted_test_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/asserted_test_suppressions.ibd
mtr/global_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/global_suppressions.ibd
mtr/test_ignored_global_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/test_ignored_global_suppressions.ibd
mtr/test_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/test_suppressions.ibd
test/t1	Single	DEFAULT	0	0	0	Dynamic	MYSQLD_DATADIR/test/t1.ibd
=== information_schema.files ===
Space_Name	File_Type	Engine	Status	Tablespace_Name	Path
mtr/asserted_test_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/asserted_test_suppressions	MYSQLD_DATADIR/mtr/asserted_test_suppressions.ibd
mtr/global_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/global_suppressions	MYSQLD_DATADIR/mtr/global_suppressions.ibd
mtr/test_ignored_global_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/test_ignored_global_suppressions	MYSQLD_DATADIR/mtr/test_ignored_global_suppressions.ibd
mtr/test_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/test_suppressions	MYSQLD_DATADIR/mtr/test_suppressions.ibd
ALTER TABLE t1 RENAME t2;
=== information_schema.innodb_tables  and innodb_tablespaces ===
Table Name	Tablespace	Table Flags	Columns	Row Format	Zip Size	Space Type
mtr/asserted_test_suppressions	mtr/asserted_test_suppressions	33	4	Dynamic	0	Single
mtr/global_suppressions	mtr/global_suppressions	33	4	Dynamic	0	Single
mtr/test_ignored_global_suppressions	mtr/test_ignored_global_suppressions	33	4	Dynamic	0	Single
mtr/test_suppressions	mtr/test_suppressions	33	4	Dynamic	0	Single
test/t2	test/t2	33	4	Dynamic	0	Single
=== information_schema.innodb_tablespaces and innodb_datafiles ===
Space_Name	Space_Type	Page_Size	Zip_Size	BlockSize!=0	FileSize!=0	Formats_Permitted	Path
mtr/asserted_test_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/asserted_test_suppressions.ibd
mtr/global_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/global_suppressions.ibd
mtr/test_ignored_global_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/test_ignored_global_suppressions.ibd
mtr/test_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/test_suppressions.ibd
test/t2	Single	DEFAULT	0	0	0	Dynamic	MYSQLD_DATADIR/test/t2.ibd
=== information_schema.files ===
Space_Name	File_Type	Engine	Status	Tablespace_Name	Path
mtr/asserted_test_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/asserted_test_suppressions	MYSQLD_DATADIR/mtr/asserted_test_suppressions.ibd
mtr/global_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/global_suppressions	MYSQLD_DATADIR/mtr/global_suppressions.ibd
mtr/test_ignored_global_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/test_ignored_global_suppressions	MYSQLD_DATADIR/mtr/test_ignored_global_suppressions.ibd
mtr/test_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/test_suppressions	MYSQLD_DATADIR/mtr/test_suppressions.ibd
CREATE TABLE t1(c INT)ENGINE=myisam;
=== information_schema.innodb_tables  and innodb_tablespaces ===
Table Name	Tablespace	Table Flags	Columns	Row Format	Zip Size	Space Type
mtr/asserted_test_suppressions	mtr/asserted_test_suppressions	33	4	Dynamic	0	Single
mtr/global_suppressions	mtr/global_suppressions	33	4	Dynamic	0	Single
mtr/test_ignored_global_suppressions	mtr/test_ignored_global_suppressions	33	4	Dynamic	0	Single
mtr/test_suppressions	mtr/test_suppressions	33	4	Dynamic	0	Single
test/t2	test/t2	33	4	Dynamic	0	Single
=== information_schema.innodb_tablespaces and innodb_datafiles ===
Space_Name	Space_Type	Page_Size	Zip_Size	BlockSize!=0	FileSize!=0	Formats_Permitted	Path
mtr/asserted_test_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/asserted_test_suppressions.ibd
mtr/global_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/global_suppressions.ibd
mtr/test_ignored_global_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/test_ignored_global_suppressions.ibd
mtr/test_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/test_suppressions.ibd
test/t2	Single	DEFAULT	0	0	0	Dynamic	MYSQLD_DATADIR/test/t2.ibd
=== information_schema.files ===
Space_Name	File_Type	Engine	Status	Tablespace_Name	Path
mtr/asserted_test_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/asserted_test_suppressions	MYSQLD_DATADIR/mtr/asserted_test_suppressions.ibd
mtr/global_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/global_suppressions	MYSQLD_DATADIR/mtr/global_suppressions.ibd
mtr/test_ignored_global_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/test_ignored_global_suppressions	MYSQLD_DATADIR/mtr/test_ignored_global_suppressions.ibd
mtr/test_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/test_suppressions	MYSQLD_DATADIR/mtr/test_suppressions.ibd
alter table t1 engine=innodb;
=== information_schema.innodb_tables  and innodb_tablespaces ===
Table Name	Tablespace	Table Flags	Columns	Row Format	Zip Size	Space Type
mtr/asserted_test_suppressions	mtr/asserted_test_suppressions	33	4	Dynamic	0	Single
mtr/global_suppressions	mtr/global_suppressions	33	4	Dynamic	0	Single
mtr/test_ignored_global_suppressions	mtr/test_ignored_global_suppressions	33	4	Dynamic	0	Single
mtr/test_suppressions	mtr/test_suppressions	33	4	Dynamic	0	Single
test/t1	test/t1	33	4	Dynamic	0	Single
test/t2	test/t2	33	4	Dynamic	0	Single
=== information_schema.innodb_tablespaces and innodb_datafiles ===
Space_Name	Space_Type	Page_Size	Zip_Size	BlockSize!=0	FileSize!=0	Formats_Permitted	Path
mtr/asserted_test_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/asserted_test_suppressions.ibd
mtr/global_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/global_suppressions.ibd
mtr/test_ignored_global_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/test_ignored_global_suppressions.ibd
mtr/test_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/test_suppressions.ibd
test/t1	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/test/t1.ibd
test/t2	Single	DEFAULT	0	0	0	Dynamic	MYSQLD_DATADIR/test/t2.ibd
=== information_schema.files ===
Space_Name	File_Type	Engine	Status	Tablespace_Name	Path
mtr/asserted_test_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/asserted_test_suppressions	MYSQLD_DATADIR/mtr/asserted_test_suppressions.ibd
mtr/global_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/global_suppressions	MYSQLD_DATADIR/mtr/global_suppressions.ibd
mtr/test_ignored_global_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/test_ignored_global_suppressions	MYSQLD_DATADIR/mtr/test_ignored_global_suppressions.ibd
mtr/test_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/test_suppressions	MYSQLD_DATADIR/mtr/test_suppressions.ibd
test/t1	TABLESPACE	InnoDB	NORMAL	test/t1	MYSQLD_DATADIR/test/t1.ibd
drop table t1;
drop table t2;
# Test Case 2:
CREATE TABLE t1(b INT)ENGINE=innodb;
=== information_schema.innodb_tables  and innodb_tablespaces ===
Table Name	Tablespace	Table Flags	Columns	Row Format	Zip Size	Space Type
mtr/asserted_test_suppressions	mtr/asserted_test_suppressions	33	4	Dynamic	0	Single
mtr/global_suppressions	mtr/global_suppressions	33	4	Dynamic	0	Single
mtr/test_ignored_global_suppressions	mtr/test_ignored_global_suppressions	33	4	Dynamic	0	Single
mtr/test_suppressions	mtr/test_suppressions	33	4	Dynamic	0	Single
test/t1	test/t1	33	4	Dynamic	0	Single
=== information_schema.innodb_tablespaces and innodb_datafiles ===
Space_Name	Space_Type	Page_Size	Zip_Size	BlockSize!=0	FileSize!=0	Formats_Permitted	Path
mtr/asserted_test_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/asserted_test_suppressions.ibd
mtr/global_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/global_suppressions.ibd
mtr/test_ignored_global_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/test_ignored_global_suppressions.ibd
mtr/test_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/test_suppressions.ibd
test/t1	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/test/t1.ibd
=== information_schema.files ===
Space_Name	File_Type	Engine	Status	Tablespace_Name	Path
mtr/asserted_test_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/asserted_test_suppressions	MYSQLD_DATADIR/mtr/asserted_test_suppressions.ibd
mtr/global_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/global_suppressions	MYSQLD_DATADIR/mtr/global_suppressions.ibd
mtr/test_ignored_global_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/test_ignored_global_suppressions	MYSQLD_DATADIR/mtr/test_ignored_global_suppressions.ibd
mtr/test_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/test_suppressions	MYSQLD_DATADIR/mtr/test_suppressions.ibd
test/t1	TABLESPACE	InnoDB	NORMAL	test/t1	MYSQLD_DATADIR/test/t1.ibd
ALTER TABLE t1 DISCARD TABLESPACE;
=== information_schema.innodb_tables  and innodb_tablespaces ===
Table Name	Tablespace	Table Flags	Columns	Row Format	Zip Size	Space Type
mtr/asserted_test_suppressions	mtr/asserted_test_suppressions	33	4	Dynamic	0	Single
mtr/global_suppressions	mtr/global_suppressions	33	4	Dynamic	0	Single
mtr/test_ignored_global_suppressions	mtr/test_ignored_global_suppressions	33	4	Dynamic	0	Single
mtr/test_suppressions	mtr/test_suppressions	33	4	Dynamic	0	Single
test/t1	test/t1	33	4	Dynamic	0	Single
=== information_schema.innodb_tablespaces and innodb_datafiles ===
Space_Name	Space_Type	Page_Size	Zip_Size	BlockSize!=0	FileSize!=0	Formats_Permitted	Path
mtr/asserted_test_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/asserted_test_suppressions.ibd
mtr/global_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/global_suppressions.ibd
mtr/test_ignored_global_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/test_ignored_global_suppressions.ibd
mtr/test_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/test_suppressions.ibd
test/t1	Single	DEFAULT	0	0	0	Dynamic	MYSQLD_DATADIR/test/t1.ibd
=== information_schema.files ===
Space_Name	File_Type	Engine	Status	Tablespace_Name	Path
mtr/asserted_test_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/asserted_test_suppressions	MYSQLD_DATADIR/mtr/asserted_test_suppressions.ibd
mtr/global_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/global_suppressions	MYSQLD_DATADIR/mtr/global_suppressions.ibd
mtr/test_ignored_global_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/test_ignored_global_suppressions	MYSQLD_DATADIR/mtr/test_ignored_global_suppressions.ibd
mtr/test_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/test_suppressions	MYSQLD_DATADIR/mtr/test_suppressions.ibd
ALTER TABLE t1 RENAME t2;
=== information_schema.innodb_tables  and innodb_tablespaces ===
Table Name	Tablespace	Table Flags	Columns	Row Format	Zip Size	Space Type
mtr/asserted_test_suppressions	mtr/asserted_test_suppressions	33	4	Dynamic	0	Single
mtr/global_suppressions	mtr/global_suppressions	33	4	Dynamic	0	Single
mtr/test_ignored_global_suppressions	mtr/test_ignored_global_suppressions	33	4	Dynamic	0	Single
mtr/test_suppressions	mtr/test_suppressions	33	4	Dynamic	0	Single
test/t2	test/t2	33	4	Dynamic	0	Single
=== information_schema.innodb_tablespaces and innodb_datafiles ===
Space_Name	Space_Type	Page_Size	Zip_Size	BlockSize!=0	FileSize!=0	Formats_Permitted	Path
mtr/asserted_test_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/asserted_test_suppressions.ibd
mtr/global_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/global_suppressions.ibd
mtr/test_ignored_global_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/test_ignored_global_suppressions.ibd
mtr/test_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/test_suppressions.ibd
test/t2	Single	DEFAULT	0	0	0	Dynamic	MYSQLD_DATADIR/test/t2.ibd
=== information_schema.files ===
Space_Name	File_Type	Engine	Status	Tablespace_Name	Path
mtr/asserted_test_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/asserted_test_suppressions	MYSQLD_DATADIR/mtr/asserted_test_suppressions.ibd
mtr/global_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/global_suppressions	MYSQLD_DATADIR/mtr/global_suppressions.ibd
mtr/test_ignored_global_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/test_ignored_global_suppressions	MYSQLD_DATADIR/mtr/test_ignored_global_suppressions.ibd
mtr/test_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/test_suppressions	MYSQLD_DATADIR/mtr/test_suppressions.ibd
CREATE TABLE t1(c INT)ENGINE=innodb;
=== information_schema.innodb_tables  and innodb_tablespaces ===
Table Name	Tablespace	Table Flags	Columns	Row Format	Zip Size	Space Type
mtr/asserted_test_suppressions	mtr/asserted_test_suppressions	33	4	Dynamic	0	Single
mtr/global_suppressions	mtr/global_suppressions	33	4	Dynamic	0	Single
mtr/test_ignored_global_suppressions	mtr/test_ignored_global_suppressions	33	4	Dynamic	0	Single
mtr/test_suppressions	mtr/test_suppressions	33	4	Dynamic	0	Single
test/t1	test/t1	33	4	Dynamic	0	Single
test/t2	test/t2	33	4	Dynamic	0	Single
=== information_schema.innodb_tablespaces and innodb_datafiles ===
Space_Name	Space_Type	Page_Size	Zip_Size	BlockSize!=0	FileSize!=0	Formats_Permitted	Path
mtr/asserted_test_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/asserted_test_suppressions.ibd
mtr/global_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/global_suppressions.ibd
mtr/test_ignored_global_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/test_ignored_global_suppressions.ibd
mtr/test_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/test_suppressions.ibd
test/t1	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/test/t1.ibd
test/t2	Single	DEFAULT	0	0	0	Dynamic	MYSQLD_DATADIR/test/t2.ibd
=== information_schema.files ===
Space_Name	File_Type	Engine	Status	Tablespace_Name	Path
mtr/asserted_test_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/asserted_test_suppressions	MYSQLD_DATADIR/mtr/asserted_test_suppressions.ibd
mtr/global_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/global_suppressions	MYSQLD_DATADIR/mtr/global_suppressions.ibd
mtr/test_ignored_global_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/test_ignored_global_suppressions	MYSQLD_DATADIR/mtr/test_ignored_global_suppressions.ibd
mtr/test_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/test_suppressions	MYSQLD_DATADIR/mtr/test_suppressions.ibd
test/t1	TABLESPACE	InnoDB	NORMAL	test/t1	MYSQLD_DATADIR/test/t1.ibd
drop table t1;
drop table t2;
# Test Case 3:
CREATE TABLE `t1/t2` (b INT, c text) ENGINE=innodb;
=== information_schema.innodb_tables  and innodb_tablespaces ===
Table Name	Tablespace	Table Flags	Columns	Row Format	Zip Size	Space Type
mtr/asserted_test_suppressions	mtr/asserted_test_suppressions	33	4	Dynamic	0	Single
mtr/global_suppressions	mtr/global_suppressions	33	4	Dynamic	0	Single
mtr/test_ignored_global_suppressions	mtr/test_ignored_global_suppressions	33	4	Dynamic	0	Single
mtr/test_suppressions	mtr/test_suppressions	33	4	Dynamic	0	Single
test/t1@002ft2	test/t1/t2	33	5	Dynamic	0	Single
=== information_schema.innodb_tablespaces and innodb_datafiles ===
Space_Name	Space_Type	Page_Size	Zip_Size	BlockSize!=0	FileSize!=0	Formats_Permitted	Path
mtr/asserted_test_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/asserted_test_suppressions.ibd
mtr/global_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/global_suppressions.ibd
mtr/test_ignored_global_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/test_ignored_global_suppressions.ibd
mtr/test_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/test_suppressions.ibd
test/t1/t2	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/test/t1@002ft2.ibd
=== information_schema.files ===
Space_Name	File_Type	Engine	Status	Tablespace_Name	Path
mtr/asserted_test_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/asserted_test_suppressions	MYSQLD_DATADIR/mtr/asserted_test_suppressions.ibd
mtr/global_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/global_suppressions	MYSQLD_DATADIR/mtr/global_suppressions.ibd
mtr/test_ignored_global_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/test_ignored_global_suppressions	MYSQLD_DATADIR/mtr/test_ignored_global_suppressions.ibd
mtr/test_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/test_suppressions	MYSQLD_DATADIR/mtr/test_suppressions.ibd
test/t1/t2	TABLESPACE	InnoDB	NORMAL	test/t1/t2	MYSQLD_DATADIR/test/t1@002ft2.ibd
ALTER TABLE `t1/t2` DISCARD TABLESPACE;
=== information_schema.innodb_tables  and innodb_tablespaces ===
Table Name	Tablespace	Table Flags	Columns	Row Format	Zip Size	Space Type
mtr/asserted_test_suppressions	mtr/asserted_test_suppressions	33	4	Dynamic	0	Single
mtr/global_suppressions	mtr/global_suppressions	33	4	Dynamic	0	Single
mtr/test_ignored_global_suppressions	mtr/test_ignored_global_suppressions	33	4	Dynamic	0	Single
mtr/test_suppressions	mtr/test_suppressions	33	4	Dynamic	0	Single
test/t1@002ft2	test/t1/t2	33	5	Dynamic	0	Single
=== information_schema.innodb_tablespaces and innodb_datafiles ===
Space_Name	Space_Type	Page_Size	Zip_Size	BlockSize!=0	FileSize!=0	Formats_Permitted	Path
mtr/asserted_test_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/asserted_test_suppressions.ibd
mtr/global_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/global_suppressions.ibd
mtr/test_ignored_global_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/test_ignored_global_suppressions.ibd
mtr/test_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/test_suppressions.ibd
test/t1/t2	Single	DEFAULT	0	0	0	Dynamic	MYSQLD_DATADIR/test/t1@002ft2.ibd
=== information_schema.files ===
Space_Name	File_Type	Engine	Status	Tablespace_Name	Path
mtr/asserted_test_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/asserted_test_suppressions	MYSQLD_DATADIR/mtr/asserted_test_suppressions.ibd
mtr/global_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/global_suppressions	MYSQLD_DATADIR/mtr/global_suppressions.ibd
mtr/test_ignored_global_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/test_ignored_global_suppressions	MYSQLD_DATADIR/mtr/test_ignored_global_suppressions.ibd
mtr/test_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/test_suppressions	MYSQLD_DATADIR/mtr/test_suppressions.ibd
ALTER TABLE `t1/t2` RENAME `t3\t4\t5`;
=== information_schema.innodb_tables  and innodb_tablespaces ===
Table Name	Tablespace	Table Flags	Columns	Row Format	Zip Size	Space Type
mtr/asserted_test_suppressions	mtr/asserted_test_suppressions	33	4	Dynamic	0	Single
mtr/global_suppressions	mtr/global_suppressions	33	4	Dynamic	0	Single
mtr/test_ignored_global_suppressions	mtr/test_ignored_global_suppressions	33	4	Dynamic	0	Single
mtr/test_suppressions	mtr/test_suppressions	33	4	Dynamic	0	Single
test/t3@005ct4@005ct5	test/t3\t4\t5	33	5	Dynamic	0	Single
=== information_schema.innodb_tablespaces and innodb_datafiles ===
Space_Name	Space_Type	Page_Size	Zip_Size	BlockSize!=0	FileSize!=0	Formats_Permitted	Path
mtr/asserted_test_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/asserted_test_suppressions.ibd
mtr/global_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/global_suppressions.ibd
mtr/test_ignored_global_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/test_ignored_global_suppressions.ibd
mtr/test_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/test_suppressions.ibd
test/t3\t4\t5	Single	DEFAULT	0	0	0	Dynamic	MYSQLD_DATADIR/test/t3@005ct4@005ct5.ibd
=== information_schema.files ===
Space_Name	File_Type	Engine	Status	Tablespace_Name	Path
mtr/asserted_test_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/asserted_test_suppressions	MYSQLD_DATADIR/mtr/asserted_test_suppressions.ibd
mtr/global_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/global_suppressions	MYSQLD_DATADIR/mtr/global_suppressions.ibd
mtr/test_ignored_global_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/test_ignored_global_suppressions	MYSQLD_DATADIR/mtr/test_ignored_global_suppressions.ibd
mtr/test_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/test_suppressions	MYSQLD_DATADIR/mtr/test_suppressions.ibd
drop table `t3\t4\t5`;
# Test Case 4:
create database `d1/d2\d3`;
use `d1/d2\d3`;
CREATE TABLE `t1/t2\t3` (b INT, c text) ENGINE=innodb;
=== information_schema.innodb_tables  and innodb_tablespaces ===
Table Name	Tablespace	Table Flags	Columns	Row Format	Zip Size	Space Type
d1@002fd2@005cd3/t1@002ft2@005ct3	d1/d2\d3/t1/t2\t3	33	5	Dynamic	0	Single
mtr/asserted_test_suppressions	mtr/asserted_test_suppressions	33	4	Dynamic	0	Single
mtr/global_suppressions	mtr/global_suppressions	33	4	Dynamic	0	Single
mtr/test_ignored_global_suppressions	mtr/test_ignored_global_suppressions	33	4	Dynamic	0	Single
mtr/test_suppressions	mtr/test_suppressions	33	4	Dynamic	0	Single
=== information_schema.innodb_tablespaces and innodb_datafiles ===
Space_Name	Space_Type	Page_Size	Zip_Size	BlockSize!=0	FileSize!=0	Formats_Permitted	Path
d1/d2\d3/t1/t2\t3	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/d1@002fd2@005cd3/t1@002ft2@005ct3.ibd
mtr/asserted_test_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/asserted_test_suppressions.ibd
mtr/global_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/global_suppressions.ibd
mtr/test_ignored_global_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/test_ignored_global_suppressions.ibd
mtr/test_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/test_suppressions.ibd
=== information_schema.files ===
Space_Name	File_Type	Engine	Status	Tablespace_Name	Path
d1/d2\d3/t1/t2\t3	TABLESPACE	InnoDB	NORMAL	d1/d2\d3/t1/t2\t3	MYSQLD_DATADIR/d1@002fd2@005cd3/t1@002ft2@005ct3.ibd
mtr/asserted_test_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/asserted_test_suppressions	MYSQLD_DATADIR/mtr/asserted_test_suppressions.ibd
mtr/global_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/global_suppressions	MYSQLD_DATADIR/mtr/global_suppressions.ibd
mtr/test_ignored_global_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/test_ignored_global_suppressions	MYSQLD_DATADIR/mtr/test_ignored_global_suppressions.ibd
mtr/test_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/test_suppressions	MYSQLD_DATADIR/mtr/test_suppressions.ibd
ALTER TABLE `t1/t2\t3` DISCARD TABLESPACE;
=== information_schema.innodb_tables  and innodb_tablespaces ===
Table Name	Tablespace	Table Flags	Columns	Row Format	Zip Size	Space Type
d1@002fd2@005cd3/t1@002ft2@005ct3	d1/d2\d3/t1/t2\t3	33	5	Dynamic	0	Single
mtr/asserted_test_suppressions	mtr/asserted_test_suppressions	33	4	Dynamic	0	Single
mtr/global_suppressions	mtr/global_suppressions	33	4	Dynamic	0	Single
mtr/test_ignored_global_suppressions	mtr/test_ignored_global_suppressions	33	4	Dynamic	0	Single
mtr/test_suppressions	mtr/test_suppressions	33	4	Dynamic	0	Single
=== information_schema.innodb_tablespaces and innodb_datafiles ===
Space_Name	Space_Type	Page_Size	Zip_Size	BlockSize!=0	FileSize!=0	Formats_Permitted	Path
d1/d2\d3/t1/t2\t3	Single	DEFAULT	0	0	0	Dynamic	MYSQLD_DATADIR/d1@002fd2@005cd3/t1@002ft2@005ct3.ibd
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
ALTER TABLE `t1/t2\t3` RENAME `t3\t4/t5`;
=== information_schema.innodb_tables  and innodb_tablespaces ===
Table Name	Tablespace	Table Flags	Columns	Row Format	Zip Size	Space Type
d1@002fd2@005cd3/t3@005ct4@002ft5	d1/d2\d3/t3\t4/t5	33	5	Dynamic	0	Single
mtr/asserted_test_suppressions	mtr/asserted_test_suppressions	33	4	Dynamic	0	Single
mtr/global_suppressions	mtr/global_suppressions	33	4	Dynamic	0	Single
mtr/test_ignored_global_suppressions	mtr/test_ignored_global_suppressions	33	4	Dynamic	0	Single
mtr/test_suppressions	mtr/test_suppressions	33	4	Dynamic	0	Single
=== information_schema.innodb_tablespaces and innodb_datafiles ===
Space_Name	Space_Type	Page_Size	Zip_Size	BlockSize!=0	FileSize!=0	Formats_Permitted	Path
d1/d2\d3/t3\t4/t5	Single	DEFAULT	0	0	0	Dynamic	MYSQLD_DATADIR/d1@002fd2@005cd3/t3@005ct4@002ft5.ibd
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
drop table `t3\t4/t5`;
drop database `d1/d2\d3`;
# Test Case 5:
create database `d1/d2\d3`;
create database `d4/d5\d6`;
CREATE TABLE `d1/d2\d3`.`t1/t2\t3` (b INT, c text) ENGINE=innodb;
=== information_schema.innodb_tables  and innodb_tablespaces ===
Table Name	Tablespace	Table Flags	Columns	Row Format	Zip Size	Space Type
d1@002fd2@005cd3/t1@002ft2@005ct3	d1/d2\d3/t1/t2\t3	33	5	Dynamic	0	Single
mtr/asserted_test_suppressions	mtr/asserted_test_suppressions	33	4	Dynamic	0	Single
mtr/global_suppressions	mtr/global_suppressions	33	4	Dynamic	0	Single
mtr/test_ignored_global_suppressions	mtr/test_ignored_global_suppressions	33	4	Dynamic	0	Single
mtr/test_suppressions	mtr/test_suppressions	33	4	Dynamic	0	Single
=== information_schema.innodb_tablespaces and innodb_datafiles ===
Space_Name	Space_Type	Page_Size	Zip_Size	BlockSize!=0	FileSize!=0	Formats_Permitted	Path
d1/d2\d3/t1/t2\t3	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/d1@002fd2@005cd3/t1@002ft2@005ct3.ibd
mtr/asserted_test_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/asserted_test_suppressions.ibd
mtr/global_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/global_suppressions.ibd
mtr/test_ignored_global_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/test_ignored_global_suppressions.ibd
mtr/test_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/test_suppressions.ibd
=== information_schema.files ===
Space_Name	File_Type	Engine	Status	Tablespace_Name	Path
d1/d2\d3/t1/t2\t3	TABLESPACE	InnoDB	NORMAL	d1/d2\d3/t1/t2\t3	MYSQLD_DATADIR/d1@002fd2@005cd3/t1@002ft2@005ct3.ibd
mtr/asserted_test_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/asserted_test_suppressions	MYSQLD_DATADIR/mtr/asserted_test_suppressions.ibd
mtr/global_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/global_suppressions	MYSQLD_DATADIR/mtr/global_suppressions.ibd
mtr/test_ignored_global_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/test_ignored_global_suppressions	MYSQLD_DATADIR/mtr/test_ignored_global_suppressions.ibd
mtr/test_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/test_suppressions	MYSQLD_DATADIR/mtr/test_suppressions.ibd
ALTER TABLE `d1/d2\d3`.`t1/t2\t3` DISCARD TABLESPACE;
=== information_schema.innodb_tables  and innodb_tablespaces ===
Table Name	Tablespace	Table Flags	Columns	Row Format	Zip Size	Space Type
d1@002fd2@005cd3/t1@002ft2@005ct3	d1/d2\d3/t1/t2\t3	33	5	Dynamic	0	Single
mtr/asserted_test_suppressions	mtr/asserted_test_suppressions	33	4	Dynamic	0	Single
mtr/global_suppressions	mtr/global_suppressions	33	4	Dynamic	0	Single
mtr/test_ignored_global_suppressions	mtr/test_ignored_global_suppressions	33	4	Dynamic	0	Single
mtr/test_suppressions	mtr/test_suppressions	33	4	Dynamic	0	Single
=== information_schema.innodb_tablespaces and innodb_datafiles ===
Space_Name	Space_Type	Page_Size	Zip_Size	BlockSize!=0	FileSize!=0	Formats_Permitted	Path
d1/d2\d3/t1/t2\t3	Single	DEFAULT	0	0	0	Dynamic	MYSQLD_DATADIR/d1@002fd2@005cd3/t1@002ft2@005ct3.ibd
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
ALTER TABLE `d1/d2\d3`.`t1/t2\t3` RENAME `d4/d5\d6`.`t3\t4/t5`;
=== information_schema.innodb_tables  and innodb_tablespaces ===
Table Name	Tablespace	Table Flags	Columns	Row Format	Zip Size	Space Type
d4@002fd5@005cd6/t3@005ct4@002ft5	d4/d5\d6/t3\t4/t5	33	5	Dynamic	0	Single
mtr/asserted_test_suppressions	mtr/asserted_test_suppressions	33	4	Dynamic	0	Single
mtr/global_suppressions	mtr/global_suppressions	33	4	Dynamic	0	Single
mtr/test_ignored_global_suppressions	mtr/test_ignored_global_suppressions	33	4	Dynamic	0	Single
mtr/test_suppressions	mtr/test_suppressions	33	4	Dynamic	0	Single
=== information_schema.innodb_tablespaces and innodb_datafiles ===
Space_Name	Space_Type	Page_Size	Zip_Size	BlockSize!=0	FileSize!=0	Formats_Permitted	Path
d4/d5\d6/t3\t4/t5	Single	DEFAULT	0	0	0	Dynamic	MYSQLD_DATADIR/d4@002fd5@005cd6/t3@005ct4@002ft5.ibd
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
drop table `d4/d5\d6`.`t3\t4/t5`;
drop database `d1/d2\d3`;
drop database `d4/d5\d6`;
# Test Case 6:
create database `d1/d2`;
create database `d1`;
CREATE TABLE `d1/d2`.`t1/t2` (b INT, c text) ENGINE=innodb;
=== information_schema.innodb_tables  and innodb_tablespaces ===
Table Name	Tablespace	Table Flags	Columns	Row Format	Zip Size	Space Type
d1@002fd2/t1@002ft2	d1/d2/t1/t2	33	5	Dynamic	0	Single
mtr/asserted_test_suppressions	mtr/asserted_test_suppressions	33	4	Dynamic	0	Single
mtr/global_suppressions	mtr/global_suppressions	33	4	Dynamic	0	Single
mtr/test_ignored_global_suppressions	mtr/test_ignored_global_suppressions	33	4	Dynamic	0	Single
mtr/test_suppressions	mtr/test_suppressions	33	4	Dynamic	0	Single
=== information_schema.innodb_tablespaces and innodb_datafiles ===
Space_Name	Space_Type	Page_Size	Zip_Size	BlockSize!=0	FileSize!=0	Formats_Permitted	Path
d1/d2/t1/t2	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/d1@002fd2/t1@002ft2.ibd
mtr/asserted_test_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/asserted_test_suppressions.ibd
mtr/global_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/global_suppressions.ibd
mtr/test_ignored_global_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/test_ignored_global_suppressions.ibd
mtr/test_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/test_suppressions.ibd
=== information_schema.files ===
Space_Name	File_Type	Engine	Status	Tablespace_Name	Path
d1/d2/t1/t2	TABLESPACE	InnoDB	NORMAL	d1/d2/t1/t2	MYSQLD_DATADIR/d1@002fd2/t1@002ft2.ibd
mtr/asserted_test_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/asserted_test_suppressions	MYSQLD_DATADIR/mtr/asserted_test_suppressions.ibd
mtr/global_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/global_suppressions	MYSQLD_DATADIR/mtr/global_suppressions.ibd
mtr/test_ignored_global_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/test_ignored_global_suppressions	MYSQLD_DATADIR/mtr/test_ignored_global_suppressions.ibd
mtr/test_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/test_suppressions	MYSQLD_DATADIR/mtr/test_suppressions.ibd
ALTER TABLE `d1/d2`.`t1/t2` DISCARD TABLESPACE;
=== information_schema.innodb_tables  and innodb_tablespaces ===
Table Name	Tablespace	Table Flags	Columns	Row Format	Zip Size	Space Type
d1@002fd2/t1@002ft2	d1/d2/t1/t2	33	5	Dynamic	0	Single
mtr/asserted_test_suppressions	mtr/asserted_test_suppressions	33	4	Dynamic	0	Single
mtr/global_suppressions	mtr/global_suppressions	33	4	Dynamic	0	Single
mtr/test_ignored_global_suppressions	mtr/test_ignored_global_suppressions	33	4	Dynamic	0	Single
mtr/test_suppressions	mtr/test_suppressions	33	4	Dynamic	0	Single
=== information_schema.innodb_tablespaces and innodb_datafiles ===
Space_Name	Space_Type	Page_Size	Zip_Size	BlockSize!=0	FileSize!=0	Formats_Permitted	Path
d1/d2/t1/t2	Single	DEFAULT	0	0	0	Dynamic	MYSQLD_DATADIR/d1@002fd2/t1@002ft2.ibd
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
ALTER TABLE `d1/d2`.`t1/t2` RENAME `d1`.`d2/t1/t2`;
=== information_schema.innodb_tables  and innodb_tablespaces ===
Table Name	Tablespace	Table Flags	Columns	Row Format	Zip Size	Space Type
d1/d2@002ft1@002ft2	d1/d2/t1/t2	33	5	Dynamic	0	Single
mtr/asserted_test_suppressions	mtr/asserted_test_suppressions	33	4	Dynamic	0	Single
mtr/global_suppressions	mtr/global_suppressions	33	4	Dynamic	0	Single
mtr/test_ignored_global_suppressions	mtr/test_ignored_global_suppressions	33	4	Dynamic	0	Single
mtr/test_suppressions	mtr/test_suppressions	33	4	Dynamic	0	Single
=== information_schema.innodb_tablespaces and innodb_datafiles ===
Space_Name	Space_Type	Page_Size	Zip_Size	BlockSize!=0	FileSize!=0	Formats_Permitted	Path
d1/d2/t1/t2	Single	DEFAULT	0	0	0	Dynamic	MYSQLD_DATADIR/d1/d2@002ft1@002ft2.ibd
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
drop table `d1`.`d2/t1/t2`;
drop database `d1/d2`;
drop database `d1`;
