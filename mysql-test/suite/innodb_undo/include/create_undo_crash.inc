let $MYSQLD_DATADIR= `SELECT @@datadir`;

if ( $CRASH_ON_DROP_UNDO_TABLESPACE == 0 )
{
--echo Setting server to crash at "$crash_point" during UNDO truncate
eval SET GLOBAL DEBUG = "+d,$crash_point";
--source include/expect_crash.inc
--error 2013
CREATE UNDO TABLESPACE undo_003 ADD DATAFILE 'undo_003.ibu';

--echo # Restart server normally
--let $restart_parameters = restart:
--source include/start_mysqld.inc

SELECT name, space_type, state, autoextend_size FROM
    INFORMATION_SCHEMA.INNODB_TABLESPACES WHERE space_type="Undo";

# list files to check the presence of undo files
--list_files $MYSQLD_DATADIR/ undo*

--echo #
--echo # Cleanup
--echo #

--error 3510
DROP UNDO TABLESPACE undo_003;
}


if ( $CRASH_ON_DROP_UNDO_TABLESPACE == 1 )
{

CREATE UNDO TABLESPACE undo_003 ADD DATAFILE 'undo_003.ibu';
ALTER UNDO TABLESPACE undo_003 SET INACTIVE;
let $inactive_undo_space = undo_003;
source include/wait_until_undo_space_is_empty.inc;

SELECT name, space_type, state, autoextend_size FROM
    INFORMATION_SCHEMA.INNODB_TABLESPACES WHERE space_type="Undo";

eval SET GLOBAL DEBUG = "+d,$crash_point";
--source include/expect_crash.inc
--error 2013
DROP UNDO TABLESPACE undo_003;

--echo # Restart server normally
--let $restart_parameters = restart:
--source include/start_mysqld.inc

SELECT name, space_type, state, autoextend_size FROM
    INFORMATION_SCHEMA.INNODB_TABLESPACES WHERE space_type="Undo";

if ( $SKIP_DROP == 0) {
DROP UNDO TABLESPACE undo_003;
}
}

