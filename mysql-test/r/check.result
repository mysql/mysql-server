drop table if exists t1,t2;
drop view if exists v1;
create table t1(n int not null, key(n), key(n), key(n), key(n));
Warnings:
Warning	1831	Duplicate index 'n_2' defined on the table 'test.t1'. This is deprecated and will be disallowed in a future release.
Warning	1831	Duplicate index 'n_3' defined on the table 'test.t1'. This is deprecated and will be disallowed in a future release.
Warning	1831	Duplicate index 'n_4' defined on the table 'test.t1'. This is deprecated and will be disallowed in a future release.
check table t1 extended;
insert into t1 values (200000);
Table	Op	Msg_type	Msg_text
test.t1	check	status	OK
drop table t1;
Create table t1(f1 int);
Create table t2(f1 int);
Create view v1 as Select * from t1;
Check Table v1,t2;
Table	Op	Msg_type	Msg_text
test.v1	check	status	OK
test.t2	check	status	OK
drop view v1;
drop table t1, t2;
#
# Bug#56422 CHECK TABLE run when the table is locked reports corruption
#           along with timeout
#
DROP TABLE IF EXISTS t1;
CREATE TABLE t1(a INT);
LOCK TABLE t1 WRITE;
# Connection con1
SET lock_wait_timeout= 1;
CHECK TABLE t1;
Table	Op	Msg_type	Msg_text
test.t1	check	Error	Lock wait timeout exceeded; try restarting transaction
test.t1	check	status	Operation failed
# Connection default
UNLOCK TABLES;
DROP TABLE t1;
#
# Bug#24741307: add last_checked_for_upgrade column to dd.tables table
#
# Checking default storage engine (Innodb)
CREATE TABLE t1(i INT);
# Table has been created with last_checked_for_upgrade = 0,
# so the check will be performed
CHECK TABLE t1 FOR UPGRADE;
Table	Op	Msg_type	Msg_text
test.t1	check	status	OK
# Checking again will detect that the table has already been checked
CHECK TABLE t1 FOR UPGRADE;
Table	Op	Msg_type	Msg_text
test.t1	check	status	Table is already up to date
ALTER TABLE t1 CHANGE COLUMN i j INT, ALGORITHM=INPLACE;
# Checking again after ALTER INPLACE to verify that the check status is
# still present
CHECK TABLE t1 FOR UPGRADE;
Table	Op	Msg_type	Msg_text
test.t1	check	status	Table is already up to date
ALTER TABLE t1 CHANGE COLUMN j k INT, ALGORITHM=COPY;
# Checking again after ALTER COPY to verify that the check status is
# still present
CHECK TABLE t1 FOR UPGRADE;
Table	Op	Msg_type	Msg_text
test.t1	check	status	Table is already up to date
ALTER TABLE t1 ADD COLUMN l INT, ALGORITHM=INSTANT;
# Checking again after ALTER INSTANT to verify that the check status
# still present
CHECK TABLE t1 FOR UPGRADE;
Table	Op	Msg_type	Msg_text
test.t1	check	status	Table is already up to date
TRUNCATE TABLE t1;
# Checking again after TRUNCATE to verify that the check status
# still present
CHECK TABLE t1 FOR UPGRADE;
Table	Op	Msg_type	Msg_text
test.t1	check	status	Table is already up to date
# Cleanup
DROP TABLE t1;
#
# Bug#28220374 	T != NULLPTR' AT BOOL)::<LAMBDA IN SQL/SQL_ADMIN.CC
#
# Verify that OK is returned for temporary tables
CREATE TEMPORARY TABLE tt1(i INT);
CHECK TABLE tt1 FOR UPGRADE;
Table	Op	Msg_type	Msg_text
test.tt1	check	status	OK
DROP TABLE tt1;
#
# Bug#33777821 Memory leak when checking a view of a view
#
CREATE VIEW w AS SELECT 1 AS a;
CREATE VIEW v AS SELECT a FROM w;
CHECK TABLE v;
Table	Op	Msg_type	Msg_text
test.v	check	status	OK
CREATE VIEW u AS SELECT * FROM v;
CHECK TABLE u;
Table	Op	Msg_type	Msg_text
test.u	check	status	OK
DROP VIEW u, v, w;
# end Bug#33777821
