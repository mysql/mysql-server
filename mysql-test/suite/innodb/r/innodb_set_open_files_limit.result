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
SELECT innodb_set_open_files_limit(1.1);
ERROR HY000: Can't initialize function 'innodb_set_open_files_limit'; Invalid first argument type.
SELECT innodb_set_open_files_limit(1e1);
ERROR HY000: Can't initialize function 'innodb_set_open_files_limit'; Invalid first argument type.
SELECT innodb_set_open_files_limit('AUTO');
ERROR HY000: Can't initialize function 'innodb_set_open_files_limit'; Invalid first argument type.
SELECT innodb_set_open_files_limit();
ERROR HY000: Can't initialize function 'innodb_set_open_files_limit'; Invalid number of arguments.
SELECT innodb_set_open_files_limit(1, 2);
ERROR HY000: Can't initialize function 'innodb_set_open_files_limit'; Invalid number of arguments.
SELECT innodb_set_open_files_limit(1, 2, 3, 4);
ERROR HY000: Can't initialize function 'innodb_set_open_files_limit'; Invalid number of arguments.
SELECT innodb_set_open_files_limit(NULL);
ERROR HY000: Can't initialize function 'innodb_set_open_files_limit'; First argument must not be null.
select @@global.innodb_open_files;
@@global.innodb_open_files
4000
SELECT innodb_set_open_files_limit(-3);
ERROR HY000: Incorrect arguments to innodb_set_open_files_limit. New limit value can't be smaller than 10.
SELECT innodb_set_open_files_limit(1);
ERROR HY000: Incorrect arguments to innodb_set_open_files_limit. New limit value can't be smaller than 10.
SELECT innodb_set_open_files_limit(3);
ERROR HY000: Incorrect arguments to innodb_set_open_files_limit. New limit value can't be smaller than 10.
SELECT innodb_set_open_files_limit(9);
ERROR HY000: Incorrect arguments to innodb_set_open_files_limit. New limit value can't be smaller than 10.
SELECT innodb_set_open_files_limit(2147483648);
ERROR HY000: Incorrect arguments to innodb_set_open_files_limit. New limit value can't be larger than 2147483647.
SELECT innodb_set_open_files_limit(12342147483647);
ERROR HY000: Incorrect arguments to innodb_set_open_files_limit. New limit value can't be larger than 2147483647.
SELECT innodb_set_open_files_limit(10);
ERROR HY000: Incorrect arguments to innodb_set_open_files_limit. Cannot update innodb_open_files to this value. Not enough files could be closed in last 5 seconds or a number of files that cannot be closed would exceed 90% of the new limit. Consider setting it above 15.
SELECT innodb_set_open_files_limit(14);
ERROR HY000: Incorrect arguments to innodb_set_open_files_limit. Cannot update innodb_open_files to this value. Not enough files could be closed in last 5 seconds or a number of files that cannot be closed would exceed 90% of the new limit. Consider setting it above 15.
SELECT innodb_set_open_files_limit(15);
innodb_set_open_files_limit(15)
15
SELECT innodb_set_open_files_limit(14);
ERROR HY000: Incorrect arguments to innodb_set_open_files_limit. Cannot update innodb_open_files to this value. Not enough files could be closed in last 5 seconds or a number of files that cannot be closed would exceed 90% of the new limit. Consider setting it above 15.
SELECT innodb_set_open_files_limit(2147483647);
innodb_set_open_files_limit(2147483647)
2147483647
SELECT innodb_set_open_files_limit(4000);
innodb_set_open_files_limit(4000)
4000
SELECT innodb_set_open_files_limit(150);
innodb_set_open_files_limit(150)
150
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
