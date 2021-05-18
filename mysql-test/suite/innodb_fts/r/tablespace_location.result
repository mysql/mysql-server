#
# BUG#20527217 - INNODB: PUT FTS AUXILIARY TABLES INTO THE SAME GENERAL
#		TABLESPACE AS THE PRIMARY
#
SET DEFAULT_STORAGE_ENGINE=InnoDB;
SET GLOBAL innodb_file_per_table = ON;
SHOW VARIABLES LIKE 'innodb_file_per_table';
Variable_name	Value
innodb_file_per_table	ON
# Strict-mode has no affect on CREATE TABLESPACE.
# It rejects all invalid input, as if strict mode is always ON.
SET SESSION innodb_strict_mode = OFF;
SHOW VARIABLES LIKE 'innodb_strict_mode';
Variable_name	Value
innodb_strict_mode	OFF
CREATE TABLESPACE s_def ADD DATAFILE 's_def.ibd';
CREATE TABLESPACE s_zip ADD DATAFILE 's_zip.ibd' FILE_BLOCK_SIZE=2k;
=== information_schema.innodb_tablespaces and innodb_datafiles ===
Space_Name	Space_Type	Page_Size	Zip_Size	Formats_Permitted	Path
mtr/global_suppressions	Single	DEFAULT	0	Dynamic	MYSQLD_DATADIR/mtr/global_suppressions.ibd
mtr/test_suppressions	Single	DEFAULT	0	Dynamic	MYSQLD_DATADIR/mtr/test_suppressions.ibd
s_def	General	DEFAULT	0	Any	s_def.ibd
s_zip	General	DEFAULT	2048	Compressed	s_zip.ibd
=== information_schema.files ===
Space_Name	File_Type	Engine	Status	Tablespace_Name	Path
mtr/global_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/global_suppressions	MYSQLD_DATADIR/mtr/global_suppressions.ibd
mtr/test_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/test_suppressions	MYSQLD_DATADIR/mtr/test_suppressions.ibd
s_def	TABLESPACE	InnoDB	NORMAL	s_def	MYSQLD_DATADIR/s_def.ibd
s_zip	TABLESPACE	InnoDB	NORMAL	s_zip	MYSQLD_DATADIR/s_zip.ibd
#
# tablespace=s_def row_format=redundant
#
CREATE TABLE t1 (a VARCHAR(200), b TEXT, FULLTEXT (a,b)) TABLESPACE=s_def ROW_FORMAT=redundant ;
=== information_schema.innodb_tables  and innodb_tablespaces ===
Table Name	Tablespace	Table Flags	Columns	Row Format	Zip Size	Space Type
mtr/global_suppressions	mtr/global_suppressions	33	4	Dynamic	0	Single
mtr/test_suppressions	mtr/test_suppressions	33	4	Dynamic	0	Single
test/fts_aux_index_1	s_def	128	8	Redundant	0	General
test/fts_aux_index_2	s_def	128	8	Redundant	0	General
test/fts_aux_index_3	s_def	128	8	Redundant	0	General
test/fts_aux_index_4	s_def	128	8	Redundant	0	General
test/fts_aux_index_5	s_def	128	8	Redundant	0	General
test/fts_aux_index_6	s_def	128	8	Redundant	0	General
test/fts_aux_being_deleted	s_def	128	4	Redundant	0	General
test/fts_aux_being_deleted_cache	s_def	128	4	Redundant	0	General
test/fts_aux_config	s_def	128	5	Redundant	0	General
test/fts_aux_deleted	s_def	128	4	Redundant	0	General
test/fts_aux_deleted_cache	s_def	128	4	Redundant	0	General
test/t1	s_def	128	6	Redundant	0	General
INSERT INTO t1 (a,b) VALUES
('MySQL Tutorial','DBMS stands for DataBase')  ,
('How To Use MySQL Well','After you went through a'),
('Optimizing MySQL','In this tutorial we will show');
INSERT INTO t1 (a,b) VALUES
('1001 MySQL Tricks','1. Never run mysqld as root'),
('MySQL vs. YourSQL','In the following database comparison'),
('MySQL Security','When configured properly, MySQL');
SELECT * FROM t1 WHERE MATCH (a,b)
AGAINST ('Tutorial' IN NATURAL LANGUAGE MODE);
a	b
MySQL Tutorial	DBMS stands for DataBase
Optimizing MySQL	In this tutorial we will show
TRUNCATE TABLE t1;
=== information_schema.innodb_tables  and innodb_tablespaces ===
Table Name	Tablespace	Table Flags	Columns	Row Format	Zip Size	Space Type
mtr/global_suppressions	mtr/global_suppressions	33	4	Dynamic	0	Single
mtr/test_suppressions	mtr/test_suppressions	33	4	Dynamic	0	Single
test/fts_aux_index_1	s_def	128	8	Redundant	0	General
test/fts_aux_index_2	s_def	128	8	Redundant	0	General
test/fts_aux_index_3	s_def	128	8	Redundant	0	General
test/fts_aux_index_4	s_def	128	8	Redundant	0	General
test/fts_aux_index_5	s_def	128	8	Redundant	0	General
test/fts_aux_index_6	s_def	128	8	Redundant	0	General
test/fts_aux_being_deleted	s_def	128	4	Redundant	0	General
test/fts_aux_being_deleted_cache	s_def	128	4	Redundant	0	General
test/fts_aux_config	s_def	128	5	Redundant	0	General
test/fts_aux_deleted	s_def	128	4	Redundant	0	General
test/fts_aux_deleted_cache	s_def	128	4	Redundant	0	General
test/t1	s_def	128	6	Redundant	0	General
INSERT INTO t1 (a,b) VALUES
('MySQL Tutorial','DBMS stands for DataBase')  ,
('How To Use MySQL Well','After you went through a'),
('Optimizing MySQL','In this tutorial we will show');
INSERT INTO t1 (a,b) VALUES
('1001 MySQL Tricks','1. Never run mysqld as root'),
('MySQL vs. YourSQL','In the following database comparison'),
('MySQL Security','When configured properly, MySQL');
SELECT * FROM t1 WHERE MATCH (a,b)
AGAINST ('Tutorial' IN NATURAL LANGUAGE MODE);
a	b
MySQL Tutorial	DBMS stands for DataBase
Optimizing MySQL	In this tutorial we will show
DROP TABLE t1;
#
# tablespace=s_def row_format=compact
#
CREATE TABLE t1 (a VARCHAR(200), b TEXT, FULLTEXT (a,b)) TABLESPACE=s_def ROW_FORMAT=compact ;
=== information_schema.innodb_tables  and innodb_tablespaces ===
Table Name	Tablespace	Table Flags	Columns	Row Format	Zip Size	Space Type
mtr/global_suppressions	mtr/global_suppressions	33	4	Dynamic	0	Single
mtr/test_suppressions	mtr/test_suppressions	33	4	Dynamic	0	Single
test/fts_aux_index_1	s_def	129	8	Compact	0	General
test/fts_aux_index_2	s_def	129	8	Compact	0	General
test/fts_aux_index_3	s_def	129	8	Compact	0	General
test/fts_aux_index_4	s_def	129	8	Compact	0	General
test/fts_aux_index_5	s_def	129	8	Compact	0	General
test/fts_aux_index_6	s_def	129	8	Compact	0	General
test/fts_aux_being_deleted	s_def	129	4	Compact	0	General
test/fts_aux_being_deleted_cache	s_def	129	4	Compact	0	General
test/fts_aux_config	s_def	129	5	Compact	0	General
test/fts_aux_deleted	s_def	129	4	Compact	0	General
test/fts_aux_deleted_cache	s_def	129	4	Compact	0	General
test/t1	s_def	129	6	Compact	0	General
INSERT INTO t1 (a,b) VALUES
('MySQL Tutorial','DBMS stands for DataBase')  ,
('How To Use MySQL Well','After you went through a'),
('Optimizing MySQL','In this tutorial we will show');
INSERT INTO t1 (a,b) VALUES
('1001 MySQL Tricks','1. Never run mysqld as root'),
('MySQL vs. YourSQL','In the following database comparison'),
('MySQL Security','When configured properly, MySQL');
SELECT * FROM t1 WHERE MATCH (a,b)
AGAINST ('Tutorial' IN NATURAL LANGUAGE MODE);
a	b
MySQL Tutorial	DBMS stands for DataBase
Optimizing MySQL	In this tutorial we will show
TRUNCATE TABLE t1;
=== information_schema.innodb_tables  and innodb_tablespaces ===
Table Name	Tablespace	Table Flags	Columns	Row Format	Zip Size	Space Type
mtr/global_suppressions	mtr/global_suppressions	33	4	Dynamic	0	Single
mtr/test_suppressions	mtr/test_suppressions	33	4	Dynamic	0	Single
test/fts_aux_index_1	s_def	129	8	Compact	0	General
test/fts_aux_index_2	s_def	129	8	Compact	0	General
test/fts_aux_index_3	s_def	129	8	Compact	0	General
test/fts_aux_index_4	s_def	129	8	Compact	0	General
test/fts_aux_index_5	s_def	129	8	Compact	0	General
test/fts_aux_index_6	s_def	129	8	Compact	0	General
test/fts_aux_being_deleted	s_def	129	4	Compact	0	General
test/fts_aux_being_deleted_cache	s_def	129	4	Compact	0	General
test/fts_aux_config	s_def	129	5	Compact	0	General
test/fts_aux_deleted	s_def	129	4	Compact	0	General
test/fts_aux_deleted_cache	s_def	129	4	Compact	0	General
test/t1	s_def	129	6	Compact	0	General
INSERT INTO t1 (a,b) VALUES
('MySQL Tutorial','DBMS stands for DataBase')  ,
('How To Use MySQL Well','After you went through a'),
('Optimizing MySQL','In this tutorial we will show');
INSERT INTO t1 (a,b) VALUES
('1001 MySQL Tricks','1. Never run mysqld as root'),
('MySQL vs. YourSQL','In the following database comparison'),
('MySQL Security','When configured properly, MySQL');
SELECT * FROM t1 WHERE MATCH (a,b)
AGAINST ('Tutorial' IN NATURAL LANGUAGE MODE);
a	b
MySQL Tutorial	DBMS stands for DataBase
Optimizing MySQL	In this tutorial we will show
DROP TABLE t1;
#
# tablespace=s_def row_format=dynamic
#
CREATE TABLE t1 (a VARCHAR(200), b TEXT, FULLTEXT (a,b)) TABLESPACE=s_def ROW_FORMAT=dynamic ;
=== information_schema.innodb_tables  and innodb_tablespaces ===
Table Name	Tablespace	Table Flags	Columns	Row Format	Zip Size	Space Type
mtr/global_suppressions	mtr/global_suppressions	33	4	Dynamic	0	Single
mtr/test_suppressions	mtr/test_suppressions	33	4	Dynamic	0	Single
test/fts_aux_index_1	s_def	161	8	Dynamic	0	General
test/fts_aux_index_2	s_def	161	8	Dynamic	0	General
test/fts_aux_index_3	s_def	161	8	Dynamic	0	General
test/fts_aux_index_4	s_def	161	8	Dynamic	0	General
test/fts_aux_index_5	s_def	161	8	Dynamic	0	General
test/fts_aux_index_6	s_def	161	8	Dynamic	0	General
test/fts_aux_being_deleted	s_def	161	4	Dynamic	0	General
test/fts_aux_being_deleted_cache	s_def	161	4	Dynamic	0	General
test/fts_aux_config	s_def	161	5	Dynamic	0	General
test/fts_aux_deleted	s_def	161	4	Dynamic	0	General
test/fts_aux_deleted_cache	s_def	161	4	Dynamic	0	General
test/t1	s_def	161	6	Dynamic	0	General
INSERT INTO t1 (a,b) VALUES
('MySQL Tutorial','DBMS stands for DataBase')  ,
('How To Use MySQL Well','After you went through a'),
('Optimizing MySQL','In this tutorial we will show');
INSERT INTO t1 (a,b) VALUES
('1001 MySQL Tricks','1. Never run mysqld as root'),
('MySQL vs. YourSQL','In the following database comparison'),
('MySQL Security','When configured properly, MySQL');
SELECT * FROM t1 WHERE MATCH (a,b)
AGAINST ('Tutorial' IN NATURAL LANGUAGE MODE);
a	b
MySQL Tutorial	DBMS stands for DataBase
Optimizing MySQL	In this tutorial we will show
TRUNCATE TABLE t1;
=== information_schema.innodb_tables  and innodb_tablespaces ===
Table Name	Tablespace	Table Flags	Columns	Row Format	Zip Size	Space Type
mtr/global_suppressions	mtr/global_suppressions	33	4	Dynamic	0	Single
mtr/test_suppressions	mtr/test_suppressions	33	4	Dynamic	0	Single
test/fts_aux_index_1	s_def	161	8	Dynamic	0	General
test/fts_aux_index_2	s_def	161	8	Dynamic	0	General
test/fts_aux_index_3	s_def	161	8	Dynamic	0	General
test/fts_aux_index_4	s_def	161	8	Dynamic	0	General
test/fts_aux_index_5	s_def	161	8	Dynamic	0	General
test/fts_aux_index_6	s_def	161	8	Dynamic	0	General
test/fts_aux_being_deleted	s_def	161	4	Dynamic	0	General
test/fts_aux_being_deleted_cache	s_def	161	4	Dynamic	0	General
test/fts_aux_config	s_def	161	5	Dynamic	0	General
test/fts_aux_deleted	s_def	161	4	Dynamic	0	General
test/fts_aux_deleted_cache	s_def	161	4	Dynamic	0	General
test/t1	s_def	161	6	Dynamic	0	General
INSERT INTO t1 (a,b) VALUES
('MySQL Tutorial','DBMS stands for DataBase')  ,
('How To Use MySQL Well','After you went through a'),
('Optimizing MySQL','In this tutorial we will show');
INSERT INTO t1 (a,b) VALUES
('1001 MySQL Tricks','1. Never run mysqld as root'),
('MySQL vs. YourSQL','In the following database comparison'),
('MySQL Security','When configured properly, MySQL');
SELECT * FROM t1 WHERE MATCH (a,b)
AGAINST ('Tutorial' IN NATURAL LANGUAGE MODE);
a	b
MySQL Tutorial	DBMS stands for DataBase
Optimizing MySQL	In this tutorial we will show
DROP TABLE t1;
#
# tablespace=s_zip row_format=compressed KEY_BLOCK_SIZE=2
#
CREATE TABLE t1 (a VARCHAR(200), b TEXT, FULLTEXT (a,b)) TABLESPACE=s_zip ROW_FORMAT=compressed KEY_BLOCK_SIZE=2;
=== information_schema.innodb_tables  and innodb_tablespaces ===
Table Name	Tablespace	Table Flags	Columns	Row Format	Zip Size	Space Type
mtr/global_suppressions	mtr/global_suppressions	33	4	Dynamic	0	Single
mtr/test_suppressions	mtr/test_suppressions	33	4	Dynamic	0	Single
test/fts_aux_index_1	s_zip	165	8	Compressed	2048	General
test/fts_aux_index_2	s_zip	165	8	Compressed	2048	General
test/fts_aux_index_3	s_zip	165	8	Compressed	2048	General
test/fts_aux_index_4	s_zip	165	8	Compressed	2048	General
test/fts_aux_index_5	s_zip	165	8	Compressed	2048	General
test/fts_aux_index_6	s_zip	165	8	Compressed	2048	General
test/fts_aux_being_deleted	s_zip	165	4	Compressed	2048	General
test/fts_aux_being_deleted_cache	s_zip	165	4	Compressed	2048	General
test/fts_aux_config	s_zip	165	5	Compressed	2048	General
test/fts_aux_deleted	s_zip	165	4	Compressed	2048	General
test/fts_aux_deleted_cache	s_zip	165	4	Compressed	2048	General
test/t1	s_zip	165	6	Compressed	2048	General
INSERT INTO t1 (a,b) VALUES
('MySQL Tutorial','DBMS stands for DataBase')  ,
('How To Use MySQL Well','After you went through a'),
('Optimizing MySQL','In this tutorial we will show');
INSERT INTO t1 (a,b) VALUES
('1001 MySQL Tricks','1. Never run mysqld as root'),
('MySQL vs. YourSQL','In the following database comparison'),
('MySQL Security','When configured properly, MySQL');
SELECT * FROM t1 WHERE MATCH (a,b)
AGAINST ('Tutorial' IN NATURAL LANGUAGE MODE);
a	b
MySQL Tutorial	DBMS stands for DataBase
Optimizing MySQL	In this tutorial we will show
TRUNCATE TABLE t1;
=== information_schema.innodb_tables  and innodb_tablespaces ===
Table Name	Tablespace	Table Flags	Columns	Row Format	Zip Size	Space Type
mtr/global_suppressions	mtr/global_suppressions	33	4	Dynamic	0	Single
mtr/test_suppressions	mtr/test_suppressions	33	4	Dynamic	0	Single
test/fts_aux_index_1	s_zip	165	8	Compressed	2048	General
test/fts_aux_index_2	s_zip	165	8	Compressed	2048	General
test/fts_aux_index_3	s_zip	165	8	Compressed	2048	General
test/fts_aux_index_4	s_zip	165	8	Compressed	2048	General
test/fts_aux_index_5	s_zip	165	8	Compressed	2048	General
test/fts_aux_index_6	s_zip	165	8	Compressed	2048	General
test/fts_aux_being_deleted	s_zip	165	4	Compressed	2048	General
test/fts_aux_being_deleted_cache	s_zip	165	4	Compressed	2048	General
test/fts_aux_config	s_zip	165	5	Compressed	2048	General
test/fts_aux_deleted	s_zip	165	4	Compressed	2048	General
test/fts_aux_deleted_cache	s_zip	165	4	Compressed	2048	General
test/t1	s_zip	165	6	Compressed	2048	General
INSERT INTO t1 (a,b) VALUES
('MySQL Tutorial','DBMS stands for DataBase')  ,
('How To Use MySQL Well','After you went through a'),
('Optimizing MySQL','In this tutorial we will show');
INSERT INTO t1 (a,b) VALUES
('1001 MySQL Tricks','1. Never run mysqld as root'),
('MySQL vs. YourSQL','In the following database comparison'),
('MySQL Security','When configured properly, MySQL');
SELECT * FROM t1 WHERE MATCH (a,b)
AGAINST ('Tutorial' IN NATURAL LANGUAGE MODE);
a	b
MySQL Tutorial	DBMS stands for DataBase
Optimizing MySQL	In this tutorial we will show
DROP TABLE t1;
#
# tablespace=innodb_file_per_table row_format=dynamic
#
CREATE TABLE t1 (a VARCHAR(200), b TEXT, FULLTEXT (a,b)) TABLESPACE=innodb_file_per_table ROW_FORMAT=dynamic ;
=== information_schema.innodb_tables  and innodb_tablespaces ===
Table Name	Tablespace	Table Flags	Columns	Row Format	Zip Size	Space Type
mtr/global_suppressions	mtr/global_suppressions	33	4	Dynamic	0	Single
mtr/test_suppressions	mtr/test_suppressions	33	4	Dynamic	0	Single
test/fts_aux_index_1	test/fts_aux_index_1	33	8	Dynamic	0	Single
test/fts_aux_index_2	test/fts_aux_index_2	33	8	Dynamic	0	Single
test/fts_aux_index_3	test/fts_aux_index_3	33	8	Dynamic	0	Single
test/fts_aux_index_4	test/fts_aux_index_4	33	8	Dynamic	0	Single
test/fts_aux_index_5	test/fts_aux_index_5	33	8	Dynamic	0	Single
test/fts_aux_index_6	test/fts_aux_index_6	33	8	Dynamic	0	Single
test/fts_aux_being_deleted	test/fts_aux_being_deleted	33	4	Dynamic	0	Single
test/fts_aux_being_deleted_cache	test/fts_aux_being_deleted_cache	33	4	Dynamic	0	Single
test/fts_aux_config	test/fts_aux_config	33	5	Dynamic	0	Single
test/fts_aux_deleted	test/fts_aux_deleted	33	4	Dynamic	0	Single
test/fts_aux_deleted_cache	test/fts_aux_deleted_cache	33	4	Dynamic	0	Single
test/t1	test/t1	33	6	Dynamic	0	Single
INSERT INTO t1 (a,b) VALUES
('MySQL Tutorial','DBMS stands for DataBase')  ,
('How To Use MySQL Well','After you went through a'),
('Optimizing MySQL','In this tutorial we will show');
INSERT INTO t1 (a,b) VALUES
('1001 MySQL Tricks','1. Never run mysqld as root'),
('MySQL vs. YourSQL','In the following database comparison'),
('MySQL Security','When configured properly, MySQL');
SELECT * FROM t1 WHERE MATCH (a,b)
AGAINST ('Tutorial' IN NATURAL LANGUAGE MODE);
a	b
MySQL Tutorial	DBMS stands for DataBase
Optimizing MySQL	In this tutorial we will show
TRUNCATE TABLE t1;
=== information_schema.innodb_tables  and innodb_tablespaces ===
Table Name	Tablespace	Table Flags	Columns	Row Format	Zip Size	Space Type
mtr/global_suppressions	mtr/global_suppressions	33	4	Dynamic	0	Single
mtr/test_suppressions	mtr/test_suppressions	33	4	Dynamic	0	Single
test/fts_aux_index_1	test/fts_aux_index_1	33	8	Dynamic	0	Single
test/fts_aux_index_2	test/fts_aux_index_2	33	8	Dynamic	0	Single
test/fts_aux_index_3	test/fts_aux_index_3	33	8	Dynamic	0	Single
test/fts_aux_index_4	test/fts_aux_index_4	33	8	Dynamic	0	Single
test/fts_aux_index_5	test/fts_aux_index_5	33	8	Dynamic	0	Single
test/fts_aux_index_6	test/fts_aux_index_6	33	8	Dynamic	0	Single
test/fts_aux_being_deleted	test/fts_aux_being_deleted	33	4	Dynamic	0	Single
test/fts_aux_being_deleted_cache	test/fts_aux_being_deleted_cache	33	4	Dynamic	0	Single
test/fts_aux_config	test/fts_aux_config	33	5	Dynamic	0	Single
test/fts_aux_deleted	test/fts_aux_deleted	33	4	Dynamic	0	Single
test/fts_aux_deleted_cache	test/fts_aux_deleted_cache	33	4	Dynamic	0	Single
test/t1	test/t1	33	6	Dynamic	0	Single
INSERT INTO t1 (a,b) VALUES
('MySQL Tutorial','DBMS stands for DataBase')  ,
('How To Use MySQL Well','After you went through a'),
('Optimizing MySQL','In this tutorial we will show');
INSERT INTO t1 (a,b) VALUES
('1001 MySQL Tricks','1. Never run mysqld as root'),
('MySQL vs. YourSQL','In the following database comparison'),
('MySQL Security','When configured properly, MySQL');
SELECT * FROM t1 WHERE MATCH (a,b)
AGAINST ('Tutorial' IN NATURAL LANGUAGE MODE);
a	b
MySQL Tutorial	DBMS stands for DataBase
Optimizing MySQL	In this tutorial we will show
DROP TABLE t1;
#
# tablespace=innodb_file_per_table row_format=compressed KEY_BLOCK_SIZE=2
#
CREATE TABLE t1 (a VARCHAR(200), b TEXT, FULLTEXT (a,b)) TABLESPACE=innodb_file_per_table ROW_FORMAT=compressed KEY_BLOCK_SIZE=2;
=== information_schema.innodb_tables  and innodb_tablespaces ===
Table Name	Tablespace	Table Flags	Columns	Row Format	Zip Size	Space Type
mtr/global_suppressions	mtr/global_suppressions	33	4	Dynamic	0	Single
mtr/test_suppressions	mtr/test_suppressions	33	4	Dynamic	0	Single
test/fts_aux_index_1	test/fts_aux_index_1	37	8	Compressed	2048	Single
test/fts_aux_index_2	test/fts_aux_index_2	37	8	Compressed	2048	Single
test/fts_aux_index_3	test/fts_aux_index_3	37	8	Compressed	2048	Single
test/fts_aux_index_4	test/fts_aux_index_4	37	8	Compressed	2048	Single
test/fts_aux_index_5	test/fts_aux_index_5	37	8	Compressed	2048	Single
test/fts_aux_index_6	test/fts_aux_index_6	37	8	Compressed	2048	Single
test/fts_aux_being_deleted	test/fts_aux_being_deleted	37	4	Compressed	2048	Single
test/fts_aux_being_deleted_cache	test/fts_aux_being_deleted_cache	37	4	Compressed	2048	Single
test/fts_aux_config	test/fts_aux_config	37	5	Compressed	2048	Single
test/fts_aux_deleted	test/fts_aux_deleted	37	4	Compressed	2048	Single
test/fts_aux_deleted_cache	test/fts_aux_deleted_cache	37	4	Compressed	2048	Single
test/t1	test/t1	37	6	Compressed	2048	Single
INSERT INTO t1 (a,b) VALUES
('MySQL Tutorial','DBMS stands for DataBase')  ,
('How To Use MySQL Well','After you went through a'),
('Optimizing MySQL','In this tutorial we will show');
INSERT INTO t1 (a,b) VALUES
('1001 MySQL Tricks','1. Never run mysqld as root'),
('MySQL vs. YourSQL','In the following database comparison'),
('MySQL Security','When configured properly, MySQL');
SELECT * FROM t1 WHERE MATCH (a,b)
AGAINST ('Tutorial' IN NATURAL LANGUAGE MODE);
a	b
MySQL Tutorial	DBMS stands for DataBase
Optimizing MySQL	In this tutorial we will show
TRUNCATE TABLE t1;
=== information_schema.innodb_tables  and innodb_tablespaces ===
Table Name	Tablespace	Table Flags	Columns	Row Format	Zip Size	Space Type
mtr/global_suppressions	mtr/global_suppressions	33	4	Dynamic	0	Single
mtr/test_suppressions	mtr/test_suppressions	33	4	Dynamic	0	Single
test/fts_aux_index_1	test/fts_aux_index_1	37	8	Compressed	2048	Single
test/fts_aux_index_2	test/fts_aux_index_2	37	8	Compressed	2048	Single
test/fts_aux_index_3	test/fts_aux_index_3	37	8	Compressed	2048	Single
test/fts_aux_index_4	test/fts_aux_index_4	37	8	Compressed	2048	Single
test/fts_aux_index_5	test/fts_aux_index_5	37	8	Compressed	2048	Single
test/fts_aux_index_6	test/fts_aux_index_6	37	8	Compressed	2048	Single
test/fts_aux_being_deleted	test/fts_aux_being_deleted	37	4	Compressed	2048	Single
test/fts_aux_being_deleted_cache	test/fts_aux_being_deleted_cache	37	4	Compressed	2048	Single
test/fts_aux_config	test/fts_aux_config	37	5	Compressed	2048	Single
test/fts_aux_deleted	test/fts_aux_deleted	37	4	Compressed	2048	Single
test/fts_aux_deleted_cache	test/fts_aux_deleted_cache	37	4	Compressed	2048	Single
test/t1	test/t1	37	6	Compressed	2048	Single
INSERT INTO t1 (a,b) VALUES
('MySQL Tutorial','DBMS stands for DataBase')  ,
('How To Use MySQL Well','After you went through a'),
('Optimizing MySQL','In this tutorial we will show');
INSERT INTO t1 (a,b) VALUES
('1001 MySQL Tricks','1. Never run mysqld as root'),
('MySQL vs. YourSQL','In the following database comparison'),
('MySQL Security','When configured properly, MySQL');
SELECT * FROM t1 WHERE MATCH (a,b)
AGAINST ('Tutorial' IN NATURAL LANGUAGE MODE);
a	b
MySQL Tutorial	DBMS stands for DataBase
Optimizing MySQL	In this tutorial we will show
DROP TABLE t1;
#
# tablespace=innodb_system row_format=redundant
#
CREATE TABLE t1 (a VARCHAR(200), b TEXT, FULLTEXT (a,b)) TABLESPACE=innodb_system ROW_FORMAT=redundant ;
=== information_schema.innodb_tables  and innodb_tablespaces ===
Table Name	Tablespace	Table Flags	Columns	Row Format	Zip Size	Space Type
mtr/global_suppressions	mtr/global_suppressions	33	4	Dynamic	0	Single
mtr/test_suppressions	mtr/test_suppressions	33	4	Dynamic	0	Single
test/fts_aux_index_1	innodb_system	128	8	Redundant	0	System
test/fts_aux_index_2	innodb_system	128	8	Redundant	0	System
test/fts_aux_index_3	innodb_system	128	8	Redundant	0	System
test/fts_aux_index_4	innodb_system	128	8	Redundant	0	System
test/fts_aux_index_5	innodb_system	128	8	Redundant	0	System
test/fts_aux_index_6	innodb_system	128	8	Redundant	0	System
test/fts_aux_being_deleted	innodb_system	128	4	Redundant	0	System
test/fts_aux_being_deleted_cache	innodb_system	128	4	Redundant	0	System
test/fts_aux_config	innodb_system	128	5	Redundant	0	System
test/fts_aux_deleted	innodb_system	128	4	Redundant	0	System
test/fts_aux_deleted_cache	innodb_system	128	4	Redundant	0	System
test/t1	innodb_system	128	6	Redundant	0	System
INSERT INTO t1 (a,b) VALUES
('MySQL Tutorial','DBMS stands for DataBase')  ,
('How To Use MySQL Well','After you went through a'),
('Optimizing MySQL','In this tutorial we will show');
INSERT INTO t1 (a,b) VALUES
('1001 MySQL Tricks','1. Never run mysqld as root'),
('MySQL vs. YourSQL','In the following database comparison'),
('MySQL Security','When configured properly, MySQL');
SELECT * FROM t1 WHERE MATCH (a,b)
AGAINST ('Tutorial' IN NATURAL LANGUAGE MODE);
a	b
MySQL Tutorial	DBMS stands for DataBase
Optimizing MySQL	In this tutorial we will show
TRUNCATE TABLE t1;
=== information_schema.innodb_tables  and innodb_tablespaces ===
Table Name	Tablespace	Table Flags	Columns	Row Format	Zip Size	Space Type
mtr/global_suppressions	mtr/global_suppressions	33	4	Dynamic	0	Single
mtr/test_suppressions	mtr/test_suppressions	33	4	Dynamic	0	Single
test/fts_aux_index_1	innodb_system	128	8	Redundant	0	System
test/fts_aux_index_2	innodb_system	128	8	Redundant	0	System
test/fts_aux_index_3	innodb_system	128	8	Redundant	0	System
test/fts_aux_index_4	innodb_system	128	8	Redundant	0	System
test/fts_aux_index_5	innodb_system	128	8	Redundant	0	System
test/fts_aux_index_6	innodb_system	128	8	Redundant	0	System
test/fts_aux_being_deleted	innodb_system	128	4	Redundant	0	System
test/fts_aux_being_deleted_cache	innodb_system	128	4	Redundant	0	System
test/fts_aux_config	innodb_system	128	5	Redundant	0	System
test/fts_aux_deleted	innodb_system	128	4	Redundant	0	System
test/fts_aux_deleted_cache	innodb_system	128	4	Redundant	0	System
test/t1	innodb_system	128	6	Redundant	0	System
INSERT INTO t1 (a,b) VALUES
('MySQL Tutorial','DBMS stands for DataBase')  ,
('How To Use MySQL Well','After you went through a'),
('Optimizing MySQL','In this tutorial we will show');
INSERT INTO t1 (a,b) VALUES
('1001 MySQL Tricks','1. Never run mysqld as root'),
('MySQL vs. YourSQL','In the following database comparison'),
('MySQL Security','When configured properly, MySQL');
SELECT * FROM t1 WHERE MATCH (a,b)
AGAINST ('Tutorial' IN NATURAL LANGUAGE MODE);
a	b
MySQL Tutorial	DBMS stands for DataBase
Optimizing MySQL	In this tutorial we will show
DROP TABLE t1;
#
# tablespace=innodb_system row_format=compact
#
CREATE TABLE t1 (a VARCHAR(200), b TEXT, FULLTEXT (a,b)) TABLESPACE=innodb_system ROW_FORMAT=compact ;
=== information_schema.innodb_tables  and innodb_tablespaces ===
Table Name	Tablespace	Table Flags	Columns	Row Format	Zip Size	Space Type
mtr/global_suppressions	mtr/global_suppressions	33	4	Dynamic	0	Single
mtr/test_suppressions	mtr/test_suppressions	33	4	Dynamic	0	Single
test/fts_aux_index_1	innodb_system	129	8	Compact	0	System
test/fts_aux_index_2	innodb_system	129	8	Compact	0	System
test/fts_aux_index_3	innodb_system	129	8	Compact	0	System
test/fts_aux_index_4	innodb_system	129	8	Compact	0	System
test/fts_aux_index_5	innodb_system	129	8	Compact	0	System
test/fts_aux_index_6	innodb_system	129	8	Compact	0	System
test/fts_aux_being_deleted	innodb_system	129	4	Compact	0	System
test/fts_aux_being_deleted_cache	innodb_system	129	4	Compact	0	System
test/fts_aux_config	innodb_system	129	5	Compact	0	System
test/fts_aux_deleted	innodb_system	129	4	Compact	0	System
test/fts_aux_deleted_cache	innodb_system	129	4	Compact	0	System
test/t1	innodb_system	129	6	Compact	0	System
INSERT INTO t1 (a,b) VALUES
('MySQL Tutorial','DBMS stands for DataBase')  ,
('How To Use MySQL Well','After you went through a'),
('Optimizing MySQL','In this tutorial we will show');
INSERT INTO t1 (a,b) VALUES
('1001 MySQL Tricks','1. Never run mysqld as root'),
('MySQL vs. YourSQL','In the following database comparison'),
('MySQL Security','When configured properly, MySQL');
SELECT * FROM t1 WHERE MATCH (a,b)
AGAINST ('Tutorial' IN NATURAL LANGUAGE MODE);
a	b
MySQL Tutorial	DBMS stands for DataBase
Optimizing MySQL	In this tutorial we will show
TRUNCATE TABLE t1;
=== information_schema.innodb_tables  and innodb_tablespaces ===
Table Name	Tablespace	Table Flags	Columns	Row Format	Zip Size	Space Type
mtr/global_suppressions	mtr/global_suppressions	33	4	Dynamic	0	Single
mtr/test_suppressions	mtr/test_suppressions	33	4	Dynamic	0	Single
test/fts_aux_index_1	innodb_system	129	8	Compact	0	System
test/fts_aux_index_2	innodb_system	129	8	Compact	0	System
test/fts_aux_index_3	innodb_system	129	8	Compact	0	System
test/fts_aux_index_4	innodb_system	129	8	Compact	0	System
test/fts_aux_index_5	innodb_system	129	8	Compact	0	System
test/fts_aux_index_6	innodb_system	129	8	Compact	0	System
test/fts_aux_being_deleted	innodb_system	129	4	Compact	0	System
test/fts_aux_being_deleted_cache	innodb_system	129	4	Compact	0	System
test/fts_aux_config	innodb_system	129	5	Compact	0	System
test/fts_aux_deleted	innodb_system	129	4	Compact	0	System
test/fts_aux_deleted_cache	innodb_system	129	4	Compact	0	System
test/t1	innodb_system	129	6	Compact	0	System
INSERT INTO t1 (a,b) VALUES
('MySQL Tutorial','DBMS stands for DataBase')  ,
('How To Use MySQL Well','After you went through a'),
('Optimizing MySQL','In this tutorial we will show');
INSERT INTO t1 (a,b) VALUES
('1001 MySQL Tricks','1. Never run mysqld as root'),
('MySQL vs. YourSQL','In the following database comparison'),
('MySQL Security','When configured properly, MySQL');
SELECT * FROM t1 WHERE MATCH (a,b)
AGAINST ('Tutorial' IN NATURAL LANGUAGE MODE);
a	b
MySQL Tutorial	DBMS stands for DataBase
Optimizing MySQL	In this tutorial we will show
DROP TABLE t1;
#
# tablespace=innodb_system row_format=dynamic
#
CREATE TABLE t1 (a VARCHAR(200), b TEXT, FULLTEXT (a,b)) TABLESPACE=innodb_system ROW_FORMAT=dynamic ;
=== information_schema.innodb_tables  and innodb_tablespaces ===
Table Name	Tablespace	Table Flags	Columns	Row Format	Zip Size	Space Type
mtr/global_suppressions	mtr/global_suppressions	33	4	Dynamic	0	Single
mtr/test_suppressions	mtr/test_suppressions	33	4	Dynamic	0	Single
test/fts_aux_index_1	innodb_system	161	8	Dynamic	0	System
test/fts_aux_index_2	innodb_system	161	8	Dynamic	0	System
test/fts_aux_index_3	innodb_system	161	8	Dynamic	0	System
test/fts_aux_index_4	innodb_system	161	8	Dynamic	0	System
test/fts_aux_index_5	innodb_system	161	8	Dynamic	0	System
test/fts_aux_index_6	innodb_system	161	8	Dynamic	0	System
test/fts_aux_being_deleted	innodb_system	161	4	Dynamic	0	System
test/fts_aux_being_deleted_cache	innodb_system	161	4	Dynamic	0	System
test/fts_aux_config	innodb_system	161	5	Dynamic	0	System
test/fts_aux_deleted	innodb_system	161	4	Dynamic	0	System
test/fts_aux_deleted_cache	innodb_system	161	4	Dynamic	0	System
test/t1	innodb_system	161	6	Dynamic	0	System
INSERT INTO t1 (a,b) VALUES
('MySQL Tutorial','DBMS stands for DataBase')  ,
('How To Use MySQL Well','After you went through a'),
('Optimizing MySQL','In this tutorial we will show');
INSERT INTO t1 (a,b) VALUES
('1001 MySQL Tricks','1. Never run mysqld as root'),
('MySQL vs. YourSQL','In the following database comparison'),
('MySQL Security','When configured properly, MySQL');
SELECT * FROM t1 WHERE MATCH (a,b)
AGAINST ('Tutorial' IN NATURAL LANGUAGE MODE);
a	b
MySQL Tutorial	DBMS stands for DataBase
Optimizing MySQL	In this tutorial we will show
TRUNCATE TABLE t1;
=== information_schema.innodb_tables  and innodb_tablespaces ===
Table Name	Tablespace	Table Flags	Columns	Row Format	Zip Size	Space Type
mtr/global_suppressions	mtr/global_suppressions	33	4	Dynamic	0	Single
mtr/test_suppressions	mtr/test_suppressions	33	4	Dynamic	0	Single
test/fts_aux_index_1	innodb_system	161	8	Dynamic	0	System
test/fts_aux_index_2	innodb_system	161	8	Dynamic	0	System
test/fts_aux_index_3	innodb_system	161	8	Dynamic	0	System
test/fts_aux_index_4	innodb_system	161	8	Dynamic	0	System
test/fts_aux_index_5	innodb_system	161	8	Dynamic	0	System
test/fts_aux_index_6	innodb_system	161	8	Dynamic	0	System
test/fts_aux_being_deleted	innodb_system	161	4	Dynamic	0	System
test/fts_aux_being_deleted_cache	innodb_system	161	4	Dynamic	0	System
test/fts_aux_config	innodb_system	161	5	Dynamic	0	System
test/fts_aux_deleted	innodb_system	161	4	Dynamic	0	System
test/fts_aux_deleted_cache	innodb_system	161	4	Dynamic	0	System
test/t1	innodb_system	161	6	Dynamic	0	System
INSERT INTO t1 (a,b) VALUES
('MySQL Tutorial','DBMS stands for DataBase')  ,
('How To Use MySQL Well','After you went through a'),
('Optimizing MySQL','In this tutorial we will show');
INSERT INTO t1 (a,b) VALUES
('1001 MySQL Tricks','1. Never run mysqld as root'),
('MySQL vs. YourSQL','In the following database comparison'),
('MySQL Security','When configured properly, MySQL');
SELECT * FROM t1 WHERE MATCH (a,b)
AGAINST ('Tutorial' IN NATURAL LANGUAGE MODE);
a	b
MySQL Tutorial	DBMS stands for DataBase
Optimizing MySQL	In this tutorial we will show
DROP TABLE t1;
#
# Without TABLESPACE=, innodb_file_per_table=ON(default);
#
CREATE TABLE t1 (a VARCHAR(200), b TEXT, FULLTEXT (a,b));;
=== information_schema.innodb_tables  and innodb_tablespaces ===
Table Name	Tablespace	Table Flags	Columns	Row Format	Zip Size	Space Type
mtr/global_suppressions	mtr/global_suppressions	33	4	Dynamic	0	Single
mtr/test_suppressions	mtr/test_suppressions	33	4	Dynamic	0	Single
test/fts_aux_index_1	test/fts_aux_index_1	33	8	Dynamic	0	Single
test/fts_aux_index_2	test/fts_aux_index_2	33	8	Dynamic	0	Single
test/fts_aux_index_3	test/fts_aux_index_3	33	8	Dynamic	0	Single
test/fts_aux_index_4	test/fts_aux_index_4	33	8	Dynamic	0	Single
test/fts_aux_index_5	test/fts_aux_index_5	33	8	Dynamic	0	Single
test/fts_aux_index_6	test/fts_aux_index_6	33	8	Dynamic	0	Single
test/fts_aux_being_deleted	test/fts_aux_being_deleted	33	4	Dynamic	0	Single
test/fts_aux_being_deleted_cache	test/fts_aux_being_deleted_cache	33	4	Dynamic	0	Single
test/fts_aux_config	test/fts_aux_config	33	5	Dynamic	0	Single
test/fts_aux_deleted	test/fts_aux_deleted	33	4	Dynamic	0	Single
test/fts_aux_deleted_cache	test/fts_aux_deleted_cache	33	4	Dynamic	0	Single
test/t1	test/t1	33	6	Dynamic	0	Single
DROP TABLE t1;
#
# Without TABLESPACE=, innodb_file_per_table=OFF
#
SET GLOBAL innodb_file_per_table=OFF;
CREATE TABLE t1 (a VARCHAR(200), b TEXT, FULLTEXT (a,b));;
=== information_schema.innodb_tables  and innodb_tablespaces ===
Table Name	Tablespace	Table Flags	Columns	Row Format	Zip Size	Space Type
mtr/global_suppressions	mtr/global_suppressions	33	4	Dynamic	0	Single
mtr/test_suppressions	mtr/test_suppressions	33	4	Dynamic	0	Single
test/fts_aux_index_1	innodb_system	33	8	Dynamic	0	System
test/fts_aux_index_2	innodb_system	33	8	Dynamic	0	System
test/fts_aux_index_3	innodb_system	33	8	Dynamic	0	System
test/fts_aux_index_4	innodb_system	33	8	Dynamic	0	System
test/fts_aux_index_5	innodb_system	33	8	Dynamic	0	System
test/fts_aux_index_6	innodb_system	33	8	Dynamic	0	System
test/fts_aux_being_deleted	innodb_system	33	4	Dynamic	0	System
test/fts_aux_being_deleted_cache	innodb_system	33	4	Dynamic	0	System
test/fts_aux_config	innodb_system	33	5	Dynamic	0	System
test/fts_aux_deleted	innodb_system	33	4	Dynamic	0	System
test/fts_aux_deleted_cache	innodb_system	33	4	Dynamic	0	System
test/t1	innodb_system	33	6	Dynamic	0	System
DROP TABLE t1;
SET GLOBAL innodb_file_per_table=default;
CREATE TEMPORARY TABLE t1 (a VARCHAR(200), b TEXT, FULLTEXT(a,b))
TABLESPACE=innodb_temporary ROW_FORMAT=compact;
ERROR HY000: Cannot create FULLTEXT index on temporary InnoDB table
# Test with FTS TABLES in remote DATA DIRECTORY
CREATE TABLE t1 (a VARCHAR(200), b TEXT, FULLTEXT (a,b)) DATA DIRECTORY 'REMOTE_DIR';
=== information_schema.innodb_tables  and innodb_tablespaces ===
Table Name	Tablespace	Table Flags	Columns	Row Format	Zip Size	Space Type
mtr/global_suppressions	mtr/global_suppressions	33	4	Dynamic	0	Single
mtr/test_suppressions	mtr/test_suppressions	33	4	Dynamic	0	Single
test/fts_aux_index_1	test/fts_aux_index_1	97	8	Dynamic	0	Single
test/fts_aux_index_2	test/fts_aux_index_2	97	8	Dynamic	0	Single
test/fts_aux_index_3	test/fts_aux_index_3	97	8	Dynamic	0	Single
test/fts_aux_index_4	test/fts_aux_index_4	97	8	Dynamic	0	Single
test/fts_aux_index_5	test/fts_aux_index_5	97	8	Dynamic	0	Single
test/fts_aux_index_6	test/fts_aux_index_6	97	8	Dynamic	0	Single
test/fts_aux_being_deleted	test/fts_aux_being_deleted	97	4	Dynamic	0	Single
test/fts_aux_being_deleted_cache	test/fts_aux_being_deleted_cache	97	4	Dynamic	0	Single
test/fts_aux_config	test/fts_aux_config	97	5	Dynamic	0	Single
test/fts_aux_deleted	test/fts_aux_deleted	97	4	Dynamic	0	Single
test/fts_aux_deleted_cache	test/fts_aux_deleted_cache	97	4	Dynamic	0	Single
test/t1	test/t1	97	6	Dynamic	0	Single
# list files in REMOTE_DIR
fts_aux_index_1.ibd
fts_aux_index_2.ibd
fts_aux_index_3.ibd
fts_aux_index_4.ibd
fts_aux_index_5.ibd
fts_aux_index_6.ibd
fts_aux_being_deleted.ibd
fts_aux_being_deleted_cache.ibd
fts_aux_config.ibd
fts_aux_deleted.ibd
fts_aux_deleted_cache.ibd
t1.ibd
TRUNCATE TABLE t1;
# restart
TRUNCATE TABLE t1;
# list files in REMOTE_DIR after TRUNCATE
fts_aux_index_1.ibd
fts_aux_index_2.ibd
fts_aux_index_3.ibd
fts_aux_index_4.ibd
fts_aux_index_5.ibd
fts_aux_index_6.ibd
fts_aux_being_deleted.ibd
fts_aux_being_deleted_cache.ibd
fts_aux_config.ibd
fts_aux_deleted.ibd
fts_aux_deleted_cache.ibd
t1.ibd
DROP TABLE t1;
# list files after DROP TABLE. Should be empty
# Test with FTS , IMPORT & EXPORT tablespace
CREATE TABLE t1 (a VARCHAR(200), b TEXT, FULLTEXT (a,b)) TABLESPACE=s_def;
FLUSH TABLES t1 FOR EXPORT;
Warnings:
Warning	1235	InnoDB: This version of MySQL doesn't yet support 'FLUSH TABLES FOR EXPORT on table `test`.`t1` in a general tablespace.'
UNLOCK TABLES;
DROP TABLE t1;
# Test with ALTER. Add a FTS Index and also move the current
# table to general tablespace
CREATE TABLE t1 (
id INT UNSIGNED AUTO_INCREMENT NOT NULL PRIMARY KEY,
title VARCHAR(200)
) ENGINE=InnoDB;
=== information_schema.innodb_tables  and innodb_tablespaces ===
Table Name	Tablespace	Table Flags	Columns	Row Format	Zip Size	Space Type
mtr/global_suppressions	mtr/global_suppressions	33	4	Dynamic	0	Single
mtr/test_suppressions	mtr/test_suppressions	33	4	Dynamic	0	Single
test/t1	test/t1	33	5	Dynamic	0	Single
ALTER TABLE t1 ADD FULLTEXT INDEX (title), TABLESPACE=s_def;
Warnings:
Warning	124	InnoDB rebuilding table to add column FTS_DOC_ID
=== information_schema.innodb_tables  and innodb_tablespaces ===
Table Name	Tablespace	Table Flags	Columns	Row Format	Zip Size	Space Type
mtr/global_suppressions	mtr/global_suppressions	33	4	Dynamic	0	Single
mtr/test_suppressions	mtr/test_suppressions	33	4	Dynamic	0	Single
test/fts_aux_index_1	s_def	161	8	Dynamic	0	General
test/fts_aux_index_2	s_def	161	8	Dynamic	0	General
test/fts_aux_index_3	s_def	161	8	Dynamic	0	General
test/fts_aux_index_4	s_def	161	8	Dynamic	0	General
test/fts_aux_index_5	s_def	161	8	Dynamic	0	General
test/fts_aux_index_6	s_def	161	8	Dynamic	0	General
test/fts_aux_being_deleted	s_def	161	4	Dynamic	0	General
test/fts_aux_being_deleted_cache	s_def	161	4	Dynamic	0	General
test/fts_aux_config	s_def	161	5	Dynamic	0	General
test/fts_aux_deleted	s_def	161	4	Dynamic	0	General
test/fts_aux_deleted_cache	s_def	161	4	Dynamic	0	General
test/t1	s_def	161	6	Dynamic	0	General
DROP TABLE t1;
CREATE TABLE t1 (a VARCHAR(200), b TEXT, FULLTEXT fts_idx(a,b), c INT PRIMARY KEY) TABLESPACE=s_def ROW_FORMAT=compact;
=== information_schema.innodb_tables  and innodb_tablespaces ===
Table Name	Tablespace	Table Flags	Columns	Row Format	Zip Size	Space Type
mtr/global_suppressions	mtr/global_suppressions	33	4	Dynamic	0	Single
mtr/test_suppressions	mtr/test_suppressions	33	4	Dynamic	0	Single
test/fts_aux_index_1	s_def	129	8	Compact	0	General
test/fts_aux_index_2	s_def	129	8	Compact	0	General
test/fts_aux_index_3	s_def	129	8	Compact	0	General
test/fts_aux_index_4	s_def	129	8	Compact	0	General
test/fts_aux_index_5	s_def	129	8	Compact	0	General
test/fts_aux_index_6	s_def	129	8	Compact	0	General
test/fts_aux_being_deleted	s_def	129	4	Compact	0	General
test/fts_aux_being_deleted_cache	s_def	129	4	Compact	0	General
test/fts_aux_config	s_def	129	5	Compact	0	General
test/fts_aux_deleted	s_def	129	4	Compact	0	General
test/fts_aux_deleted_cache	s_def	129	4	Compact	0	General
test/t1	s_def	129	7	Compact	0	General
INSERT INTO t1 VALUES
('MySQL Tutorial','DBMS stands for DataBase', 1)  ,
('How To Use MySQL Well','After you went through a', 2),
('Optimizing MySQL','In this tutorial we will show', 3);
INSERT INTO t1 VALUES
('1001 MySQL Tricks','1. Never run mysqld as root', 4),
('MySQL vs. YourSQL','In the following database comparison', 5),
('MySQL Security','When configured properly, MySQL', 6);
SELECT c FROM t1 WHERE MATCH (a,b)
AGAINST ('Tutorial' IN NATURAL LANGUAGE MODE);
c
1
3
ALTER  TABLE t1 ADD UNIQUE INDEX idx1(b(20)), ALGORITHM=INPLACE;
=== information_schema.innodb_tables  and innodb_tablespaces ===
Table Name	Tablespace	Table Flags	Columns	Row Format	Zip Size	Space Type
mtr/global_suppressions	mtr/global_suppressions	33	4	Dynamic	0	Single
mtr/test_suppressions	mtr/test_suppressions	33	4	Dynamic	0	Single
test/fts_aux_index_1	s_def	129	8	Compact	0	General
test/fts_aux_index_2	s_def	129	8	Compact	0	General
test/fts_aux_index_3	s_def	129	8	Compact	0	General
test/fts_aux_index_4	s_def	129	8	Compact	0	General
test/fts_aux_index_5	s_def	129	8	Compact	0	General
test/fts_aux_index_6	s_def	129	8	Compact	0	General
test/fts_aux_being_deleted	s_def	129	4	Compact	0	General
test/fts_aux_being_deleted_cache	s_def	129	4	Compact	0	General
test/fts_aux_config	s_def	129	5	Compact	0	General
test/fts_aux_deleted	s_def	129	4	Compact	0	General
test/fts_aux_deleted_cache	s_def	129	4	Compact	0	General
test/t1	s_def	129	7	Compact	0	General
SELECT c FROM t1 WHERE MATCH (a,b)
AGAINST ('Tutorial' IN NATURAL LANGUAGE MODE);
c
1
3
# Test with ALGORITHM=COPY and with FTS Index
ALTER TABLE t1 ADD COLUMN pk INT NOT NULL AUTO_INCREMENT, DROP PRIMARY KEY, ADD PRIMARY KEY(pk), ALGORITHM=copy;
=== information_schema.innodb_tables  and innodb_tablespaces ===
Table Name	Tablespace	Table Flags	Columns	Row Format	Zip Size	Space Type
mtr/global_suppressions	mtr/global_suppressions	33	4	Dynamic	0	Single
mtr/test_suppressions	mtr/test_suppressions	33	4	Dynamic	0	Single
test/fts_aux_index_1	s_def	129	8	Compact	0	General
test/fts_aux_index_2	s_def	129	8	Compact	0	General
test/fts_aux_index_3	s_def	129	8	Compact	0	General
test/fts_aux_index_4	s_def	129	8	Compact	0	General
test/fts_aux_index_5	s_def	129	8	Compact	0	General
test/fts_aux_index_6	s_def	129	8	Compact	0	General
test/fts_aux_being_deleted	s_def	129	4	Compact	0	General
test/fts_aux_being_deleted_cache	s_def	129	4	Compact	0	General
test/fts_aux_config	s_def	129	5	Compact	0	General
test/fts_aux_deleted	s_def	129	4	Compact	0	General
test/fts_aux_deleted_cache	s_def	129	4	Compact	0	General
test/t1	s_def	129	8	Compact	0	General
SELECT pk FROM t1 WHERE MATCH (a,b)
AGAINST ('Tutorial' IN NATURAL LANGUAGE MODE);
pk
1
3
# Before drop FTS index
=== information_schema.innodb_tables  and innodb_tablespaces ===
Table Name	Tablespace	Table Flags	Columns	Row Format	Zip Size	Space Type
mtr/global_suppressions	mtr/global_suppressions	33	4	Dynamic	0	Single
mtr/test_suppressions	mtr/test_suppressions	33	4	Dynamic	0	Single
test/fts_aux_index_1	s_def	129	8	Compact	0	General
test/fts_aux_index_2	s_def	129	8	Compact	0	General
test/fts_aux_index_3	s_def	129	8	Compact	0	General
test/fts_aux_index_4	s_def	129	8	Compact	0	General
test/fts_aux_index_5	s_def	129	8	Compact	0	General
test/fts_aux_index_6	s_def	129	8	Compact	0	General
test/fts_aux_being_deleted	s_def	129	4	Compact	0	General
test/fts_aux_being_deleted_cache	s_def	129	4	Compact	0	General
test/fts_aux_config	s_def	129	5	Compact	0	General
test/fts_aux_deleted	s_def	129	4	Compact	0	General
test/fts_aux_deleted_cache	s_def	129	4	Compact	0	General
test/t1	s_def	129	8	Compact	0	General
# Test with DROP FTS Index & ADD FTS Index
ALTER TABLE t1 DROP INDEX fts_idx, ADD FULLTEXT INDEX fts_idx(a,b);
=== information_schema.innodb_tables  and innodb_tablespaces ===
Table Name	Tablespace	Table Flags	Columns	Row Format	Zip Size	Space Type
mtr/global_suppressions	mtr/global_suppressions	33	4	Dynamic	0	Single
mtr/test_suppressions	mtr/test_suppressions	33	4	Dynamic	0	Single
test/fts_aux_index_1	s_def	129	8	Compact	0	General
test/fts_aux_index_2	s_def	129	8	Compact	0	General
test/fts_aux_index_3	s_def	129	8	Compact	0	General
test/fts_aux_index_4	s_def	129	8	Compact	0	General
test/fts_aux_index_5	s_def	129	8	Compact	0	General
test/fts_aux_index_6	s_def	129	8	Compact	0	General
test/fts_aux_being_deleted	s_def	129	4	Compact	0	General
test/fts_aux_being_deleted_cache	s_def	129	4	Compact	0	General
test/fts_aux_config	s_def	129	5	Compact	0	General
test/fts_aux_deleted	s_def	129	4	Compact	0	General
test/fts_aux_deleted_cache	s_def	129	4	Compact	0	General
test/t1	s_def	129	8	Compact	0	General
SELECT pk FROM t1 WHERE MATCH (a,b)
AGAINST ('Tutorial' IN NATURAL LANGUAGE MODE);
pk
1
3
DROP TABLE t1;
# Test with file-per-table tablespace
CREATE TABLE t1 (a VARCHAR(200), b TEXT, FULLTEXT fts_idx(a,b), c INT PRIMARY KEY) ;
=== information_schema.innodb_tables  and innodb_tablespaces ===
Table Name	Tablespace	Table Flags	Columns	Row Format	Zip Size	Space Type
mtr/global_suppressions	mtr/global_suppressions	33	4	Dynamic	0	Single
mtr/test_suppressions	mtr/test_suppressions	33	4	Dynamic	0	Single
test/fts_aux_index_1	test/fts_aux_index_1	33	8	Dynamic	0	Single
test/fts_aux_index_2	test/fts_aux_index_2	33	8	Dynamic	0	Single
test/fts_aux_index_3	test/fts_aux_index_3	33	8	Dynamic	0	Single
test/fts_aux_index_4	test/fts_aux_index_4	33	8	Dynamic	0	Single
test/fts_aux_index_5	test/fts_aux_index_5	33	8	Dynamic	0	Single
test/fts_aux_index_6	test/fts_aux_index_6	33	8	Dynamic	0	Single
test/fts_aux_being_deleted	test/fts_aux_being_deleted	33	4	Dynamic	0	Single
test/fts_aux_being_deleted_cache	test/fts_aux_being_deleted_cache	33	4	Dynamic	0	Single
test/fts_aux_config	test/fts_aux_config	33	5	Dynamic	0	Single
test/fts_aux_deleted	test/fts_aux_deleted	33	4	Dynamic	0	Single
test/fts_aux_deleted_cache	test/fts_aux_deleted_cache	33	4	Dynamic	0	Single
test/t1	test/t1	33	7	Dynamic	0	Single
INSERT INTO t1 VALUES
('MySQL Tutorial','DBMS stands for DataBase', 1)  ,
('How To Use MySQL Well','After you went through a', 2),
('Optimizing MySQL','In this tutorial we will show', 3);
INSERT INTO t1 VALUES
('1001 MySQL Tricks','1. Never run mysqld as root', 4),
('MySQL vs. YourSQL','In the following database comparison', 5),
('MySQL Security','When configured properly, MySQL', 6);
SELECT c FROM t1 WHERE MATCH (a,b)
AGAINST ('Tutorial' IN NATURAL LANGUAGE MODE);
c
1
3
ALTER  TABLE t1 ADD UNIQUE INDEX idx1(b(20)), ALGORITHM=INPLACE;
=== information_schema.innodb_tables  and innodb_tablespaces ===
Table Name	Tablespace	Table Flags	Columns	Row Format	Zip Size	Space Type
mtr/global_suppressions	mtr/global_suppressions	33	4	Dynamic	0	Single
mtr/test_suppressions	mtr/test_suppressions	33	4	Dynamic	0	Single
test/fts_aux_index_1	test/fts_aux_index_1	33	8	Dynamic	0	Single
test/fts_aux_index_2	test/fts_aux_index_2	33	8	Dynamic	0	Single
test/fts_aux_index_3	test/fts_aux_index_3	33	8	Dynamic	0	Single
test/fts_aux_index_4	test/fts_aux_index_4	33	8	Dynamic	0	Single
test/fts_aux_index_5	test/fts_aux_index_5	33	8	Dynamic	0	Single
test/fts_aux_index_6	test/fts_aux_index_6	33	8	Dynamic	0	Single
test/fts_aux_being_deleted	test/fts_aux_being_deleted	33	4	Dynamic	0	Single
test/fts_aux_being_deleted_cache	test/fts_aux_being_deleted_cache	33	4	Dynamic	0	Single
test/fts_aux_config	test/fts_aux_config	33	5	Dynamic	0	Single
test/fts_aux_deleted	test/fts_aux_deleted	33	4	Dynamic	0	Single
test/fts_aux_deleted_cache	test/fts_aux_deleted_cache	33	4	Dynamic	0	Single
test/t1	test/t1	33	7	Dynamic	0	Single
SELECT c FROM t1 WHERE MATCH (a,b)
AGAINST ('Tutorial' IN NATURAL LANGUAGE MODE);
c
1
3
# Test with ALGORITHM=COPY and with FTS Index
ALTER TABLE t1 ADD COLUMN pk INT NOT NULL AUTO_INCREMENT, DROP PRIMARY KEY, ADD PRIMARY KEY(pk), ALGORITHM=copy;
=== information_schema.innodb_tables  and innodb_tablespaces ===
Table Name	Tablespace	Table Flags	Columns	Row Format	Zip Size	Space Type
mtr/global_suppressions	mtr/global_suppressions	33	4	Dynamic	0	Single
mtr/test_suppressions	mtr/test_suppressions	33	4	Dynamic	0	Single
test/fts_aux_index_1	test/fts_aux_index_1	33	8	Dynamic	0	Single
test/fts_aux_index_2	test/fts_aux_index_2	33	8	Dynamic	0	Single
test/fts_aux_index_3	test/fts_aux_index_3	33	8	Dynamic	0	Single
test/fts_aux_index_4	test/fts_aux_index_4	33	8	Dynamic	0	Single
test/fts_aux_index_5	test/fts_aux_index_5	33	8	Dynamic	0	Single
test/fts_aux_index_6	test/fts_aux_index_6	33	8	Dynamic	0	Single
test/fts_aux_being_deleted	test/fts_aux_being_deleted	33	4	Dynamic	0	Single
test/fts_aux_being_deleted_cache	test/fts_aux_being_deleted_cache	33	4	Dynamic	0	Single
test/fts_aux_config	test/fts_aux_config	33	5	Dynamic	0	Single
test/fts_aux_deleted	test/fts_aux_deleted	33	4	Dynamic	0	Single
test/fts_aux_deleted_cache	test/fts_aux_deleted_cache	33	4	Dynamic	0	Single
test/t1	test/t1	33	8	Dynamic	0	Single
SELECT pk FROM t1 WHERE MATCH (a,b)
AGAINST ('Tutorial' IN NATURAL LANGUAGE MODE);
pk
1
3
# Before drop FTS index
=== information_schema.innodb_tables  and innodb_tablespaces ===
Table Name	Tablespace	Table Flags	Columns	Row Format	Zip Size	Space Type
mtr/global_suppressions	mtr/global_suppressions	33	4	Dynamic	0	Single
mtr/test_suppressions	mtr/test_suppressions	33	4	Dynamic	0	Single
test/fts_aux_index_1	test/fts_aux_index_1	33	8	Dynamic	0	Single
test/fts_aux_index_2	test/fts_aux_index_2	33	8	Dynamic	0	Single
test/fts_aux_index_3	test/fts_aux_index_3	33	8	Dynamic	0	Single
test/fts_aux_index_4	test/fts_aux_index_4	33	8	Dynamic	0	Single
test/fts_aux_index_5	test/fts_aux_index_5	33	8	Dynamic	0	Single
test/fts_aux_index_6	test/fts_aux_index_6	33	8	Dynamic	0	Single
test/fts_aux_being_deleted	test/fts_aux_being_deleted	33	4	Dynamic	0	Single
test/fts_aux_being_deleted_cache	test/fts_aux_being_deleted_cache	33	4	Dynamic	0	Single
test/fts_aux_config	test/fts_aux_config	33	5	Dynamic	0	Single
test/fts_aux_deleted	test/fts_aux_deleted	33	4	Dynamic	0	Single
test/fts_aux_deleted_cache	test/fts_aux_deleted_cache	33	4	Dynamic	0	Single
test/t1	test/t1	33	8	Dynamic	0	Single
# Test with DROP FTS Index & ADD FTS Index
ALTER TABLE t1 DROP INDEX fts_idx, ADD FULLTEXT INDEX fts_idx(a,b);
=== information_schema.innodb_tables  and innodb_tablespaces ===
Table Name	Tablespace	Table Flags	Columns	Row Format	Zip Size	Space Type
mtr/global_suppressions	mtr/global_suppressions	33	4	Dynamic	0	Single
mtr/test_suppressions	mtr/test_suppressions	33	4	Dynamic	0	Single
test/fts_aux_index_1	test/fts_aux_index_1	33	8	Dynamic	0	Single
test/fts_aux_index_2	test/fts_aux_index_2	33	8	Dynamic	0	Single
test/fts_aux_index_3	test/fts_aux_index_3	33	8	Dynamic	0	Single
test/fts_aux_index_4	test/fts_aux_index_4	33	8	Dynamic	0	Single
test/fts_aux_index_5	test/fts_aux_index_5	33	8	Dynamic	0	Single
test/fts_aux_index_6	test/fts_aux_index_6	33	8	Dynamic	0	Single
test/fts_aux_being_deleted	test/fts_aux_being_deleted	33	4	Dynamic	0	Single
test/fts_aux_being_deleted_cache	test/fts_aux_being_deleted_cache	33	4	Dynamic	0	Single
test/fts_aux_config	test/fts_aux_config	33	5	Dynamic	0	Single
test/fts_aux_deleted	test/fts_aux_deleted	33	4	Dynamic	0	Single
test/fts_aux_deleted_cache	test/fts_aux_deleted_cache	33	4	Dynamic	0	Single
test/t1	test/t1	33	8	Dynamic	0	Single
SELECT pk FROM t1 WHERE MATCH (a,b)
AGAINST ('Tutorial' IN NATURAL LANGUAGE MODE);
pk
1
3
DROP TABLE t1;
DROP TABLESPACE s_def;
DROP TABLESPACE s_zip;
SET GLOBAL innodb_file_per_table = default;
