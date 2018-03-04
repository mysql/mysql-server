# Remember some variables.
LET $MYSQLD_DATADIR = `select @@datadir`;
LET $INNODB_PAGE_SIZE = `select @@innodb_page_size`;

--echo #
--echo # Try a series of invalid relative ADD DATAFILE entries.
--echo # General tablespace files can be in the datadir but they cannot be under the datadir.
--echo #
--error ER_WRONG_FILE_NAME
CREATE TABLESPACE bad ADD DATAFILE '';
SHOW WARNINGS;
--error ER_WRONG_FILE_NAME
CREATE TABLESPACE bad ADD DATAFILE '.ibd';
SHOW WARNINGS;
--error ER_WRONG_FILE_NAME
CREATE TABLESPACE bad ADD DATAFILE './ibd';
SHOW WARNINGS;
--error ER_WRONG_FILE_NAME
CREATE TABLESPACE bad ADD DATAFILE './.ibd';
SHOW WARNINGS;
--error ER_WRONG_FILE_NAME
CREATE TABLESPACE bad ADD DATAFILE '../spaces/';
SHOW WARNINGS;
--error ER_WRONG_FILE_NAME
CREATE TABLESPACE bad ADD DATAFILE 'test/bad.ibd';
SHOW WARNINGS;
--error ER_WRONG_FILE_NAME
CREATE TABLESPACE bad ADD DATAFILE './test/bad.ibd';
SHOW WARNINGS;
--error ER_WRONG_FILE_NAME
CREATE TABLESPACE bad ADD DATAFILE './test//bad.ibd';
SHOW WARNINGS;
--error ER_WRONG_FILE_NAME
CREATE TABLESPACE bad ADD DATAFILE '../data/test/bad.ibd';
SHOW WARNINGS;
--error ER_WRONG_FILE_NAME
CREATE TABLESPACE bad ADD DATAFILE '../data/../data/test/bad.ibd';
SHOW WARNINGS;
--error ER_WRONG_FILE_NAME
CREATE TABLESPACE bad ADD DATAFILE '../does_not_exist/bad.ibd';
SHOW WARNINGS;

--echo #
--echo # Try a series of valid relative ADD DATAFILE entries.
--echo # General tablespace files can be in the datadir but they cannot be under the datadir.
--echo #
CREATE TABLESPACE ibport_s01 ADD DATAFILE 'ibport_s01.ibd';
CREATE TABLESPACE ibport_s02 ADD DATAFILE './ibport_s02.ibd';
CREATE TABLESPACE ibport_s03 ADD DATAFILE '../data/ibport_s03.ibd';
CREATE TABLESPACE ibport_s04 ADD DATAFILE 'test/../ibport_s04.ibd';
CREATE TABLESPACE ibport_s05 ADD DATAFILE './test/../ibport_s05.ibd';
CREATE TABLESPACE ibport_s06 ADD DATAFILE '..///data//test///..//ibport_s06.ibd';
CREATE TABLESPACE ibport_s12 ADD DATAFILE '.\\ibport_s12.ibd';
CREATE TABLESPACE ibport_s13 ADD DATAFILE '..\\data\\ibport_s13.ibd';
CREATE TABLESPACE ibport_s14 ADD DATAFILE 'test\\..\\ibport_s14.ibd';
CREATE TABLESPACE ibport_s15 ADD DATAFILE '.\\test\\..\\ibport_s15.ibd';
CREATE TABLESPACE ibport_s16 ADD DATAFILE '..\\\\\\data\\\\test\\\\\\..\\\\ibport_s16.ibd';

--echo # Directory listing of MYSQLD_DATADIR/ ibport*
--list_files $MYSQLD_DATADIR/ ibport*

--replace_result  $MYSQLD_DATADIR/ MYSQLD_DATADIR/  $MYSQLD_DATADIR MYSQLD_DATADIR/
SELECT s.space_type 'Type', s.name 'Space Name', d.path 'Path'
	FROM information_schema.innodb_tablespaces s, information_schema.innodb_datafiles d
	WHERE s.name like '%ibport%' AND s.space = d.space ORDER BY s.space;

--replace_result  $MYSQLD_DATADIR/ MYSQLD_DATADIR/  $MYSQLD_DATADIR MYSQLD_DATADIR/
SELECT s.space_type 'Type', s.name 'Space Name', f.file_name 'Path'
	FROM information_schema.innodb_tablespaces s, information_schema.files f
	WHERE s.name like '%ibport%' AND s.space = f.file_id ORDER BY s.space;

DROP TABLESPACE ibport_s01;
DROP TABLESPACE ibport_s02;
DROP TABLESPACE ibport_s03;
DROP TABLESPACE ibport_s04;
DROP TABLESPACE ibport_s05;
DROP TABLESPACE ibport_s06;
DROP TABLESPACE ibport_s12;
DROP TABLESPACE ibport_s13;
DROP TABLESPACE ibport_s14;
DROP TABLESPACE ibport_s15;
DROP TABLESPACE ibport_s16;

--disable_query_log
call mtr.add_suppression("\\[ERROR\\] \\[[^]]*\\] InnoDB: Operating system error number [0-9]* in a file operation");
--enable_query_log
