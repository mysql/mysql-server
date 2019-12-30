--source include/force_myisam_default.inc
--source include/have_myisam.inc

--source include/have_innodb_default_undo_tablespaces.inc

--echo #
--echo # MyISAM does not support undo tablespaces commands
--echo #

--source include/have_innodb_default_undo_tablespaces.inc

--error ER_ILLEGAL_HA_CREATE_OPTION
CREATE UNDO TABLESPACE undo_003 ADD DATAFILE 'undo_003.ibu' ENGINE MyISAM;

--error ER_ILLEGAL_HA_CREATE_OPTION
ALTER UNDO TABLESPACE undo_003 SET ACTIVE ENGINE MyISAM;

--error ER_ILLEGAL_HA_CREATE_OPTION
ALTER UNDO TABLESPACE undo_003 SET INACTIVE ENGINE MyISAM;

--error ER_ILLEGAL_HA_CREATE_OPTION
DROP UNDO TABLESPACE undo_003 ENGINE MyISAM;

