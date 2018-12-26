# Creating table t1
CREATE TABLE t1(c1 INT NOT NULL, c2 INT);
INSERT INTO t1 VALUES (1,1), (2,2), (3,1), (4,2), (5,1), (6,2), (7,3);
# Creating table t2
CREATE TABLE t2(c1 INT NOT NULL, c2 INT, PRIMARY KEY(c1));
INSERT INTO t2 VALUES (1,1), (2,2), (3,1), (4,2), (5,1), (6,2), (7,3);
# Count of elements in table t1
SELECT COUNT(*) FROM t1;
COUNT(*)
7
###################################################################
# Run plugin
###################################################################
INSTALL PLUGIN test_sql_lock SONAME 'TEST_SQL_LOCK';
###################################################################
# Stop plugin
###################################################################
UNINSTALL PLUGIN test_sql_lock;
###################################################################
# Plugin log
###################################################################
========================================================================
Test in a server thread

Opening Session 1
Opening Session 2

===================================================================
Isolation Level : READ COMMITTED
===================================================================
-------------------------------------------------------------------
Session 1 :
-------------------------------------------------------------------
SET AUTOCOMMIT = 0
SET SESSION TRANSACTION ISOLATION LEVEL READ COMMITTED
INSERT INTO test.t1 VALUES (8,4)
-------------------------------------------------------------------
Session 2 :
-------------------------------------------------------------------
SET AUTOCOMMIT = 0
SET SESSION TRANSACTION ISOLATION LEVEL READ COMMITTED
SELECT COUNT(*) FROM test.t1
COUNT(*)
7
-------------------------------------------------------------------
Session 1 :
-------------------------------------------------------------------
COMMIT
SET AUTOCOMMIT = 1
-------------------------------------------------------------------
Session 2 :
-------------------------------------------------------------------
SELECT COUNT(*) FROM test.t1
COUNT(*)
8
COMMIT
SET AUTOCOMMIT = 1
SELECT COUNT(*) FROM test.t1
COUNT(*)
8

===================================================================
Isolation Level : READ UNCOMMITTED
===================================================================
-------------------------------------------------------------------
Session 1 :
-------------------------------------------------------------------
SET AUTOCOMMIT = 0
SET SESSION TRANSACTION ISOLATION LEVEL READ UNCOMMITTED
INSERT INTO test.t1 VALUES (9,5)
-------------------------------------------------------------------
Session 2 :
-------------------------------------------------------------------
SET AUTOCOMMIT = 0
SET SESSION TRANSACTION ISOLATION LEVEL READ UNCOMMITTED
SELECT COUNT(*) FROM test.t1
COUNT(*)
9
-------------------------------------------------------------------
Session 1 :
-------------------------------------------------------------------
COMMIT
SET AUTOCOMMIT = 1
-------------------------------------------------------------------
Session 2 :
-------------------------------------------------------------------
SELECT COUNT(*) FROM test.t1
COUNT(*)
9
COMMIT
SET AUTOCOMMIT = 1
SELECT COUNT(*) FROM test.t1
COUNT(*)
9

===================================================================
Isolation Level : REPEATABLE READ
===================================================================
-------------------------------------------------------------------
Session 1 :
-------------------------------------------------------------------
SET AUTOCOMMIT = 0
SET SESSION TRANSACTION ISOLATION LEVEL REPEATABLE READ
SELECT COUNT(*) FROM test.t1
COUNT(*)
9
-------------------------------------------------------------------
Session 2 :
-------------------------------------------------------------------
SET AUTOCOMMIT = 0
SET SESSION TRANSACTION ISOLATION LEVEL REPEATABLE READ
INSERT INTO test.t1 VALUES (10,6)
COMMIT
SET AUTOCOMMIT = 1
-------------------------------------------------------------------
Session 1 :
-------------------------------------------------------------------
SELECT COUNT(*) FROM test.t1
COUNT(*)
9
COMMIT
SELECT COUNT(*) FROM test.t1
COUNT(*)
10
SET AUTOCOMMIT = 1

===================================================================
Isolation Level : SERIALIZABLE
===================================================================
-------------------------------------------------------------------
Session 1 :
-------------------------------------------------------------------
SET AUTOCOMMIT = 0
SET SESSION TRANSACTION ISOLATION LEVEL SERIALIZABLE
INSERT INTO test.t1 VALUES (11,7)
-------------------------------------------------------------------
Session 2 :
-------------------------------------------------------------------
SET AUTOCOMMIT = 0
SELECT COUNT(*) FROM test.t1
COUNT(*)
10
-------------------------------------------------------------------
Session 1 :
-------------------------------------------------------------------
COMMIT
SET AUTOCOMMIT = 1
-------------------------------------------------------------------
Session 2 :
-------------------------------------------------------------------
SELECT COUNT(*) FROM test.t1
COUNT(*)
10
COMMIT
SELECT COUNT(*) FROM test.t1
COUNT(*)
11
SET AUTOCOMMIT = 1

===================================================================
Locking done by a SQL transaction
===================================================================
-------------------------------------------------------------------
Session 1 :
-------------------------------------------------------------------
BEGIN
DELETE FROM test.t1 WHERE c1 = 11
SELECT COUNT(*) FROM test.t1
COUNT(*)
10
-------------------------------------------------------------------
Session 2 :
-------------------------------------------------------------------
SELECT COUNT(*) FROM test.t1
COUNT(*)
11
-------------------------------------------------------------------
Session 1 :
-------------------------------------------------------------------
COMMIT
-------------------------------------------------------------------
Session 2 :
-------------------------------------------------------------------
SELECT COUNT(*) FROM test.t1
COUNT(*)
10

Closing Session 1
Closing Session 2

Opening Session 1
Opening Session 2
Opening Session 3
===================================================================
Locking using LOCK TABLE
===================================================================
-------------------------------------------------------------------
Session 1 :
-------------------------------------------------------------------
LOCK TABLE test.t1 READ
/*statement should return an error because table t1 is locked with a READ lock*/
INSERT INTO test.t1 VALUES (11,7)
error: 1099
error msg: Table 't1' was locked with a READ lock and can't be updated
SHOW OPEN TABLES FROM test LIKE 't1'
Database	Table	In_use	Name_locked
test		t1		1		0
-------------------------------------------------------------------
Session 2 :
-------------------------------------------------------------------
LOCK TABLE test.t2 WRITE
SHOW OPEN TABLES FROM test LIKE 't1'
Database	Table	In_use	Name_locked
test		t1		1		0
SHOW OPEN TABLES FROM test LIKE 't2'
Database	Table	In_use	Name_locked
test		t2		1		0
-------------------------------------------------------------------
Session 1 :
-------------------------------------------------------------------
SHOW OPEN TABLES FROM test LIKE 't2'
Database	Table	In_use	Name_locked
test		t2		1		0

Closing Session 1
===================================================================
Check session 1's lock after its close
===================================================================
-------------------------------------------------------------------
Session 2 :
-------------------------------------------------------------------
SHOW OPEN TABLES FROM test LIKE 't1'
Database	Table	In_use	Name_locked
test		t1		0		0
SHOW OPEN TABLES FROM test LIKE 't2'
Database	Table	In_use	Name_locked
test		t2		1		0

Closing Session 2

===================================================================
Check session 2's lock after its close
===================================================================
-------------------------------------------------------------------
Session 3 :
-------------------------------------------------------------------
SHOW OPEN TABLES FROM test LIKE 't1'
Database	Table	In_use	Name_locked
test		t1		0		0
SHOW OPEN TABLES FROM test LIKE 't2'
Database	Table	In_use	Name_locked
test		t2		0		0

Closing Session 3

Follows threaded run
========================================================================
init thread

Opening Session 1
Opening Session 2

===================================================================
Isolation Level : READ COMMITTED
===================================================================
-------------------------------------------------------------------
Session 1 :
-------------------------------------------------------------------
SET AUTOCOMMIT = 0
SET SESSION TRANSACTION ISOLATION LEVEL READ COMMITTED
INSERT INTO test.t1 VALUES (8,4)
-------------------------------------------------------------------
Session 2 :
-------------------------------------------------------------------
SET AUTOCOMMIT = 0
SET SESSION TRANSACTION ISOLATION LEVEL READ COMMITTED
SELECT COUNT(*) FROM test.t1
COUNT(*)
10
-------------------------------------------------------------------
Session 1 :
-------------------------------------------------------------------
COMMIT
SET AUTOCOMMIT = 1
-------------------------------------------------------------------
Session 2 :
-------------------------------------------------------------------
SELECT COUNT(*) FROM test.t1
COUNT(*)
11
COMMIT
SET AUTOCOMMIT = 1
SELECT COUNT(*) FROM test.t1
COUNT(*)
11

===================================================================
Isolation Level : READ UNCOMMITTED
===================================================================
-------------------------------------------------------------------
Session 1 :
-------------------------------------------------------------------
SET AUTOCOMMIT = 0
SET SESSION TRANSACTION ISOLATION LEVEL READ UNCOMMITTED
INSERT INTO test.t1 VALUES (9,5)
-------------------------------------------------------------------
Session 2 :
-------------------------------------------------------------------
SET AUTOCOMMIT = 0
SET SESSION TRANSACTION ISOLATION LEVEL READ UNCOMMITTED
SELECT COUNT(*) FROM test.t1
COUNT(*)
12
-------------------------------------------------------------------
Session 1 :
-------------------------------------------------------------------
COMMIT
SET AUTOCOMMIT = 1
-------------------------------------------------------------------
Session 2 :
-------------------------------------------------------------------
SELECT COUNT(*) FROM test.t1
COUNT(*)
12
COMMIT
SET AUTOCOMMIT = 1
SELECT COUNT(*) FROM test.t1
COUNT(*)
12

===================================================================
Isolation Level : REPEATABLE READ
===================================================================
-------------------------------------------------------------------
Session 1 :
-------------------------------------------------------------------
SET AUTOCOMMIT = 0
SET SESSION TRANSACTION ISOLATION LEVEL REPEATABLE READ
SELECT COUNT(*) FROM test.t1
COUNT(*)
12
-------------------------------------------------------------------
Session 2 :
-------------------------------------------------------------------
SET AUTOCOMMIT = 0
SET SESSION TRANSACTION ISOLATION LEVEL REPEATABLE READ
INSERT INTO test.t1 VALUES (10,6)
COMMIT
SET AUTOCOMMIT = 1
-------------------------------------------------------------------
Session 1 :
-------------------------------------------------------------------
SELECT COUNT(*) FROM test.t1
COUNT(*)
12
COMMIT
SELECT COUNT(*) FROM test.t1
COUNT(*)
13
SET AUTOCOMMIT = 1

===================================================================
Isolation Level : SERIALIZABLE
===================================================================
-------------------------------------------------------------------
Session 1 :
-------------------------------------------------------------------
SET AUTOCOMMIT = 0
SET SESSION TRANSACTION ISOLATION LEVEL SERIALIZABLE
INSERT INTO test.t1 VALUES (11,7)
-------------------------------------------------------------------
Session 2 :
-------------------------------------------------------------------
SET AUTOCOMMIT = 0
SELECT COUNT(*) FROM test.t1
COUNT(*)
13
-------------------------------------------------------------------
Session 1 :
-------------------------------------------------------------------
COMMIT
SET AUTOCOMMIT = 1
-------------------------------------------------------------------
Session 2 :
-------------------------------------------------------------------
SELECT COUNT(*) FROM test.t1
COUNT(*)
13
COMMIT
SELECT COUNT(*) FROM test.t1
COUNT(*)
14
SET AUTOCOMMIT = 1

===================================================================
Locking done by a SQL transaction
===================================================================
-------------------------------------------------------------------
Session 1 :
-------------------------------------------------------------------
BEGIN
DELETE FROM test.t1 WHERE c1 = 11
SELECT COUNT(*) FROM test.t1
COUNT(*)
13
-------------------------------------------------------------------
Session 2 :
-------------------------------------------------------------------
SELECT COUNT(*) FROM test.t1
COUNT(*)
14
-------------------------------------------------------------------
Session 1 :
-------------------------------------------------------------------
COMMIT
-------------------------------------------------------------------
Session 2 :
-------------------------------------------------------------------
SELECT COUNT(*) FROM test.t1
COUNT(*)
13

Closing Session 1
Closing Session 2

Opening Session 1
Opening Session 2
Opening Session 3
===================================================================
Locking using LOCK TABLE
===================================================================
-------------------------------------------------------------------
Session 1 :
-------------------------------------------------------------------
LOCK TABLE test.t1 READ
/*statement should return an error because table t1 is locked with a READ lock*/
INSERT INTO test.t1 VALUES (11,7)
error: 1099
error msg: Table 't1' was locked with a READ lock and can't be updated
SHOW OPEN TABLES FROM test LIKE 't1'
Database	Table	In_use	Name_locked
test		t1		1		0
-------------------------------------------------------------------
Session 2 :
-------------------------------------------------------------------
LOCK TABLE test.t2 WRITE
SHOW OPEN TABLES FROM test LIKE 't1'
Database	Table	In_use	Name_locked
test		t1		1		0
SHOW OPEN TABLES FROM test LIKE 't2'
Database	Table	In_use	Name_locked
test		t2		1		0
-------------------------------------------------------------------
Session 1 :
-------------------------------------------------------------------
SHOW OPEN TABLES FROM test LIKE 't2'
Database	Table	In_use	Name_locked
test		t2		1		0

Closing Session 1
===================================================================
Check session 1's lock after its close
===================================================================
-------------------------------------------------------------------
Session 2 :
-------------------------------------------------------------------
SHOW OPEN TABLES FROM test LIKE 't1'
Database	Table	In_use	Name_locked
test		t1		0		0
SHOW OPEN TABLES FROM test LIKE 't2'
Database	Table	In_use	Name_locked
test		t2		1		0

Closing Session 2

===================================================================
Check session 2's lock after its close
===================================================================
-------------------------------------------------------------------
Session 3 :
-------------------------------------------------------------------
SHOW OPEN TABLES FROM test LIKE 't1'
Database	Table	In_use	Name_locked
test		t1		0		0
SHOW OPEN TABLES FROM test LIKE 't2'
Database	Table	In_use	Name_locked
test		t2		0		0

Closing Session 3

deinit thread
SHOW OPEN TABLES FROM test LIKE 't1';
Database	Table	In_use	Name_locked
test	t1	0	0
SHOW OPEN TABLES FROM test LIKE 't2';
Database	Table	In_use	Name_locked
test	t2	0	0
# Dropping the created tables
DROP TABLE t1;
DROP TABLE t2;
