# Scenario:1
#  T1= ({W(B)}, HIGH_PRIORITY)
#  T2= ( Try to take table level lock )
# Outcome: T2 waits for t1 to acquire table lock
CREATE TABLE t1 (c1 INT NOT NULL PRIMARY KEY) ENGINE=InnoDB;
INSERT INTO t1 VALUES (0);

# On connection 1
include/start_transaction_high_prio.inc
START TRANSACTION /* HIGH PRIORITY */;
UPDATE t1 SET c1=1 WHERE c1=0;

# On connection 2
LOCK TABLE t1 WRITE/* From connection 2 */;

# On connection 1
COMMIT;

# On connection 2.
SELECT * FROM t1;
c1
1
UNLOCK TABLES;
include/assert.inc ['There is a 1 in t1']
DROP TABLE t1;
# Scenario:2
#  T1= (LOCK TABLE)
#  T2= ({ W(B)},HIGH_PRIORITY)
# Outcome : high prio transaction T2 waits for T1.
CREATE TABLE t1 (c1 INT NOT NULL PRIMARY KEY) ENGINE=InnoDB;
INSERT INTO t1 VALUES (0);

# On connection 2
LOCK TABLE t1 WRITE/* From connection 2 */;
UPDATE t1 SET c1=2 WHERE c1=0;

# On connection 1
include/start_transaction_high_prio.inc
START TRANSACTION /* HIGH PRIORITY */;
UPDATE t1 SET c1=1 WHERE c1=0;
# On connection 2
COMMIT;
UNLOCK TABLES;
COMMIT;
SELECT * FROM t1;
c1
2
DROP TABLE t1;
# Scenario:3
#  T1= ({R(B)}, HIGH_PRIORITY)
#  T2= ({ R(B)},HIGH_PRIORITY)
# Outcome: Both T1 and T2 continue as they are read only.
CREATE TABLE t1 (c1 INT NOT NULL PRIMARY KEY) ENGINE=InnoDB;
INSERT INTO t1 VALUES (0);

# On connection 1
START TRANSACTION READ ONLY /* HIGH PRIORITY */ ;
SELECT c1 FROM t1 WHERE c1=0 LOCK IN SHARE MODE;
c1
0

# On connection 2
SET GLOBAL DEBUG='+d,dbug_set_high_prio_trx';
START TRANSACTION READ ONLY /* HIGH PRIORITY */ ;
SELECT c1 FROM t1 WHERE c1=0 LOCK IN SHARE MODE;
c1
0
UPDATE t1 SET c1=1 WHERE c1=0;
ERROR 25006: Cannot execute statement in a READ ONLY transaction.
COMMIT;
COMMIT;
include/assert.inc ['There is a 1 in t1']
DROP TABLE t1;
# Scenario:4
# Start of transaction causes table locks acquired with LOCK TABLES to be released
# LOCK TABLES Implicitly commit any active transaction
CREATE TABLE t1 (c1 INT NOT NULL PRIMARY KEY) ENGINE=InnoDB;
CREATE TABLE t2 (c1 INT NOT NULL PRIMARY KEY) ENGINE=InnoDB;
INSERT INTO t1 VALUES (0);
INSERT INTO t2 VALUES (0);
LOCK TABLE t2 WRITE;
# In_use is 1
SHOW OPEN TABLES LIKE 't2';
Database	Table	In_use	Name_locked
test	t2	1	0
include/start_transaction_high_prio.inc
START TRANSACTION /* HIGH PRIORITY */;
# In_use is 0 , its released due to start transaction statement
SHOW OPEN TABLES LIKE 't2';
Database	Table	In_use	Name_locked
test	t2	0	0
UPDATE t1 SET c1=1;
# Implicitly commits active transaction
LOCK TABLE t1 READ;
UNLOCK TABLES;
include/assert.inc ['There is a 1 in t1']
DROP TABLES t1,t2;
# Scenario:5
# Multiple concurrent high prio transactions with the same priority.
# Outcome : Transaction which is marked high prio first completes first and other high prio txns wait
CREATE TABLE t1 (c1 INT NOT NULL PRIMARY KEY) ENGINE=InnoDB;
INSERT INTO t1 VALUES (0);
include/start_transaction_high_prio.inc
START TRANSACTION /* HIGH PRIORITY */;
# connection 1:
INSERT INTO t1 VALUES (0);
ERROR 23000: Duplicate entry '0' for key 't1.PRIMARY'
UPDATE t1 SET c1=1 WHERE c1=0;
include/start_transaction_high_prio.inc
START TRANSACTION /* HIGH PRIORITY */;
# connection 2: wait - update
UPDATE t1 SET c1=2 WHERE c1=1 /* From connection 2 */;
include/start_transaction_high_prio.inc
START TRANSACTION /* HIGH PRIORITY */;
# connection 3: wait - delete
DELETE FROM t1 WHERE c1=1 /* From connection 3 */;
include/start_transaction_high_prio.inc
START TRANSACTION /* HIGH PRIORITY */;
# connection 4: wait - insert
# wait over
INSERT INTO t1 VALUES (1) /* From connection 4 */;
include/start_transaction_high_prio.inc
START TRANSACTION /* HIGH PRIORITY */;
# connection 5: Insert goes but as it new row
INSERT INTO t1 VALUES (50);
# connection 1:
COMMIT;
# connection 2: Reap UPDATE t1 SET c1=2 WHERE c1=1
COMMIT;
# connection 3: Reap DELETE FROM t1 WHERE c1=1
COMMIT;
# connection 4: Reap INSERT INTO t1 VALUES (1)
COMMIT;
INSERT INTO t1 VALUES (3);
UPDATE t1 SET c1=4 WHERE c1=2;
INSERT INTO t1 VALUES (50);
COMMIT;
ERROR 23000: Duplicate entry '50' for key 't1.PRIMARY'
include/assert.inc ['There is a 3 in t1']
# Expected values : 1,3,4,50
SELECT * FROM t1 order by c1;
c1
1
3
4
50
DROP TABLES t1;
# Scenario:6
# Mark XA transaction as high prio.
CREATE TABLE t1 (c1 INT NOT NULL PRIMARY KEY) ENGINE=InnoDB;
INSERT INTO t1 VALUES (0),(1);
START TRANSACTION;
UPDATE t1 SET c1=11 WHERE c1=0;
XA START 'xa_high_prio' /* HIGH PRIORITY */ ;
UPDATE t1 SET c1=10 WHERE c1=0;
XA END 'xa_high_prio';
XA PREPARE 'xa_high_prio';
XA COMMIT 'xa_high_prio';
COMMIT;
ERROR HY000: Got error 149 - 'Lock deadlock; Retry transaction' during COMMIT
SELECT * FROM t1;
c1
1
10
DROP TABLE t1;
# Scenario:7
# Multiple XA transaction as high prio.
# Outcome : detect deadlock .
CREATE TABLE t1 (c1 INT NOT NULL PRIMARY KEY) ENGINE=InnoDB;
INSERT INTO t1 VALUES (0),(1);
START TRANSACTION;
UPDATE t1 SET c1=11 WHERE c1=0;
XA START 'xa_high_prio' /* HIGH PRIORITY */ ;
UPDATE t1 SET c1=10 WHERE c1=0;
XA START 'second_xa_high_prio' /* HIGH PRIORITY */ ;
UPDATE t1 SET c1=13 WHERE c1=0;
XA END 'xa_high_prio';
XA PREPARE 'xa_high_prio';
XA COMMIT 'xa_high_prio';
COMMIT;
ERROR HY000: Got error 149 - 'Lock deadlock; Retry transaction' during COMMIT
Reap UPDATE t1 SET c1=13 WHERE c1=0;
SELECT * FROM t1;
c1
1
10
DROP TABLE t1;
#
# Bug#21075892 SHUTDOWN HANG, WAITING FOR ASYNC ROLLBACK TO FINISH
#
CREATE TABLE t1(col1 INT PRIMARY KEY, col2 INT) ENGINE=InnoDB;
INSERT INTO t1 values(10, 10), (20, 20), (30, 30);
#connection 0
START TRANSACTION;
#Lock record (col1 = 20)
UPDATE t1 set col2 = 25 where col1 = 20;
#connection 1
SET SESSION TRANSACTION ISOLATION LEVEL READ COMMITTED;
include/start_transaction_high_prio.inc
START TRANSACTION /* HIGH PRIORITY */;
#Semi-consistent read: Attempt lock (col1 = 20) and skip in SQL
UPDATE t1 SET col2 = 20 where col1 > 10 and col1 <30 and col2 = 30;
COMMIT WORK;
#connection 0
ROLLBACK WORK;
DROP TABLE t1;
