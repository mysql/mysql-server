#
# Inject failures in order to get code coverage.
# Use explicit undo tablespaces and make the implicit spaces inactive.
#
SET GLOBAL innodb_fast_shutdown = 0;
# restart
SET @old_truncate_frequency = @@innodb_purge_rseg_truncate_frequency;
SET GLOBAL innodb_purge_rseg_truncate_frequency=5;
CREATE UNDO TABLESPACE undo_003 ADD DATAFILE 'undo_003.ibu';
CREATE UNDO TABLESPACE undo_004 ADD DATAFILE 'undo_004.ibu';
CREATE UNDO TABLESPACE undo_005 ADD DATAFILE 'undo_005.ibu';
ALTER UNDO TABLESPACE innodb_undo_001 SET INACTIVE;
ALTER UNDO TABLESPACE innodb_undo_002 SET INACTIVE;
SELECT NAME, SPACE_TYPE, STATE FROM INFORMATION_SCHEMA.INNODB_TABLESPACES
WHERE SPACE_TYPE = 'Undo' ORDER BY NAME;
NAME	SPACE_TYPE	STATE
innodb_undo_001	Undo	empty
innodb_undo_002	Undo	empty
undo_003	Undo	active
undo_004	Undo	active
undo_005	Undo	active
SELECT TABLESPACE_NAME, FILE_TYPE, FILE_NAME FROM INFORMATION_SCHEMA.FILES
WHERE FILE_NAME LIKE '%undo%' ORDER BY TABLESPACE_NAME;
TABLESPACE_NAME	FILE_TYPE	FILE_NAME
innodb_undo_001	UNDO LOG	./undo_001
innodb_undo_002	UNDO LOG	./undo_002
undo_003	UNDO LOG	./undo_003.ibu
undo_004	UNDO LOG	./undo_004.ibu
undo_005	UNDO LOG	./undo_005.ibu
#
# Bug#28966457: IF CREATE UNDO TABLESPACE FAILS, INCOMPLETE UNDO
#               TABLESPACE SHOULD BE CLEANED UP
#
SET GLOBAL DEBUG="+d,ib_undo_tablespace_create_fail";
CREATE UNDO TABLESPACE undo_006 ADD DATAFILE 'undo_006.ibu';
ERROR HY000: Failed to create UNDO TABLESPACE undo_006
SET GLOBAL DEBUG="-d,ib_undo_tablespace_create_fail";
SELECT NAME, SPACE_TYPE, STATE FROM INFORMATION_SCHEMA.INNODB_TABLESPACES
WHERE SPACE_TYPE = 'Undo' ORDER BY NAME;
NAME	SPACE_TYPE	STATE
innodb_undo_001	Undo	empty
innodb_undo_002	Undo	empty
undo_003	Undo	active
undo_004	Undo	active
undo_005	Undo	active
SELECT TABLESPACE_NAME, FILE_TYPE, FILE_NAME FROM INFORMATION_SCHEMA.FILES
WHERE FILE_NAME LIKE '%undo%' ORDER BY TABLESPACE_NAME;
TABLESPACE_NAME	FILE_TYPE	FILE_NAME
innodb_undo_001	UNDO LOG	./undo_001
innodb_undo_002	UNDO LOG	./undo_002
undo_003	UNDO LOG	./undo_003.ibu
undo_004	UNDO LOG	./undo_004.ibu
undo_005	UNDO LOG	./undo_005.ibu
# Make sure file is not left behind in datadir
#
# Inject a failure to open an undo tablespace after it is created.
# The file remains on disk along with the truncate log file.
#
SET GLOBAL DEBUG="+d,ib_undo_tablespace_open_fail";
CREATE UNDO TABLESPACE undo_006 ADD DATAFILE 'undo_006.ibu';
ERROR HY000: Failed to create UNDO TABLESPACE undo_006
SET GLOBAL DEBUG="-d,ib_undo_tablespace_open_fail";
SELECT NAME, SPACE_TYPE, STATE FROM INFORMATION_SCHEMA.INNODB_TABLESPACES
WHERE SPACE_TYPE = 'Undo' ORDER BY NAME;
NAME	SPACE_TYPE	STATE
innodb_undo_001	Undo	empty
innodb_undo_002	Undo	empty
undo_003	Undo	active
undo_004	Undo	active
undo_005	Undo	active
SELECT TABLESPACE_NAME, FILE_TYPE, FILE_NAME FROM INFORMATION_SCHEMA.FILES
WHERE FILE_NAME LIKE '%undo%' ORDER BY TABLESPACE_NAME;
TABLESPACE_NAME	FILE_TYPE	FILE_NAME
innodb_undo_001	UNDO LOG	./undo_001
innodb_undo_002	UNDO LOG	./undo_002
undo_003	UNDO LOG	./undo_003.ibu
undo_004	UNDO LOG	./undo_004.ibu
undo_005	UNDO LOG	./undo_005.ibu
# Make sure file is not left behind in datadir
#
# Inject a failure to get the MDL during undo truncate.
#
SET GLOBAL DEBUG="+d,ib_undo_trunc_fail_get_mdl";
ALTER UNDO TABLESPACE undo_003 SET INACTIVE;
ALTER UNDO TABLESPACE undo_003 SET ACTIVE;
SELECT NAME, SPACE_TYPE, STATE FROM INFORMATION_SCHEMA.INNODB_TABLESPACES
WHERE SPACE_TYPE = 'Undo' ORDER BY NAME;
NAME	SPACE_TYPE	STATE
innodb_undo_001	Undo	empty
innodb_undo_002	Undo	empty
undo_003	Undo	active
undo_004	Undo	active
undo_005	Undo	active
#
# Inject a failure to locate the undo space during undo truncate.
#
SET GLOBAL DEBUG="+d,ib_undo_trunc_fail_marked_space";
ALTER UNDO TABLESPACE undo_003 SET INACTIVE;
ALTER UNDO TABLESPACE undo_003 SET ACTIVE;
SELECT NAME, SPACE_TYPE, STATE FROM INFORMATION_SCHEMA.INNODB_TABLESPACES
WHERE SPACE_TYPE = 'Undo' ORDER BY NAME;
NAME	SPACE_TYPE	STATE
innodb_undo_001	Undo	empty
innodb_undo_002	Undo	empty
undo_003	Undo	active
undo_004	Undo	active
undo_005	Undo	active
#
# Inject a failure to write the undo trunc log file.
#
SET GLOBAL DEBUG="+d,ib_undo_trunc_fail_start_logging";
ALTER UNDO TABLESPACE undo_003 SET INACTIVE;
ALTER UNDO TABLESPACE undo_003 SET ACTIVE;
SELECT NAME, SPACE_TYPE, STATE FROM INFORMATION_SCHEMA.INNODB_TABLESPACES
WHERE SPACE_TYPE = 'Undo' ORDER BY NAME;
NAME	SPACE_TYPE	STATE
innodb_undo_001	Undo	empty
innodb_undo_002	Undo	empty
undo_003	Undo	active
undo_004	Undo	active
undo_005	Undo	active
#
# Inject a simulated fast shutdown during undo truncate. The injection
# does't really shutdown, it just does not finish the truncate.
#
SET GLOBAL DEBUG="+d,ib_undo_trunc_fail_fast_shutdown";
ALTER UNDO TABLESPACE undo_003 SET INACTIVE;
ALTER UNDO TABLESPACE undo_003 SET ACTIVE;
SELECT NAME, SPACE_TYPE, STATE FROM INFORMATION_SCHEMA.INNODB_TABLESPACES
WHERE SPACE_TYPE = 'Undo' ORDER BY NAME;
NAME	SPACE_TYPE	STATE
innodb_undo_001	Undo	empty
innodb_undo_002	Undo	empty
undo_003	Undo	active
undo_004	Undo	active
undo_005	Undo	active
#
# Inject a failure to actually truncate the undo space.
#
SET GLOBAL DEBUG="+d,ib_undo_trunc_fail_truncate";
ALTER UNDO TABLESPACE undo_003 SET INACTIVE;
ALTER UNDO TABLESPACE undo_003 SET ACTIVE;
SELECT NAME, SPACE_TYPE, STATE FROM INFORMATION_SCHEMA.INNODB_TABLESPACES
WHERE SPACE_TYPE = 'Undo' ORDER BY NAME;
NAME	SPACE_TYPE	STATE
innodb_undo_001	Undo	empty
innodb_undo_002	Undo	empty
undo_003	Undo	active
undo_004	Undo	active
undo_005	Undo	active
#
# Cleanup
#
SET GLOBAL DEBUG="-d,ib_undo_trunc_fail_get_mdl";
SET GLOBAL DEBUG="-d,ib_undo_trunc_fail_marked_space";
SET GLOBAL DEBUG="-d,ib_undo_trunc_fail_start_logging";
SET GLOBAL DEBUG="-d,ib_undo_trunc_fail_fast_shutdown";
SET GLOBAL DEBUG="-d,ib_undo_trunc_fail_truncate";
ALTER UNDO TABLESPACE innodb_undo_001 SET ACTIVE;
ALTER UNDO TABLESPACE innodb_undo_002 SET ACTIVE;
ALTER UNDO TABLESPACE undo_003 SET INACTIVE;
ALTER UNDO TABLESPACE undo_004 SET INACTIVE;
ALTER UNDO TABLESPACE undo_005 SET INACTIVE;
DROP UNDO TABLESPACE undo_003;
DROP UNDO TABLESPACE undo_004;
DROP UNDO TABLESPACE undo_005;
SET GLOBAL innodb_purge_rseg_truncate_frequency=@old_truncate_frequency;
