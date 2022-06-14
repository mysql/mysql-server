#
# Truncate explicitly created UNDO logs before and after recovery.
#
CREATE TABLE t1 (a SERIAL, b INT, c TEXT);
#
# Test 1: Check affect of transactions on undo truncation online
#
INSERT INTO t1(b, c) VALUES (0,"zero");
# Create 2 explicit undo tablespaces.
CREATE UNDO TABLESPACE undo_003 ADD DATAFILE 'undo_003.ibu';
CREATE UNDO TABLESPACE undo_004 ADD DATAFILE 'undo_004.ibu';
# Make the 2 implicit undo tablespaces inactive.
ALTER UNDO TABLESPACE innodb_undo_001 SET INACTIVE;
ALTER UNDO TABLESPACE innodb_undo_002 SET INACTIVE;
SELECT NAME, SPACE_TYPE, STATE FROM INFORMATION_SCHEMA.INNODB_TABLESPACES WHERE SPACE_TYPE = 'Undo' ORDER BY NAME;
NAME	SPACE_TYPE	STATE
innodb_undo_001	Undo	empty
innodb_undo_002	Undo	empty
undo_003	Undo	active
undo_004	Undo	active
# Switch to connection  #1
XA START 'x1';
INSERT INTO t1(b, c) VALUES (1,"one");
SELECT * FROM t1;
a	b	c
1	0	zero
2	1	one
XA END 'x1';
XA PREPARE 'x1';
# Switch to connection  #2
XA START 'x2';
INSERT INTO t1(b, c) VALUES (2,"two");
SELECT * FROM t1;
a	b	c
1	0	zero
3	2	two
XA END 'x2';
XA PREPARE 'x2';
# Switch to connection  Default
SELECT * FROM t1;
a	b	c
1	0	zero
ALTER UNDO TABLESPACE innodb_undo_001 SET ACTIVE;
ALTER UNDO TABLESPACE innodb_undo_002 SET ACTIVE;
ALTER UNDO TABLESPACE undo_003 SET INACTIVE;
ALTER UNDO TABLESPACE undo_004 SET INACTIVE;
SELECT NAME, SPACE_TYPE, STATE FROM INFORMATION_SCHEMA.INNODB_TABLESPACES WHERE SPACE_TYPE = 'Undo' ORDER BY NAME;
NAME	SPACE_TYPE	STATE
innodb_undo_001	Undo	active
innodb_undo_002	Undo	active
undo_003	Undo	inactive
undo_004	Undo	inactive
# Switch to connection #1
XA COMMIT 'x1';
# Switch to connection #2
XA ROLLBACK 'x2';
# Switch to connection Default
SELECT * FROM t1;
a	b	c
1	0	zero
2	1	one
SELECT NAME, SPACE_TYPE, STATE FROM INFORMATION_SCHEMA.INNODB_TABLESPACES WHERE SPACE_TYPE = 'Undo' ORDER BY NAME;
NAME	SPACE_TYPE	STATE
innodb_undo_001	Undo	active
innodb_undo_002	Undo	active
undo_003	Undo	empty
undo_004	Undo	empty
DELETE FROM t1;
#
# Test 2: Check affect of transactions on undo truncation after a restart.
#
INSERT INTO t1(b, c) VALUES (0,"zero");
# Make the two explicit undo tablespaces active.
ALTER UNDO TABLESPACE undo_003 SET ACTIVE;
ALTER UNDO TABLESPACE undo_004 SET ACTIVE;
# Make the 2 implicit undo tablespaces inactive.
ALTER UNDO TABLESPACE innodb_undo_001 SET INACTIVE;
ALTER UNDO TABLESPACE innodb_undo_002 SET INACTIVE;
SELECT NAME, SPACE_TYPE, STATE FROM INFORMATION_SCHEMA.INNODB_TABLESPACES WHERE SPACE_TYPE = 'Undo' ORDER BY NAME;
NAME	SPACE_TYPE	STATE
innodb_undo_001	Undo	empty
innodb_undo_002	Undo	empty
undo_003	Undo	active
undo_004	Undo	active
# Switch to connection #1
XA START 'x1';
INSERT INTO t1(b, c) VALUES (1,"one");
SELECT * FROM t1;
a	b	c
4	0	zero
5	1	one
XA END 'x1';
XA PREPARE 'x1';
# Switch to connection #2
XA START 'x2';
INSERT INTO t1(b, c) VALUES (2,"two");
SELECT * FROM t1;
a	b	c
4	0	zero
6	2	two
XA END 'x2';
XA PREPARE 'x2';
# Switch to connection Default
# Try to truncate the two explicit undo tablespaces. They will be stuck inactive.
ALTER UNDO TABLESPACE innodb_undo_001 SET ACTIVE;
ALTER UNDO TABLESPACE innodb_undo_002 SET ACTIVE;
ALTER UNDO TABLESPACE undo_003 SET INACTIVE;
ALTER UNDO TABLESPACE undo_004 SET INACTIVE;
SELECT * FROM t1;
a	b	c
4	0	zero
SELECT SLEEP(1);
SLEEP(1)
0
SELECT NAME, SPACE_TYPE, STATE FROM INFORMATION_SCHEMA.INNODB_TABLESPACES WHERE SPACE_TYPE = 'Undo' ORDER BY NAME;
NAME	SPACE_TYPE	STATE
innodb_undo_001	Undo	active
innodb_undo_002	Undo	active
undo_003	Undo	inactive
undo_004	Undo	inactive
# restart
# After a restart, they are still stuck inactive.
SELECT NAME, SPACE_TYPE, STATE FROM INFORMATION_SCHEMA.INNODB_TABLESPACES WHERE SPACE_TYPE = 'Undo' ORDER BY NAME;
NAME	SPACE_TYPE	STATE
innodb_undo_001	Undo	active
innodb_undo_002	Undo	active
undo_003	Undo	inactive
undo_004	Undo	inactive
XA COMMIT 'x1';
XA ROLLBACK 'x2';
SELECT * FROM t1;
a	b	c
4	0	zero
5	1	one
SELECT NAME, SPACE_TYPE, STATE FROM INFORMATION_SCHEMA.INNODB_TABLESPACES WHERE SPACE_TYPE = 'Undo' ORDER BY NAME;
NAME	SPACE_TYPE	STATE
innodb_undo_001	Undo	active
innodb_undo_002	Undo	active
undo_003	Undo	empty
undo_004	Undo	empty
DELETE FROM t1;
#
# Test 3: Check affect of transactions on undo truncation after a crash.
#
INSERT INTO t1(b, c) VALUES (0,"zero");
# Make the two explicit undo tablespaces active.
ALTER UNDO TABLESPACE undo_003 SET ACTIVE;
ALTER UNDO TABLESPACE undo_004 SET ACTIVE;
# Make the 2 implicit undo tablespaces inactive.
ALTER UNDO TABLESPACE innodb_undo_001 SET INACTIVE;
ALTER UNDO TABLESPACE innodb_undo_002 SET INACTIVE;
SELECT NAME, SPACE_TYPE, STATE FROM INFORMATION_SCHEMA.INNODB_TABLESPACES WHERE SPACE_TYPE = 'Undo' ORDER BY NAME;
NAME	SPACE_TYPE	STATE
innodb_undo_001	Undo	empty
innodb_undo_002	Undo	empty
undo_003	Undo	active
undo_004	Undo	active
# Switch to connection #3
XA START 'x3';
INSERT INTO t1(b, c) VALUES (1,"one");
SELECT * FROM t1;
a	b	c
7	0	zero
8	1	one
XA END 'x3';
XA PREPARE 'x3';
# Switch to connection #4
XA START 'x4';
INSERT INTO t1(b, c) VALUES (2,"two");
SELECT * FROM t1;
a	b	c
7	0	zero
9	2	two
XA END 'x4';
XA PREPARE 'x4';
# Switch to connection Default
# Try to truncate the two explicit undo tablespaces. They will be stuck inactive.
ALTER UNDO TABLESPACE innodb_undo_001 SET ACTIVE;
ALTER UNDO TABLESPACE innodb_undo_002 SET ACTIVE;
ALTER UNDO TABLESPACE undo_003 SET INACTIVE;
ALTER UNDO TABLESPACE undo_004 SET INACTIVE;
SELECT * FROM t1;
a	b	c
7	0	zero
SELECT SLEEP(1);
SLEEP(1)
0
SELECT NAME, SPACE_TYPE, STATE FROM INFORMATION_SCHEMA.INNODB_TABLESPACES WHERE SPACE_TYPE = 'Undo' ORDER BY NAME;
NAME	SPACE_TYPE	STATE
innodb_undo_001	Undo	active
innodb_undo_002	Undo	active
undo_003	Undo	inactive
undo_004	Undo	inactive
# Kill and restart
# After a kill and restart, they are still stuck inactive.
SELECT NAME, SPACE_TYPE, STATE FROM INFORMATION_SCHEMA.INNODB_TABLESPACES WHERE SPACE_TYPE = 'Undo' ORDER BY NAME;
NAME	SPACE_TYPE	STATE
innodb_undo_001	Undo	active
innodb_undo_002	Undo	active
undo_003	Undo	inactive
undo_004	Undo	inactive
XA COMMIT 'x3';
XA ROLLBACK 'x4';
SELECT * FROM t1;
a	b	c
7	0	zero
8	1	one
SELECT NAME, SPACE_TYPE, STATE FROM INFORMATION_SCHEMA.INNODB_TABLESPACES WHERE SPACE_TYPE = 'Undo' ORDER BY NAME;
NAME	SPACE_TYPE	STATE
innodb_undo_001	Undo	active
innodb_undo_002	Undo	active
undo_003	Undo	empty
undo_004	Undo	empty
DELETE FROM t1;
#
# Cleanup
#
# restart
DROP TABLE t1;
SELECT NAME, SPACE_TYPE, STATE FROM INFORMATION_SCHEMA.INNODB_TABLESPACES
WHERE SPACE_TYPE = 'Undo' ORDER BY NAME;
NAME	SPACE_TYPE	STATE
innodb_undo_001	Undo	active
innodb_undo_002	Undo	active
undo_003	Undo	empty
undo_004	Undo	empty
DROP UNDO TABLESPACE undo_003;
DROP UNDO TABLESPACE undo_004;
