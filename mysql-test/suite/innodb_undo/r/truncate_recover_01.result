#
# WL#6965: Truncate UNDO logs during recovery.
# Crash before MDL.
#
# While the purge thread is turned off, perform enough DML to cause
# undo tablespace sizes grow beyond the truncation threshold.
# Then inject a crash at ib_undo_trunc_before_mdl and check that recovery occurs
# and that the truncation is completed if it was started.
# Create 2 explicit undo tablespaces.
CREATE UNDO TABLESPACE undo_003 ADD DATAFILE 'undo_003.ibu';
CREATE UNDO TABLESPACE undo_004 ADD DATAFILE 'undo_004.ibu';
# Make the 2 implicit undo tablespaces inactive so they get truncated.
SET GLOBAL innodb_purge_rseg_truncate_frequency=1;
ALTER UNDO TABLESPACE innodb_undo_001 SET INACTIVE;
ALTER UNDO TABLESPACE innodb_undo_002 SET INACTIVE;
# Make the 2 implicit undo tablespaces active.
ALTER UNDO TABLESPACE innodb_undo_001 SET ACTIVE;
ALTER UNDO TABLESPACE innodb_undo_002 SET ACTIVE;
# Get rid of the explcit undo tablespaces.
ALTER UNDO TABLESPACE undo_003 SET INACTIVE;
ALTER UNDO TABLESPACE undo_004 SET INACTIVE;
DROP UNDO TABLESPACE undo_003;
DROP UNDO TABLESPACE undo_004;
SET GLOBAL innodb_purge_rseg_truncate_frequency=default;
# The two undo tablespaces to truncate at shutdown will be innodb_undo_001 and innodb_undo_002
SELECT NAME, SPACE_TYPE, STATE, AUTOEXTEND_SIZE FROM INFORMATION_SCHEMA.INNODB_TABLESPACES
WHERE SPACE_TYPE = 'Undo' ORDER BY NAME;
NAME	SPACE_TYPE	STATE	AUTOEXTEND_SIZE
innodb_undo_001	Undo	active	0
innodb_undo_002	Undo	active	0
# Turn off purging so that undo logs build up in the undo tablespaces.
SET GLOBAL innodb_purge_stop_now=ON;
# Perform enough DML action so that undo tablespace sizes grow beyond
# the threshold set by --innodb-max-undo-log-size.
create table t1(
keyc int,
c1 char(255),
c2 char(255),
c3 char(255),
c4 char(255),
c5 char(255),
c6 char(255),
primary key(keyc)) engine = innodb;
CREATE PROCEDURE populate_t1()
BEGIN
DECLARE i INT DEFAULT 1;
while (i <= 16000) DO
insert into t1 values (i, 'a', 'b', 'c', 'd', 'e', 'f' );
SET i = i + 1;
END WHILE;
END |
call populate_t1();
delete from t1 where keyc < 10000;
update t1 set c1 = 'mysql' where keyc > 10000;
update t1 set c2 = 'mysql' where keyc > 10000;
update t1 set c3 = 'mysql' where keyc > 10000;
update t1 set c4 = 'mysql' where keyc > 10000;
update t1 set c5 = 'mysql' where keyc > 10000;
update t1 set c6 = 'mysql' where keyc > 10000;
# Start a new transaction for both tablespaces to get utilized
create table t2 (
keyc int,
c1 char(255),
c2 char(255),
c3 char(255),
c4 char(255),
c5 char(255),
c6 char(255),
primary key(keyc)) engine = innodb;
CREATE PROCEDURE populate_t2()
BEGIN
DECLARE i INT DEFAULT 1;
while (i <= 16000) DO
insert into t2 values (i, 'a', 'b', 'c', 'd', 'e', 'f' );
SET i = i + 1;
END WHILE;
END |
call populate_t2();
delete from t2 where keyc < 10000;
update t2 set c1 = 'mysql' where keyc > 10000;
update t2 set c2 = 'mysql' where keyc > 10000;
update t2 set c3 = 'mysql' where keyc > 10000;
update t2 set c4 = 'mysql' where keyc > 10000;
update t2 set c5 = 'mysql' where keyc > 10000;
update t2 set c6 = 'mysql' where keyc > 10000;
drop PROCEDURE populate_t1;
drop PROCEDURE populate_t2;
drop table t1, t2;
SELECT NAME, SPACE_TYPE, STATE, AUTOEXTEND_SIZE FROM INFORMATION_SCHEMA.INNODB_TABLESPACES
WHERE SPACE_TYPE = 'Undo' ORDER BY NAME;
NAME	SPACE_TYPE	STATE	AUTOEXTEND_SIZE
innodb_undo_001	Undo	active	0
innodb_undo_002	Undo	active	0
# Compare the undo tablespace sizes before and after DML growth.
Setting crash point = 'ib_undo_trunc_before_mdl'.
SET GLOBAL DEBUG = "+d,ib_undo_trunc_before_mdl";
# Do a slow shut down so the undo tablespaces can be truncated.
SET GLOBAL innodb_max_undo_log_size=20000000;
SET GLOBAL innodb_fast_shutdown=0;
SET GLOBAL innodb_purge_rseg_truncate_frequency=1;
SET GLOBAL innodb_purge_run_now=ON;
Recovery from crash point = 'ib_undo_trunc_before_mdl'.
Pattern "ib_undo_trunc_before_mdl" found
# restart:
SELECT NAME, SPACE_TYPE, STATE, AUTOEXTEND_SIZE FROM INFORMATION_SCHEMA.INNODB_TABLESPACES
WHERE SPACE_TYPE = 'Undo' ORDER BY NAME;
NAME	SPACE_TYPE	STATE	AUTOEXTEND_SIZE
innodb_undo_001	Undo	active	0
innodb_undo_002	Undo	active	0
#
# Cleanup
#
SELECT NAME, SPACE_TYPE, STATE, AUTOEXTEND_SIZE FROM INFORMATION_SCHEMA.INNODB_TABLESPACES
WHERE SPACE_TYPE = 'Undo' ORDER BY NAME;
NAME	SPACE_TYPE	STATE	AUTOEXTEND_SIZE
innodb_undo_001	Undo	active	0
innodb_undo_002	Undo	active	0
