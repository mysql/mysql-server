#
# Non-Windows version of Tablespace Portability test.
#
# This testcase is related to WL#5980 & WL#6205.
# It tests whether datafiles can be copied from one location
# to another, and still be opened. In order to make this testcase run
# on any OS, we create the tablespaces here.
#
SET DEFAULT_STORAGE_ENGINE=InnoDB;
# restart:--innodb-directories=MYSQL_TMP_DIR
SHOW VARIABLES LIKE 'innodb_directories';
Variable_name	Value
innodb_directories	MYSQL_TMP_DIR
#
# Try to create a tablespace in a read-only directory.
#
CREATE TABLESPACE s7_def ADD DATAFILE 'MYSQL_TMP_DIR/read_only_dir/s7.ibd';
ERROR HY000: Failed to create TABLESPACE s7_def
SHOW WARNINGS;
Level	Code	Message
Error	1528	Failed to create TABLESPACE s7_def
Error	1030	Got error 168 - 'Unknown (generic) error from engine' from storage engine
#
# Try a series of invalid relative ADD DATAFILE entries.
# General tablespace files can be in the datadir but they cannot be under the datadir.
#
CREATE TABLESPACE bad ADD DATAFILE '';
ERROR HY000: The ADD DATAFILE filepath does not have a proper filename.
SHOW WARNINGS;
Level	Code	Message
Error	3121	The ADD DATAFILE filepath does not have a proper filename.
Error	3121	The ADD DATAFILE filepath must end with '.ibd'.
Error	1528	Failed to create TABLESPACE bad
Error	3121	Incorrect File Name ''.
CREATE TABLESPACE bad ADD DATAFILE '.ibd';
ERROR HY000: The ADD DATAFILE filepath does not have a proper filename.
SHOW WARNINGS;
Level	Code	Message
Error	3121	The ADD DATAFILE filepath does not have a proper filename.
Error	1528	Failed to create TABLESPACE bad
Error	3121	Incorrect File Name '.ibd'.
CREATE TABLESPACE bad ADD DATAFILE './ibd';
ERROR HY000: The ADD DATAFILE filepath does not have a proper filename.
SHOW WARNINGS;
Level	Code	Message
Error	3121	The ADD DATAFILE filepath does not have a proper filename.
Error	3121	The ADD DATAFILE filepath must end with '.ibd'.
Error	1528	Failed to create TABLESPACE bad
Error	3121	Incorrect File Name './ibd'.
CREATE TABLESPACE bad ADD DATAFILE './.ibd';
ERROR HY000: The ADD DATAFILE filepath does not have a proper filename.
SHOW WARNINGS;
Level	Code	Message
Error	3121	The ADD DATAFILE filepath does not have a proper filename.
Error	1528	Failed to create TABLESPACE bad
Error	3121	Incorrect File Name './.ibd'.
CREATE TABLESPACE bad ADD DATAFILE '../spaces/';
ERROR HY000: The ADD DATAFILE filepath does not have a proper filename.
SHOW WARNINGS;
Level	Code	Message
Error	3121	The ADD DATAFILE filepath does not have a proper filename.
Error	3121	The ADD DATAFILE filepath must end with '.ibd'.
Error	3121	The directory does not exist or is incorrect.
Error	3121	The DATAFILE location must be in a known directory.
Error	1528	Failed to create TABLESPACE bad
Error	3121	Incorrect File Name '../spaces/'.
CREATE TABLESPACE bad ADD DATAFILE 'test/bad.ibd';
ERROR HY000: The DATAFILE location cannot be under the datadir.
SHOW WARNINGS;
Level	Code	Message
Error	3121	The DATAFILE location cannot be under the datadir.
Error	1528	Failed to create TABLESPACE bad
Error	3121	Incorrect File Name 'test/bad.ibd'.
CREATE TABLESPACE bad ADD DATAFILE './test/bad.ibd';
ERROR HY000: The DATAFILE location cannot be under the datadir.
SHOW WARNINGS;
Level	Code	Message
Error	3121	The DATAFILE location cannot be under the datadir.
Error	1528	Failed to create TABLESPACE bad
Error	3121	Incorrect File Name './test/bad.ibd'.
CREATE TABLESPACE bad ADD DATAFILE './test//bad.ibd';
ERROR HY000: The DATAFILE location cannot be under the datadir.
SHOW WARNINGS;
Level	Code	Message
Error	3121	The DATAFILE location cannot be under the datadir.
Error	1528	Failed to create TABLESPACE bad
Error	3121	Incorrect File Name './test//bad.ibd'.
CREATE TABLESPACE bad ADD DATAFILE '../data/test/bad.ibd';
ERROR HY000: The DATAFILE location cannot be under the datadir.
SHOW WARNINGS;
Level	Code	Message
Error	3121	The DATAFILE location cannot be under the datadir.
Error	1528	Failed to create TABLESPACE bad
Error	3121	Incorrect File Name '../data/test/bad.ibd'.
CREATE TABLESPACE bad ADD DATAFILE '../data/../data/test/bad.ibd';
ERROR HY000: The ADD DATAFILE filepath cannot contain circular directory references.
SHOW WARNINGS;
Level	Code	Message
Error	3121	The ADD DATAFILE filepath cannot contain circular directory references.
Error	3121	The DATAFILE location cannot be under the datadir.
Error	1528	Failed to create TABLESPACE bad
Error	3121	Incorrect File Name '../data/../data/test/bad.ibd'.
CREATE TABLESPACE bad ADD DATAFILE '../data/../data/bad.ibd';
ERROR HY000: The ADD DATAFILE filepath cannot contain circular directory references.
SHOW WARNINGS;
Level	Code	Message
Error	3121	The ADD DATAFILE filepath cannot contain circular directory references.
Error	1528	Failed to create TABLESPACE bad
Error	3121	Incorrect File Name '../data/../data/bad.ibd'.
CREATE TABLESPACE bad ADD DATAFILE '../does_not_exist/bad.ibd';
ERROR HY000: The directory does not exist or is incorrect.
SHOW WARNINGS;
Level	Code	Message
Error	3121	The directory does not exist or is incorrect.
Error	3121	The DATAFILE location must be in a known directory.
Error	1528	Failed to create TABLESPACE bad
Error	3121	Incorrect File Name '../does_not_exist/bad.ibd'.
CREATE TABLESPACE bad ADD DATAFILE 'test/../bad.ibd';
ERROR HY000: The ADD DATAFILE filepath cannot contain circular directory references.
SHOW WARNINGS;
Level	Code	Message
Error	3121	The ADD DATAFILE filepath cannot contain circular directory references.
Error	1528	Failed to create TABLESPACE bad
Error	3121	Incorrect File Name 'test/../bad.ibd'.
CREATE TABLESPACE bad ADD DATAFILE './test/../bad.ibd';
ERROR HY000: The ADD DATAFILE filepath cannot contain circular directory references.
SHOW WARNINGS;
Level	Code	Message
Error	3121	The ADD DATAFILE filepath cannot contain circular directory references.
Error	1528	Failed to create TABLESPACE bad
Error	3121	Incorrect File Name './test/../bad.ibd'.
CREATE TABLESPACE bad ADD DATAFILE 'test\\..\\bad.ibd';
ERROR HY000: The ADD DATAFILE filepath cannot contain circular directory references.
SHOW WARNINGS;
Level	Code	Message
Error	3121	The ADD DATAFILE filepath cannot contain circular directory references.
Error	1528	Failed to create TABLESPACE bad
Error	3121	Incorrect File Name 'test\..\bad.ibd'.
CREATE TABLESPACE bad ADD DATAFILE '..///data//test///..//bad.ibd';
ERROR HY000: The ADD DATAFILE filepath cannot contain circular directory references.
SHOW WARNINGS;
Level	Code	Message
Error	3121	The ADD DATAFILE filepath cannot contain circular directory references.
Error	1528	Failed to create TABLESPACE bad
Error	3121	Incorrect File Name '..///data//test///..//bad.ibd'.
CREATE TABLESPACE bad ADD DATAFILE '.\\test\\..\\bad.ibd';
ERROR HY000: The ADD DATAFILE filepath cannot contain circular directory references.
SHOW WARNINGS;
Level	Code	Message
Error	3121	The ADD DATAFILE filepath cannot contain circular directory references.
Error	1528	Failed to create TABLESPACE bad
Error	3121	Incorrect File Name '.\test\..\bad.ibd'.
CREATE TABLESPACE bad ADD DATAFILE '..\\\\\\data\\\\test\\\\\\..\\\\bad.ibd';
ERROR HY000: The ADD DATAFILE filepath cannot contain circular directory references.
SHOW WARNINGS;
Level	Code	Message
Error	3121	The ADD DATAFILE filepath cannot contain circular directory references.
Error	1528	Failed to create TABLESPACE bad
Error	3121	Incorrect File Name '..\\\data\\test\\\..\\bad.ibd'.
#
# Try a series of valid relative ADD DATAFILE entries.
# General tablespace files can be in the datadir but they cannot be under the datadir.
#
CREATE TABLESPACE ibport_s01 ADD DATAFILE 'ibport_s01.ibd';
CREATE TABLESPACE ibport_s02 ADD DATAFILE './ibport_s02.ibd';
CREATE TABLESPACE ibport_s03 ADD DATAFILE '../data/ibport_s03.ibd';
CREATE TABLESPACE ibport_s04 ADD DATAFILE '.\\ibport_s04.ibd';
CREATE TABLESPACE ibport_s05 ADD DATAFILE '..\\data\\ibport_s05.ibd';
# Directory listing of MYSQLD_DATADIR/ ibport*
ibport_s01.ibd
ibport_s02.ibd
ibport_s03.ibd
ibport_s04.ibd
ibport_s05.ibd
SELECT s.space_type 'Type', s.name 'Space Name', d.path 'Path'
	FROM information_schema.innodb_tablespaces s, information_schema.innodb_datafiles d
WHERE s.name like '%ibport%' AND s.space = d.space ORDER BY s.space;
Type	Space Name	Path
General	ibport_s01	ibport_s01.ibd
General	ibport_s02	./ibport_s02.ibd
General	ibport_s03	../data/ibport_s03.ibd
General	ibport_s04	.\ibport_s04.ibd
General	ibport_s05	..\data\ibport_s05.ibd
SELECT s.space_type 'Type', s.name 'Space Name', f.file_name 'Path'
	FROM information_schema.innodb_tablespaces s, information_schema.files f
WHERE s.name like '%ibport%' AND s.space = f.file_id ORDER BY s.space;
Type	Space Name	Path
General	ibport_s01	./ibport_s01.ibd
General	ibport_s02	./ibport_s02.ibd
General	ibport_s03	../data/ibport_s03.ibd
General	ibport_s04	./ibport_s04.ibd
General	ibport_s05	../data/ibport_s05.ibd
DROP TABLESPACE ibport_s01;
DROP TABLESPACE ibport_s02;
DROP TABLESPACE ibport_s03;
DROP TABLESPACE ibport_s04;
DROP TABLESPACE ibport_s05;
# restart:
#
# Bug#32721533 CAN'T ALTER TABLE AFTER MOVING THE DATA FILE USING INNODB_DIRECTORIES OPTION
#
CREATE TABLE t1(a INT);
include/save_error_log_position.inc
# restart:
include/assert_error_log.inc [server: 1, pattern: .*\[Warning\].*Tablespace .*, name 'test/t1', file '.*test.*t1.ibd' is missing]
# restart: --innodb_directories=INNODB_DIR
ALTER TABLE t1 FORCE;
DROP TABLE t1;
# restart: 
