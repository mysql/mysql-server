#
# Bug #31467626 : SERVER CRASH OBSERVED AT ROTATE_INNODB_MASTER_KEY
#
SHOW VARIABLES LIKE "%innodb_undo_log_encrypt%";
Variable_name	Value
innodb_undo_log_encrypt	OFF
###############################################################################
# With keyring plugin and innodb_undo_log_encrypt=ON
###############################################################################
SHOW VARIABLES LIKE "%innodb_undo_log_encrypt%";
Variable_name	Value
innodb_undo_log_encrypt	ON
CREATE UNDO TABLESPACE undo_003 ADD DATAFILE 'undo_003.ibu';
CREATE UNDO TABLESPACE undo_004 ADD DATAFILE 'undo_004.ibu';
SELECT NAME, SPACE_TYPE, ENCRYPTION, STATE FROM INFORMATION_SCHEMA.INNODB_TABLESPACES WHERE SPACE_TYPE='Undo';
NAME	SPACE_TYPE	ENCRYPTION	STATE
innodb_undo_001	Undo	Y	active
innodb_undo_002	Undo	Y	active
undo_003	Undo	Y	active
undo_004	Undo	Y	active
SHOW VARIABLES LIKE "%innodb_undo_log_encrypt%";
Variable_name	Value
innodb_undo_log_encrypt	ON
CREATE DATABASE crypt;
DROP PROCEDURE IF EXISTS crypt.rotate_and_truncate;
Warnings:
Note	1305	PROCEDURE crypt.rotate_and_truncate does not exist
CREATE PROCEDURE crypt.rotate_and_truncate(IN start BIGINT)
BEGIN
SET @idx = start;
SET global innodb_undo_log_encrypt=ON;
WHILE (@idx > 0) DO
ALTER UNDO TABLESPACE undo_003 SET ACTIVE;
ALTER UNDO TABLESPACE undo_004 SET ACTIVE;
ALTER INSTANCE ROTATE INNODB MASTER KEY;
ALTER UNDO TABLESPACE undo_003 SET INACTIVE;
ALTER UNDO TABLESPACE undo_004 SET INACTIVE;
SET @idx = @idx - 1;
END WHILE;
END|
call crypt.rotate_and_truncate(50);
ALTER UNDO TABLESPACE undo_003 SET ACTIVE;
ALTER UNDO TABLESPACE undo_004 SET ACTIVE;
SELECT NAME, SPACE_TYPE, ENCRYPTION, STATE FROM INFORMATION_SCHEMA.INNODB_TABLESPACES WHERE SPACE_TYPE='Undo';
NAME	SPACE_TYPE	ENCRYPTION	STATE
innodb_undo_001	Undo	Y	active
innodb_undo_002	Undo	Y	active
undo_003	Undo	Y	active
undo_004	Undo	Y	active
#
# Make sure the rotation code recovers when
# one of the tables cannot be rotated.
#
SET SESSION DEBUG="+d,ib_encryption_rotate_skip";
ALTER INSTANCE ROTATE INNODB MASTER KEY;
SET SESSION DEBUG="-d,ib_encryption_rotate_skip";
Pattern "ib_encryption_rotate_skip" found
SELECT NAME, SPACE_TYPE, ENCRYPTION, STATE FROM INFORMATION_SCHEMA.INNODB_TABLESPACES WHERE SPACE_TYPE='Undo';
NAME	SPACE_TYPE	ENCRYPTION	STATE
innodb_undo_001	Undo	Y	active
innodb_undo_002	Undo	Y	active
undo_003	Undo	Y	active
undo_004	Undo	Y	active
ALTER INSTANCE ROTATE INNODB MASTER KEY;
SELECT NAME, SPACE_TYPE, ENCRYPTION, STATE FROM INFORMATION_SCHEMA.INNODB_TABLESPACES WHERE SPACE_TYPE='Undo';
NAME	SPACE_TYPE	ENCRYPTION	STATE
innodb_undo_001	Undo	Y	active
innodb_undo_002	Undo	Y	active
undo_003	Undo	Y	active
undo_004	Undo	Y	active
#
# Get the UNDO tablespaces ready to drop
#
ALTER UNDO TABLESPACE undo_003 SET INACTIVE;
SELECT NAME, SPACE_TYPE, ENCRYPTION, STATE FROM INFORMATION_SCHEMA.INNODB_TABLESPACES WHERE SPACE_TYPE='Undo';
NAME	SPACE_TYPE	ENCRYPTION	STATE
innodb_undo_001	Undo	Y	active
innodb_undo_002	Undo	Y	active
undo_003	Undo	Y	empty
undo_004	Undo	Y	active
#
# Be sure that undo tablespaces can be dropped when a concurrent
# ENCRYPTION ROTATE is occurring
#
# Start and pause a ROTATE INNODB MASTER KEY in a second thread
SET DEBUG_SYNC='ib_pause_encryption_rotate SIGNAL rotate_paused WAIT_FOR rotate_go';
ALTER INSTANCE ROTATE INNODB MASTER KEY;
SET DEBUG_SYNC='now WAIT_FOR rotate_paused';
# In a third thread, try to drop an undo tablespace
DROP UNDO TABLESPACE undo_003;
# In a fourth thread, try to set an undo tablespace inactive
# which would cause the purge thread to truncate it.
ALTER UNDO TABLESPACE undo_004 SET INACTIVE;;
# Switch to the default thread and check that the DDL are waiting
Pattern "Dropped undo tablespace 'undo_003'" not found
SELECT NAME, SPACE_TYPE, ENCRYPTION, STATE FROM INFORMATION_SCHEMA.INNODB_TABLESPACES WHERE SPACE_TYPE='Undo';
NAME	SPACE_TYPE	ENCRYPTION	STATE
innodb_undo_001	Undo	Y	active
innodb_undo_002	Undo	Y	active
undo_003	Undo	Y	empty
undo_004	Undo	Y	active
# Resume the ROTATE thread.
SET DEBUG_SYNC='now SIGNAL rotate_go';
# Reap the other three threads.
# Return to the default thread and disconnect the other three.
#
# Cleanup
#
DROP UNDO TABLESPACE undo_004;
DROP PROCEDURE crypt.rotate_and_truncate;
DROP DATABASE crypt;
# Reset to initial state
# restart:
Pattern "The InnoDB Encryption Master Key has been rotated" found
Pattern "Dropped undo tablespace 'undo_003'" found
Pattern "Dropped undo tablespace 'undo_004'" found
