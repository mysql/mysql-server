#
# Show what happens when --innodb-directories is modified
# to ignore previously known directories.
#
SET default_storage_engine=InnoDB;
# Restart the engine to make the remote directory known.
# restart: --innodb-directories=MYSQL_TMP_DIR/remote_dir
#
# Create two tables in a known remote directory.
#
CREATE TABLE t1(c1 INT) DATA DIRECTORY='MYSQL_TMP_DIR/remote_dir';
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `c1` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci DATA DIRECTORY='MYSQL_TMP_DIR/remote_dir/'
CREATE TABLE t2(c1 INT, c2 INT) DATA DIRECTORY='MYSQL_TMP_DIR/remote_dir';
SHOW CREATE TABLE t2;
Table	Create Table
t2	CREATE TABLE `t2` (
  `c1` int DEFAULT NULL,
  `c2` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci DATA DIRECTORY='MYSQL_TMP_DIR/remote_dir/'
#
# Create a general tablespace in a known remote directory.
#
CREATE TABLESPACE ts1 ADD DATAFILE 'MYSQL_TMP_DIR/remote_dir/ts1.ibd';
#
# Create a table with a partitions in a known remote directory.
#
CREATE TABLE tp1(c1 INT)
PARTITION BY HASH(c1) (
PARTITION p1 DATA DIRECTORY='MYSQL_TMP_DIR/remote_dir',
PARTITION p2 DATA DIRECTORY='MYSQL_TMP_DIR/remote_dir');
SHOW CREATE TABLE tp1;
Table	Create Table
tp1	CREATE TABLE `tp1` (
  `c1` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
/*!50100 PARTITION BY HASH (`c1`)
(PARTITION p1 DATA DIRECTORY = 'MYSQL_TMP_DIR/remote_dir/' ENGINE = InnoDB,
 PARTITION p2 DATA DIRECTORY = 'MYSQL_TMP_DIR/remote_dir/' ENGINE = InnoDB) */
#
# Create a table with a sub-partitions in a known remote directory.
#
CREATE TABLE tsp1(c1 INT, c2 INT)
PARTITION BY RANGE(c1) SUBPARTITION BY HASH(c2) (
PARTITION p1 VALUES LESS THAN (10) (
SUBPARTITION s1 DATA DIRECTORY='MYSQL_TMP_DIR/remote_dir',
SUBPARTITION s2 DATA DIRECTORY='MYSQL_TMP_DIR/remote_dir'),
PARTITION p2 VALUES LESS THAN (20) (
SUBPARTITION s3 DATA DIRECTORY='MYSQL_TMP_DIR/remote_dir',
SUBPARTITION s4 DATA DIRECTORY='MYSQL_TMP_DIR/remote_dir'));
SHOW CREATE TABLE tsp1;
Table	Create Table
tsp1	CREATE TABLE `tsp1` (
  `c1` int DEFAULT NULL,
  `c2` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
/*!50100 PARTITION BY RANGE (`c1`)
SUBPARTITION BY HASH (`c2`)
(PARTITION p1 VALUES LESS THAN (10)
 (SUBPARTITION s1 DATA DIRECTORY = 'MYSQL_TMP_DIR/remote_dir/' ENGINE = InnoDB,
  SUBPARTITION s2 DATA DIRECTORY = 'MYSQL_TMP_DIR/remote_dir/' ENGINE = InnoDB),
 PARTITION p2 VALUES LESS THAN (20)
 (SUBPARTITION s3 DATA DIRECTORY = 'MYSQL_TMP_DIR/remote_dir/' ENGINE = InnoDB,
  SUBPARTITION s4 DATA DIRECTORY = 'MYSQL_TMP_DIR/remote_dir/' ENGINE = InnoDB)) */
#
# Create two undo tablespaces in a known remote directory.
#
CREATE UNDO TABLESPACE undo_003 ADD DATAFILE 'MYSQL_TMP_DIR/remote_dir/undo_003.ibu';
CREATE UNDO TABLESPACE undo_004 ADD DATAFILE 'MYSQL_TMP_DIR/remote_dir/undo_004.ibu';
SELECT NAME, SPACE_TYPE, STATE FROM INFORMATION_SCHEMA.INNODB_TABLESPACES
WHERE NAME LIKE 'undo_0%' ORDER BY NAME;
NAME	SPACE_TYPE	STATE
undo_003	Undo	active
undo_004	Undo	active
#
# Restart mysqld without the remote directory in --innodb-directories
# and with one of the undo tablespaces deleted.
#
# restart
#
# Validate the messages that should have been put into the error log at startup.
#
Pattern "The datafile '.*' for tablespace undo_003 is in an unprotected location. This file cannot be recovered after a crash until this location is added to innodb_directories" found
Pattern "Tablespace .*, name 'undo_004', file '.*undo_004.ibu' is missing" found
Pattern "Cannot find undo tablespace undo_004 with filename '.*' as indicated by the Data Dictionary. Did you move or delete this tablespace. Any undo logs in it cannot be used" found
Pattern "The datafile '.*' for tablespace test/t1 is in an unprotected location. This file cannot be recovered after a crash until this location is added to innodb_directories" found
Pattern "The datafile '.*' for tablespace test/t2 is in an unprotected location. This file cannot be recovered after a crash until this location is added to innodb_directories" found
Pattern "The datafile '.*' for tablespace ts1 is in an unprotected location. This file cannot be recovered after a crash until this location is added to innodb_directories" found
Pattern "The datafile '.*' for tablespace test/tp1#p#p1 is in an unprotected location. This file cannot be recovered after a crash until this location is added to innodb_directories" found
Pattern "The datafile '.*' for tablespace test/tp1#p#p2 is in an unprotected location. This file cannot be recovered after a crash until this location is added to innodb_directories" found
Pattern "The datafile '.*' for tablespace test/tsp1#p#p1#sp#s1 is in an unprotected location. This file cannot be recovered after a crash until this location is added to innodb_directories" found
Pattern "The datafile '.*' for tablespace test/tsp1#p#p1#sp#s2 is in an unprotected location. This file cannot be recovered after a crash until this location is added to innodb_directories" found
Pattern "The datafile '.*' for tablespace test/tsp1#p#p1#sp#s3 is in an unprotected location. This file cannot be recovered after a crash until this location is added to innodb_directories" not found
Pattern "The datafile '.*' for tablespace test/tsp1#p#p2#sp#s4 is in an unprotected location. This file cannot be recovered after a crash until this location is added to innodb_directories" found
#
# Show the tablespaces
#
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `c1` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci DATA DIRECTORY='MYSQL_TMP_DIR/remote_dir/'
SHOW CREATE TABLE t2;
Table	Create Table
t2	CREATE TABLE `t2` (
  `c1` int DEFAULT NULL,
  `c2` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci DATA DIRECTORY='MYSQL_TMP_DIR/remote_dir/'
SHOW CREATE TABLE tp1;
Table	Create Table
tp1	CREATE TABLE `tp1` (
  `c1` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
/*!50100 PARTITION BY HASH (`c1`)
(PARTITION p1 DATA DIRECTORY = 'MYSQL_TMP_DIR/remote_dir' ENGINE = InnoDB,
 PARTITION p2 DATA DIRECTORY = 'MYSQL_TMP_DIR/remote_dir' ENGINE = InnoDB) */
SHOW CREATE TABLE tsp1;
Table	Create Table
tsp1	CREATE TABLE `tsp1` (
  `c1` int DEFAULT NULL,
  `c2` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
/*!50100 PARTITION BY RANGE (`c1`)
SUBPARTITION BY HASH (`c2`)
(PARTITION p1 VALUES LESS THAN (10)
 (SUBPARTITION s1 DATA DIRECTORY = 'MYSQL_TMP_DIR/remote_dir' ENGINE = InnoDB,
  SUBPARTITION s2 DATA DIRECTORY = 'MYSQL_TMP_DIR/remote_dir' ENGINE = InnoDB),
 PARTITION p2 VALUES LESS THAN (20)
 (SUBPARTITION s3 DATA DIRECTORY = 'MYSQL_TMP_DIR/remote_dir' ENGINE = InnoDB,
  SUBPARTITION s4 DATA DIRECTORY = 'MYSQL_TMP_DIR/remote_dir' ENGINE = InnoDB)) */
=== information_schema.innodb_tablespaces and innodb_datafiles ===
Space_Name	Space_Type	Page_Size	Zip_Size	BlockSize!=0	FileSize!=0	Formats_Permitted	Path
mtr/asserted_test_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/asserted_test_suppressions.ibd
mtr/global_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/global_suppressions.ibd
mtr/test_ignored_global_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/test_ignored_global_suppressions.ibd
mtr/test_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/test_suppressions.ibd
test/t1	Single	DEFAULT	0	1	1	Dynamic	MYSQL_TMP_DIR/remote_dir/test/t1.ibd
test/t2	Single	DEFAULT	0	1	1	Dynamic	MYSQL_TMP_DIR/remote_dir/test/t2.ibd
test/tp1#p#p1	Single	DEFAULT	0	1	1	Dynamic	MYSQL_TMP_DIR/remote_dir/test/tp1#p#p1.ibd
test/tp1#p#p2	Single	DEFAULT	0	1	1	Dynamic	MYSQL_TMP_DIR/remote_dir/test/tp1#p#p2.ibd
test/tsp1#p#p1#sp#s1	Single	DEFAULT	0	1	1	Dynamic	MYSQL_TMP_DIR/remote_dir/test/tsp1#p#p1#sp#s1.ibd
test/tsp1#p#p1#sp#s2	Single	DEFAULT	0	1	1	Dynamic	MYSQL_TMP_DIR/remote_dir/test/tsp1#p#p1#sp#s2.ibd
test/tsp1#p#p2#sp#s3	Single	DEFAULT	0	1	1	Dynamic	MYSQL_TMP_DIR/remote_dir/test/tsp1#p#p2#sp#s3.ibd
test/tsp1#p#p2#sp#s4	Single	DEFAULT	0	1	1	Dynamic	MYSQL_TMP_DIR/remote_dir/test/tsp1#p#p2#sp#s4.ibd
ts1	General	DEFAULT	0	1	1	Any	MYSQL_TMP_DIR/remote_dir/ts1.ibd
=== information_schema.files ===
Space_Name	File_Type	Engine	Status	Tablespace_Name	Path
mtr/asserted_test_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/asserted_test_suppressions	MYSQLD_DATADIR/mtr/asserted_test_suppressions.ibd
mtr/global_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/global_suppressions	MYSQLD_DATADIR/mtr/global_suppressions.ibd
mtr/test_ignored_global_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/test_ignored_global_suppressions	MYSQLD_DATADIR/mtr/test_ignored_global_suppressions.ibd
mtr/test_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/test_suppressions	MYSQLD_DATADIR/mtr/test_suppressions.ibd
test/t1	TABLESPACE	InnoDB	NORMAL	test/t1	MYSQL_TMP_DIR/remote_dir/test/t1.ibd
test/t2	TABLESPACE	InnoDB	NORMAL	test/t2	MYSQL_TMP_DIR/remote_dir/test/t2.ibd
test/tp1#p#p1	TABLESPACE	InnoDB	NORMAL	test/tp1#p#p1	MYSQL_TMP_DIR/remote_dir/test/tp1#p#p1.ibd
test/tp1#p#p2	TABLESPACE	InnoDB	NORMAL	test/tp1#p#p2	MYSQL_TMP_DIR/remote_dir/test/tp1#p#p2.ibd
test/tsp1#p#p1#sp#s1	TABLESPACE	InnoDB	NORMAL	test/tsp1#p#p1#sp#s1	MYSQL_TMP_DIR/remote_dir/test/tsp1#p#p1#sp#s1.ibd
test/tsp1#p#p1#sp#s2	TABLESPACE	InnoDB	NORMAL	test/tsp1#p#p1#sp#s2	MYSQL_TMP_DIR/remote_dir/test/tsp1#p#p1#sp#s2.ibd
test/tsp1#p#p2#sp#s3	TABLESPACE	InnoDB	NORMAL	test/tsp1#p#p2#sp#s3	MYSQL_TMP_DIR/remote_dir/test/tsp1#p#p2#sp#s3.ibd
test/tsp1#p#p2#sp#s4	TABLESPACE	InnoDB	NORMAL	test/tsp1#p#p2#sp#s4	MYSQL_TMP_DIR/remote_dir/test/tsp1#p#p2#sp#s4.ibd
ts1	TABLESPACE	InnoDB	NORMAL	ts1	MYSQL_TMP_DIR/remote_dir/ts1.ibd
#
# If implicit tablespaces that exist in unknown locations are
# part of a TRUNCATE statement, they will be recreated in
# the default datadir location with a warning put into the error log.
#
TRUNCATE TABLE t1;
Warnings:
Warning	3121	The DATA DIRECTORY location must be in a known directory. The DATA DIRECTORY location will be ignored and the file will be put into the default datadir location.
Warning	1618	<DATA DIRECTORY> option ignored
SHOW WARNINGS;
Level	Code	Message
Warning	3121	The DATA DIRECTORY location must be in a known directory. The DATA DIRECTORY location will be ignored and the file will be put into the default datadir location.
Warning	1618	<DATA DIRECTORY> option ignored
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `c1` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
TRUNCATE TABLE t2;
Warnings:
Warning	3121	The DATA DIRECTORY location must be in a known directory. The DATA DIRECTORY location will be ignored and the file will be put into the default datadir location.
Warning	1618	<DATA DIRECTORY> option ignored
SHOW WARNINGS;
Level	Code	Message
Warning	3121	The DATA DIRECTORY location must be in a known directory. The DATA DIRECTORY location will be ignored and the file will be put into the default datadir location.
Warning	1618	<DATA DIRECTORY> option ignored
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `c1` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
=== information_schema.innodb_tablespaces and innodb_datafiles ===
Space_Name	Space_Type	Page_Size	Zip_Size	BlockSize!=0	FileSize!=0	Formats_Permitted	Path
mtr/asserted_test_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/asserted_test_suppressions.ibd
mtr/global_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/global_suppressions.ibd
mtr/test_ignored_global_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/test_ignored_global_suppressions.ibd
mtr/test_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/test_suppressions.ibd
test/t1	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/test/t1.ibd
test/t2	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/test/t2.ibd
test/tp1#p#p1	Single	DEFAULT	0	1	1	Dynamic	MYSQL_TMP_DIR/remote_dir/test/tp1#p#p1.ibd
test/tp1#p#p2	Single	DEFAULT	0	1	1	Dynamic	MYSQL_TMP_DIR/remote_dir/test/tp1#p#p2.ibd
test/tsp1#p#p1#sp#s1	Single	DEFAULT	0	1	1	Dynamic	MYSQL_TMP_DIR/remote_dir/test/tsp1#p#p1#sp#s1.ibd
test/tsp1#p#p1#sp#s2	Single	DEFAULT	0	1	1	Dynamic	MYSQL_TMP_DIR/remote_dir/test/tsp1#p#p1#sp#s2.ibd
test/tsp1#p#p2#sp#s3	Single	DEFAULT	0	1	1	Dynamic	MYSQL_TMP_DIR/remote_dir/test/tsp1#p#p2#sp#s3.ibd
test/tsp1#p#p2#sp#s4	Single	DEFAULT	0	1	1	Dynamic	MYSQL_TMP_DIR/remote_dir/test/tsp1#p#p2#sp#s4.ibd
ts1	General	DEFAULT	0	1	1	Any	MYSQL_TMP_DIR/remote_dir/ts1.ibd
=== information_schema.files ===
Space_Name	File_Type	Engine	Status	Tablespace_Name	Path
mtr/asserted_test_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/asserted_test_suppressions	MYSQLD_DATADIR/mtr/asserted_test_suppressions.ibd
mtr/global_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/global_suppressions	MYSQLD_DATADIR/mtr/global_suppressions.ibd
mtr/test_ignored_global_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/test_ignored_global_suppressions	MYSQLD_DATADIR/mtr/test_ignored_global_suppressions.ibd
mtr/test_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/test_suppressions	MYSQLD_DATADIR/mtr/test_suppressions.ibd
test/t1	TABLESPACE	InnoDB	NORMAL	test/t1	MYSQLD_DATADIR/test/t1.ibd
test/t2	TABLESPACE	InnoDB	NORMAL	test/t2	MYSQLD_DATADIR/test/t2.ibd
test/tp1#p#p1	TABLESPACE	InnoDB	NORMAL	test/tp1#p#p1	MYSQL_TMP_DIR/remote_dir/test/tp1#p#p1.ibd
test/tp1#p#p2	TABLESPACE	InnoDB	NORMAL	test/tp1#p#p2	MYSQL_TMP_DIR/remote_dir/test/tp1#p#p2.ibd
test/tsp1#p#p1#sp#s1	TABLESPACE	InnoDB	NORMAL	test/tsp1#p#p1#sp#s1	MYSQL_TMP_DIR/remote_dir/test/tsp1#p#p1#sp#s1.ibd
test/tsp1#p#p1#sp#s2	TABLESPACE	InnoDB	NORMAL	test/tsp1#p#p1#sp#s2	MYSQL_TMP_DIR/remote_dir/test/tsp1#p#p1#sp#s2.ibd
test/tsp1#p#p2#sp#s3	TABLESPACE	InnoDB	NORMAL	test/tsp1#p#p2#sp#s3	MYSQL_TMP_DIR/remote_dir/test/tsp1#p#p2#sp#s3.ibd
test/tsp1#p#p2#sp#s4	TABLESPACE	InnoDB	NORMAL	test/tsp1#p#p2#sp#s4	MYSQL_TMP_DIR/remote_dir/test/tsp1#p#p2#sp#s4.ibd
ts1	TABLESPACE	InnoDB	NORMAL	ts1	MYSQL_TMP_DIR/remote_dir/ts1.ibd
#
# EXCHANGE PARTITION does not recreate tablespaces, so files in unknown
# locations will remain there through the exchange.
#
ALTER TABLE tp1 EXCHANGE PARTITION p1 WITH TABLE t1;
SHOW WARNINGS;
Level	Code	Message
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `c1` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci DATA DIRECTORY='MYSQL_TMP_DIR/remote_dir/'
SHOW CREATE TABLE tp1;
Table	Create Table
tp1	CREATE TABLE `tp1` (
  `c1` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
/*!50100 PARTITION BY HASH (`c1`)
(PARTITION p1 ENGINE = InnoDB,
 PARTITION p2 DATA DIRECTORY = 'MYSQL_TMP_DIR/remote_dir' ENGINE = InnoDB) */
ALTER TABLE tsp1 EXCHANGE PARTITION s2 WITH TABLE t2;
SHOW WARNINGS;
Level	Code	Message
SHOW CREATE TABLE t2;
Table	Create Table
t2	CREATE TABLE `t2` (
  `c1` int DEFAULT NULL,
  `c2` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci DATA DIRECTORY='MYSQL_TMP_DIR/remote_dir/'
SHOW CREATE TABLE tsp1;
Table	Create Table
tsp1	CREATE TABLE `tsp1` (
  `c1` int DEFAULT NULL,
  `c2` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
/*!50100 PARTITION BY RANGE (`c1`)
SUBPARTITION BY HASH (`c2`)
(PARTITION p1 VALUES LESS THAN (10)
 (SUBPARTITION s1 DATA DIRECTORY = 'MYSQL_TMP_DIR/remote_dir' ENGINE = InnoDB,
  SUBPARTITION s2 ENGINE = InnoDB),
 PARTITION p2 VALUES LESS THAN (20)
 (SUBPARTITION s3 DATA DIRECTORY = 'MYSQL_TMP_DIR/remote_dir' ENGINE = InnoDB,
  SUBPARTITION s4 DATA DIRECTORY = 'MYSQL_TMP_DIR/remote_dir' ENGINE = InnoDB)) */
=== information_schema.innodb_tablespaces and innodb_datafiles ===
Space_Name	Space_Type	Page_Size	Zip_Size	BlockSize!=0	FileSize!=0	Formats_Permitted	Path
mtr/asserted_test_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/asserted_test_suppressions.ibd
mtr/global_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/global_suppressions.ibd
mtr/test_ignored_global_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/test_ignored_global_suppressions.ibd
mtr/test_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/test_suppressions.ibd
test/t1	Single	DEFAULT	0	1	1	Dynamic	MYSQL_TMP_DIR/remote_dir/test/t1.ibd
test/t2	Single	DEFAULT	0	1	1	Dynamic	MYSQL_TMP_DIR/remote_dir/test/t2.ibd
test/tp1#p#p1	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/test/tp1#p#p1.ibd
test/tp1#p#p2	Single	DEFAULT	0	1	1	Dynamic	MYSQL_TMP_DIR/remote_dir/test/tp1#p#p2.ibd
test/tsp1#p#p1#sp#s1	Single	DEFAULT	0	1	1	Dynamic	MYSQL_TMP_DIR/remote_dir/test/tsp1#p#p1#sp#s1.ibd
test/tsp1#p#p1#sp#s2	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/test/tsp1#p#p1#sp#s2.ibd
test/tsp1#p#p2#sp#s3	Single	DEFAULT	0	1	1	Dynamic	MYSQL_TMP_DIR/remote_dir/test/tsp1#p#p2#sp#s3.ibd
test/tsp1#p#p2#sp#s4	Single	DEFAULT	0	1	1	Dynamic	MYSQL_TMP_DIR/remote_dir/test/tsp1#p#p2#sp#s4.ibd
ts1	General	DEFAULT	0	1	1	Any	MYSQL_TMP_DIR/remote_dir/ts1.ibd
=== information_schema.files ===
Space_Name	File_Type	Engine	Status	Tablespace_Name	Path
mtr/asserted_test_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/asserted_test_suppressions	MYSQLD_DATADIR/mtr/asserted_test_suppressions.ibd
mtr/global_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/global_suppressions	MYSQLD_DATADIR/mtr/global_suppressions.ibd
mtr/test_ignored_global_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/test_ignored_global_suppressions	MYSQLD_DATADIR/mtr/test_ignored_global_suppressions.ibd
mtr/test_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/test_suppressions	MYSQLD_DATADIR/mtr/test_suppressions.ibd
test/t1	TABLESPACE	InnoDB	NORMAL	test/t1	MYSQL_TMP_DIR/remote_dir/test/t1.ibd
test/t2	TABLESPACE	InnoDB	NORMAL	test/t2	MYSQL_TMP_DIR/remote_dir/test/t2.ibd
test/tp1#p#p1	TABLESPACE	InnoDB	NORMAL	test/tp1#p#p1	MYSQLD_DATADIR/test/tp1#p#p1.ibd
test/tp1#p#p2	TABLESPACE	InnoDB	NORMAL	test/tp1#p#p2	MYSQL_TMP_DIR/remote_dir/test/tp1#p#p2.ibd
test/tsp1#p#p1#sp#s1	TABLESPACE	InnoDB	NORMAL	test/tsp1#p#p1#sp#s1	MYSQL_TMP_DIR/remote_dir/test/tsp1#p#p1#sp#s1.ibd
test/tsp1#p#p1#sp#s2	TABLESPACE	InnoDB	NORMAL	test/tsp1#p#p1#sp#s2	MYSQLD_DATADIR/test/tsp1#p#p1#sp#s2.ibd
test/tsp1#p#p2#sp#s3	TABLESPACE	InnoDB	NORMAL	test/tsp1#p#p2#sp#s3	MYSQL_TMP_DIR/remote_dir/test/tsp1#p#p2#sp#s3.ibd
test/tsp1#p#p2#sp#s4	TABLESPACE	InnoDB	NORMAL	test/tsp1#p#p2#sp#s4	MYSQL_TMP_DIR/remote_dir/test/tsp1#p#p2#sp#s4.ibd
ts1	TABLESPACE	InnoDB	NORMAL	ts1	MYSQL_TMP_DIR/remote_dir/ts1.ibd
TRUNCATE TABLE t2;
Warnings:
Warning	3121	The DATA DIRECTORY location must be in a known directory. The DATA DIRECTORY location will be ignored and the file will be put into the default datadir location.
Warning	1618	<DATA DIRECTORY> option ignored
SHOW WARNINGS;
Level	Code	Message
Warning	3121	The DATA DIRECTORY location must be in a known directory. The DATA DIRECTORY location will be ignored and the file will be put into the default datadir location.
Warning	1618	<DATA DIRECTORY> option ignored
SHOW CREATE TABLE t2;
Table	Create Table
t2	CREATE TABLE `t2` (
  `c1` int DEFAULT NULL,
  `c2` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
=== information_schema.innodb_tablespaces and innodb_datafiles ===
Space_Name	Space_Type	Page_Size	Zip_Size	BlockSize!=0	FileSize!=0	Formats_Permitted	Path
mtr/asserted_test_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/asserted_test_suppressions.ibd
mtr/global_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/global_suppressions.ibd
mtr/test_ignored_global_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/test_ignored_global_suppressions.ibd
mtr/test_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/test_suppressions.ibd
test/t1	Single	DEFAULT	0	1	1	Dynamic	MYSQL_TMP_DIR/remote_dir/test/t1.ibd
test/t2	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/test/t2.ibd
test/tp1#p#p1	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/test/tp1#p#p1.ibd
test/tp1#p#p2	Single	DEFAULT	0	1	1	Dynamic	MYSQL_TMP_DIR/remote_dir/test/tp1#p#p2.ibd
test/tsp1#p#p1#sp#s1	Single	DEFAULT	0	1	1	Dynamic	MYSQL_TMP_DIR/remote_dir/test/tsp1#p#p1#sp#s1.ibd
test/tsp1#p#p1#sp#s2	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/test/tsp1#p#p1#sp#s2.ibd
test/tsp1#p#p2#sp#s3	Single	DEFAULT	0	1	1	Dynamic	MYSQL_TMP_DIR/remote_dir/test/tsp1#p#p2#sp#s3.ibd
test/tsp1#p#p2#sp#s4	Single	DEFAULT	0	1	1	Dynamic	MYSQL_TMP_DIR/remote_dir/test/tsp1#p#p2#sp#s4.ibd
ts1	General	DEFAULT	0	1	1	Any	MYSQL_TMP_DIR/remote_dir/ts1.ibd
=== information_schema.files ===
Space_Name	File_Type	Engine	Status	Tablespace_Name	Path
mtr/asserted_test_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/asserted_test_suppressions	MYSQLD_DATADIR/mtr/asserted_test_suppressions.ibd
mtr/global_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/global_suppressions	MYSQLD_DATADIR/mtr/global_suppressions.ibd
mtr/test_ignored_global_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/test_ignored_global_suppressions	MYSQLD_DATADIR/mtr/test_ignored_global_suppressions.ibd
mtr/test_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/test_suppressions	MYSQLD_DATADIR/mtr/test_suppressions.ibd
test/t1	TABLESPACE	InnoDB	NORMAL	test/t1	MYSQL_TMP_DIR/remote_dir/test/t1.ibd
test/t2	TABLESPACE	InnoDB	NORMAL	test/t2	MYSQLD_DATADIR/test/t2.ibd
test/tp1#p#p1	TABLESPACE	InnoDB	NORMAL	test/tp1#p#p1	MYSQLD_DATADIR/test/tp1#p#p1.ibd
test/tp1#p#p2	TABLESPACE	InnoDB	NORMAL	test/tp1#p#p2	MYSQL_TMP_DIR/remote_dir/test/tp1#p#p2.ibd
test/tsp1#p#p1#sp#s1	TABLESPACE	InnoDB	NORMAL	test/tsp1#p#p1#sp#s1	MYSQL_TMP_DIR/remote_dir/test/tsp1#p#p1#sp#s1.ibd
test/tsp1#p#p1#sp#s2	TABLESPACE	InnoDB	NORMAL	test/tsp1#p#p1#sp#s2	MYSQLD_DATADIR/test/tsp1#p#p1#sp#s2.ibd
test/tsp1#p#p2#sp#s3	TABLESPACE	InnoDB	NORMAL	test/tsp1#p#p2#sp#s3	MYSQL_TMP_DIR/remote_dir/test/tsp1#p#p2#sp#s3.ibd
test/tsp1#p#p2#sp#s4	TABLESPACE	InnoDB	NORMAL	test/tsp1#p#p2#sp#s4	MYSQL_TMP_DIR/remote_dir/test/tsp1#p#p2#sp#s4.ibd
ts1	TABLESPACE	InnoDB	NORMAL	ts1	MYSQL_TMP_DIR/remote_dir/ts1.ibd
ALTER TABLE tsp1 EXCHANGE PARTITION s4 WITH TABLE t2;
SHOW WARNINGS;
Level	Code	Message
SHOW CREATE TABLE t2;
Table	Create Table
t2	CREATE TABLE `t2` (
  `c1` int DEFAULT NULL,
  `c2` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci DATA DIRECTORY='MYSQL_TMP_DIR/remote_dir/'
SHOW CREATE TABLE tsp1;
Table	Create Table
tsp1	CREATE TABLE `tsp1` (
  `c1` int DEFAULT NULL,
  `c2` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
/*!50100 PARTITION BY RANGE (`c1`)
SUBPARTITION BY HASH (`c2`)
(PARTITION p1 VALUES LESS THAN (10)
 (SUBPARTITION s1 DATA DIRECTORY = 'MYSQL_TMP_DIR/remote_dir' ENGINE = InnoDB,
  SUBPARTITION s2 ENGINE = InnoDB),
 PARTITION p2 VALUES LESS THAN (20)
 (SUBPARTITION s3 DATA DIRECTORY = 'MYSQL_TMP_DIR/remote_dir' ENGINE = InnoDB,
  SUBPARTITION s4 ENGINE = InnoDB)) */
=== information_schema.innodb_tablespaces and innodb_datafiles ===
Space_Name	Space_Type	Page_Size	Zip_Size	BlockSize!=0	FileSize!=0	Formats_Permitted	Path
mtr/asserted_test_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/asserted_test_suppressions.ibd
mtr/global_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/global_suppressions.ibd
mtr/test_ignored_global_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/test_ignored_global_suppressions.ibd
mtr/test_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/test_suppressions.ibd
test/t1	Single	DEFAULT	0	1	1	Dynamic	MYSQL_TMP_DIR/remote_dir/test/t1.ibd
test/t2	Single	DEFAULT	0	1	1	Dynamic	MYSQL_TMP_DIR/remote_dir/test/t2.ibd
test/tp1#p#p1	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/test/tp1#p#p1.ibd
test/tp1#p#p2	Single	DEFAULT	0	1	1	Dynamic	MYSQL_TMP_DIR/remote_dir/test/tp1#p#p2.ibd
test/tsp1#p#p1#sp#s1	Single	DEFAULT	0	1	1	Dynamic	MYSQL_TMP_DIR/remote_dir/test/tsp1#p#p1#sp#s1.ibd
test/tsp1#p#p1#sp#s2	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/test/tsp1#p#p1#sp#s2.ibd
test/tsp1#p#p2#sp#s3	Single	DEFAULT	0	1	1	Dynamic	MYSQL_TMP_DIR/remote_dir/test/tsp1#p#p2#sp#s3.ibd
test/tsp1#p#p2#sp#s4	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/test/tsp1#p#p2#sp#s4.ibd
ts1	General	DEFAULT	0	1	1	Any	MYSQL_TMP_DIR/remote_dir/ts1.ibd
=== information_schema.files ===
Space_Name	File_Type	Engine	Status	Tablespace_Name	Path
mtr/asserted_test_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/asserted_test_suppressions	MYSQLD_DATADIR/mtr/asserted_test_suppressions.ibd
mtr/global_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/global_suppressions	MYSQLD_DATADIR/mtr/global_suppressions.ibd
mtr/test_ignored_global_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/test_ignored_global_suppressions	MYSQLD_DATADIR/mtr/test_ignored_global_suppressions.ibd
mtr/test_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/test_suppressions	MYSQLD_DATADIR/mtr/test_suppressions.ibd
test/t1	TABLESPACE	InnoDB	NORMAL	test/t1	MYSQL_TMP_DIR/remote_dir/test/t1.ibd
test/t2	TABLESPACE	InnoDB	NORMAL	test/t2	MYSQL_TMP_DIR/remote_dir/test/t2.ibd
test/tp1#p#p1	TABLESPACE	InnoDB	NORMAL	test/tp1#p#p1	MYSQLD_DATADIR/test/tp1#p#p1.ibd
test/tp1#p#p2	TABLESPACE	InnoDB	NORMAL	test/tp1#p#p2	MYSQL_TMP_DIR/remote_dir/test/tp1#p#p2.ibd
test/tsp1#p#p1#sp#s1	TABLESPACE	InnoDB	NORMAL	test/tsp1#p#p1#sp#s1	MYSQL_TMP_DIR/remote_dir/test/tsp1#p#p1#sp#s1.ibd
test/tsp1#p#p1#sp#s2	TABLESPACE	InnoDB	NORMAL	test/tsp1#p#p1#sp#s2	MYSQLD_DATADIR/test/tsp1#p#p1#sp#s2.ibd
test/tsp1#p#p2#sp#s3	TABLESPACE	InnoDB	NORMAL	test/tsp1#p#p2#sp#s3	MYSQL_TMP_DIR/remote_dir/test/tsp1#p#p2#sp#s3.ibd
test/tsp1#p#p2#sp#s4	TABLESPACE	InnoDB	NORMAL	test/tsp1#p#p2#sp#s4	MYSQLD_DATADIR/test/tsp1#p#p2#sp#s4.ibd
ts1	TABLESPACE	InnoDB	NORMAL	ts1	MYSQL_TMP_DIR/remote_dir/ts1.ibd
#
# RENAME TABLE does not recreate tablespaces, so files in unknown
# locations will remain there. No error message is issued.
#
RENAME TABLE t1 to temp1;
SHOW CREATE TABLE temp1;
Table	Create Table
temp1	CREATE TABLE `temp1` (
  `c1` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci DATA DIRECTORY='MYSQL_TMP_DIR/remote_dir/'
RENAME TABLE temp1 to t1;
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `c1` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci DATA DIRECTORY='MYSQL_TMP_DIR/remote_dir/'
#
# TRUNCATE the three tables to recreate all datafiles in the
# default datafile location.
#
TRUNCATE TABLE t1;
Warnings:
Warning	3121	The DATA DIRECTORY location must be in a known directory. The DATA DIRECTORY location will be ignored and the file will be put into the default datadir location.
Warning	1618	<DATA DIRECTORY> option ignored
SHOW WARNINGS;
Level	Code	Message
Warning	3121	The DATA DIRECTORY location must be in a known directory. The DATA DIRECTORY location will be ignored and the file will be put into the default datadir location.
Warning	1618	<DATA DIRECTORY> option ignored
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `c1` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
TRUNCATE TABLE t2;
Warnings:
Warning	3121	The DATA DIRECTORY location must be in a known directory. The DATA DIRECTORY location will be ignored and the file will be put into the default datadir location.
Warning	1618	<DATA DIRECTORY> option ignored
SHOW WARNINGS;
Level	Code	Message
Warning	3121	The DATA DIRECTORY location must be in a known directory. The DATA DIRECTORY location will be ignored and the file will be put into the default datadir location.
Warning	1618	<DATA DIRECTORY> option ignored
SHOW CREATE TABLE t2;
Table	Create Table
t2	CREATE TABLE `t2` (
  `c1` int DEFAULT NULL,
  `c2` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
TRUNCATE TABLE tp1;
Warnings:
Warning	3121	The DATA DIRECTORY location must be in a known directory. The DATA DIRECTORY location will be ignored and the file will be put into the default datadir location.
Warning	1618	<DATA DIRECTORY> option ignored
SHOW WARNINGS;
Level	Code	Message
Warning	3121	The DATA DIRECTORY location must be in a known directory. The DATA DIRECTORY location will be ignored and the file will be put into the default datadir location.
Warning	1618	<DATA DIRECTORY> option ignored
SHOW CREATE TABLE tp1;
Table	Create Table
tp1	CREATE TABLE `tp1` (
  `c1` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
/*!50100 PARTITION BY HASH (`c1`)
(PARTITION p1 ENGINE = InnoDB,
 PARTITION p2 ENGINE = InnoDB) */
TRUNCATE TABLE tsp1;
Warnings:
Warning	3121	The DATA DIRECTORY location must be in a known directory. The DATA DIRECTORY location will be ignored and the file will be put into the default datadir location.
Warning	1618	<DATA DIRECTORY> option ignored
Warning	3121	The DATA DIRECTORY location must be in a known directory. The DATA DIRECTORY location will be ignored and the file will be put into the default datadir location.
Warning	1618	<DATA DIRECTORY> option ignored
SHOW WARNINGS;
Level	Code	Message
Warning	3121	The DATA DIRECTORY location must be in a known directory. The DATA DIRECTORY location will be ignored and the file will be put into the default datadir location.
Warning	1618	<DATA DIRECTORY> option ignored
Warning	3121	The DATA DIRECTORY location must be in a known directory. The DATA DIRECTORY location will be ignored and the file will be put into the default datadir location.
Warning	1618	<DATA DIRECTORY> option ignored
SHOW CREATE TABLE tsp1;
Table	Create Table
tsp1	CREATE TABLE `tsp1` (
  `c1` int DEFAULT NULL,
  `c2` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
/*!50100 PARTITION BY RANGE (`c1`)
SUBPARTITION BY HASH (`c2`)
(PARTITION p1 VALUES LESS THAN (10)
 (SUBPARTITION s1 ENGINE = InnoDB,
  SUBPARTITION s2 ENGINE = InnoDB),
 PARTITION p2 VALUES LESS THAN (20)
 (SUBPARTITION s3 ENGINE = InnoDB,
  SUBPARTITION s4 ENGINE = InnoDB)) */
=== information_schema.innodb_tablespaces and innodb_datafiles ===
Space_Name	Space_Type	Page_Size	Zip_Size	BlockSize!=0	FileSize!=0	Formats_Permitted	Path
mtr/asserted_test_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/asserted_test_suppressions.ibd
mtr/global_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/global_suppressions.ibd
mtr/test_ignored_global_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/test_ignored_global_suppressions.ibd
mtr/test_suppressions	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/mtr/test_suppressions.ibd
test/t1	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/test/t1.ibd
test/t2	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/test/t2.ibd
test/tp1#p#p1	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/test/tp1#p#p1.ibd
test/tp1#p#p2	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/test/tp1#p#p2.ibd
test/tsp1#p#p1#sp#s1	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/test/tsp1#p#p1#sp#s1.ibd
test/tsp1#p#p1#sp#s2	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/test/tsp1#p#p1#sp#s2.ibd
test/tsp1#p#p2#sp#s3	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/test/tsp1#p#p2#sp#s3.ibd
test/tsp1#p#p2#sp#s4	Single	DEFAULT	0	1	1	Dynamic	MYSQLD_DATADIR/test/tsp1#p#p2#sp#s4.ibd
ts1	General	DEFAULT	0	1	1	Any	MYSQL_TMP_DIR/remote_dir/ts1.ibd
=== information_schema.files ===
Space_Name	File_Type	Engine	Status	Tablespace_Name	Path
mtr/asserted_test_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/asserted_test_suppressions	MYSQLD_DATADIR/mtr/asserted_test_suppressions.ibd
mtr/global_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/global_suppressions	MYSQLD_DATADIR/mtr/global_suppressions.ibd
mtr/test_ignored_global_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/test_ignored_global_suppressions	MYSQLD_DATADIR/mtr/test_ignored_global_suppressions.ibd
mtr/test_suppressions	TABLESPACE	InnoDB	NORMAL	mtr/test_suppressions	MYSQLD_DATADIR/mtr/test_suppressions.ibd
test/t1	TABLESPACE	InnoDB	NORMAL	test/t1	MYSQLD_DATADIR/test/t1.ibd
test/t2	TABLESPACE	InnoDB	NORMAL	test/t2	MYSQLD_DATADIR/test/t2.ibd
test/tp1#p#p1	TABLESPACE	InnoDB	NORMAL	test/tp1#p#p1	MYSQLD_DATADIR/test/tp1#p#p1.ibd
test/tp1#p#p2	TABLESPACE	InnoDB	NORMAL	test/tp1#p#p2	MYSQLD_DATADIR/test/tp1#p#p2.ibd
test/tsp1#p#p1#sp#s1	TABLESPACE	InnoDB	NORMAL	test/tsp1#p#p1#sp#s1	MYSQLD_DATADIR/test/tsp1#p#p1#sp#s1.ibd
test/tsp1#p#p1#sp#s2	TABLESPACE	InnoDB	NORMAL	test/tsp1#p#p1#sp#s2	MYSQLD_DATADIR/test/tsp1#p#p1#sp#s2.ibd
test/tsp1#p#p2#sp#s3	TABLESPACE	InnoDB	NORMAL	test/tsp1#p#p2#sp#s3	MYSQLD_DATADIR/test/tsp1#p#p2#sp#s3.ibd
test/tsp1#p#p2#sp#s4	TABLESPACE	InnoDB	NORMAL	test/tsp1#p#p2#sp#s4	MYSQLD_DATADIR/test/tsp1#p#p2#sp#s4.ibd
ts1	TABLESPACE	InnoDB	NORMAL	ts1	MYSQL_TMP_DIR/remote_dir/ts1.ibd
#
# Validate the messages that should have been put into the error log.
#
Pattern "Cannot create a tablespace for table test/t1 because the directory is not a valid location. The DATA DIRECTORY location must be in a known directory. The DATA DIRECTORY location will be ignored and the file will be put into the default datadir location" found
Pattern "Cannot create a tablespace for table test/t2 because the directory is not a valid location. The DATA DIRECTORY location must be in a known directory. The DATA DIRECTORY location will be ignored and the file will be put into the default datadir location" found
Pattern "Cannot create a tablespace for table test/tp1#p#p2 because the directory is not a valid location. The DATA DIRECTORY location must be in a known directory. The DATA DIRECTORY location will be ignored and the file will be put into the default datadir location" found
Pattern "Cannot create a tablespace for table test/tsp1#p#p1#sp#s1 because the directory is not a valid location. The DATA DIRECTORY location must be in a known directory. The DATA DIRECTORY location will be ignored and the file will be put into the default datadir location" found
Pattern "Cannot create a tablespace for table test/tsp1#p#p2#sp#s3 because the directory is not a valid location. The DATA DIRECTORY location must be in a known directory. The DATA DIRECTORY location will be ignored and the file will be put into the default datadir location" found
#
# Cleanup the IBD tablespaces
#
DROP TABLE t1;
DROP TABLE t2;
DROP TABLE tp1;
DROP TABLE tsp1;
DROP TABLESPACE ts1;
#
# Cleanup the Undo IBU tablespaces
#
SELECT NAME, SPACE_TYPE, STATE FROM INFORMATION_SCHEMA.INNODB_TABLESPACES
WHERE NAME LIKE 'undo_0%' ORDER BY NAME;
NAME	SPACE_TYPE	STATE
undo_003	Undo	active
undo_004	Undo	active
ALTER UNDO TABLESPACE undo_003 SET INACTIVE;
DROP UNDO TABLESPACE undo_003;
DROP UNDO TABLESPACE undo_004;
