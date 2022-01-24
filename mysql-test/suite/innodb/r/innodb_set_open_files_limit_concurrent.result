CREATE UNDO TABLESPACE undo_009 ADD DATAFILE 'undo_009.ibu';
CREATE UNDO TABLESPACE undo_008 ADD DATAFILE 'undo_008.ibu';
CREATE UNDO TABLESPACE undo_007 ADD DATAFILE 'undo_007.ibu';
CREATE UNDO TABLESPACE undo_006 ADD DATAFILE 'undo_006.ibu';
CREATE UNDO TABLESPACE undo_005 ADD DATAFILE 'undo_005.ibu';
CREATE UNDO TABLESPACE undo_004 ADD DATAFILE 'undo_004.ibu';
CREATE UNDO TABLESPACE undo_003 ADD DATAFILE 'undo_003.ibu';
CREATE UNDO TABLESPACE undo_002 ADD DATAFILE 'undo_002.ibu';
CREATE UNDO TABLESPACE undo_001 ADD DATAFILE 'undo_001.ibu';
CREATE UNDO TABLESPACE undo_000 ADD DATAFILE 'undo_000.ibu';
SET @start_global_value = @@GLOBAL.innodb_open_files;
SELECT @start_global_value;
@start_global_value
4000
SELECT innodb_set_open_files_limit(150);
innodb_set_open_files_limit(150)
150
SET DEBUG_SYNC='fil_open_files_desired_limit_set SIGNAL started WAIT_FOR proceed';
SELECT innodb_set_open_files_limit(200);;
SET DEBUG_SYNC= 'now WAIT_FOR started';
SELECT innodb_set_open_files_limit(250);
ERROR HY000: The innodb_set_open_files_limit was called when there was already another concurrent call to this procedure. No action was taken. Wait for other calls to innodb_set_open_files_limit to finish before retrying.
SET DEBUG_SYNC= 'now SIGNAL proceed';
innodb_set_open_files_limit(200)
200
SELECT @@GLOBAL.innodb_open_files;
@@GLOBAL.innodb_open_files
200
ALTER UNDO TABLESPACE undo_009 SET INACTIVE;
ALTER UNDO TABLESPACE undo_008 SET INACTIVE;
ALTER UNDO TABLESPACE undo_007 SET INACTIVE;
ALTER UNDO TABLESPACE undo_006 SET INACTIVE;
ALTER UNDO TABLESPACE undo_005 SET INACTIVE;
ALTER UNDO TABLESPACE undo_004 SET INACTIVE;
ALTER UNDO TABLESPACE undo_003 SET INACTIVE;
ALTER UNDO TABLESPACE undo_002 SET INACTIVE;
ALTER UNDO TABLESPACE undo_001 SET INACTIVE;
ALTER UNDO TABLESPACE undo_000 SET INACTIVE;
DROP UNDO TABLESPACE undo_009;
DROP UNDO TABLESPACE undo_008;
DROP UNDO TABLESPACE undo_007;
DROP UNDO TABLESPACE undo_006;
DROP UNDO TABLESPACE undo_005;
DROP UNDO TABLESPACE undo_004;
DROP UNDO TABLESPACE undo_003;
DROP UNDO TABLESPACE undo_002;
DROP UNDO TABLESPACE undo_001;
DROP UNDO TABLESPACE undo_000;
SELECT innodb_set_open_files_limit(@start_global_value);
innodb_set_open_files_limit(@start_global_value)
4000
SELECT @@GLOBAL.innodb_open_files;
@@GLOBAL.innodb_open_files
4000
