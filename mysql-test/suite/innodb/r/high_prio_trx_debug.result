#
# Bug #21143276	HANGING DUE TO ASYNC_ROLLBACK
#
CREATE TABLE t1 (c1 INT NOT NULL PRIMARY KEY) ENGINE=InnoDB;
INSERT INTO t1 VALUES (0);
START TRANSACTION;
insert into t1 values(1);
UPDATE t1 SET c1=2 WHERE c1=0;
SET DEBUG_SYNC='ha_innobase_info_low SIGNAL update_cmd WAIT_FOR enter_innodb';
select * from t1;

# On connection 1
include/start_transaction_high_prio.inc
START TRANSACTION /* HIGH PRIORITY */;
set debug_sync='now WAIT_FOR update_cmd';
select * from t1;
c1
0
SET DEBUG_SYNC='trx_kill_blocking_enter SIGNAL enter_innodb';
UPDATE t1 SET c1=2 WHERE c1=0;
commit;
ERROR 40001: Deadlock found when trying to get lock; try restarting transaction
commit;
select * from t1;
c1
2
DROP TABLE t1;
#
# Bug #25032066: PREPARED TRANSACTION SHOULD NOT BE ROLLED BACK
#
CREATE TABLE t1(col1 int PRIMARY KEY, col2 int);
INSERT INTO t1 VALUES(1, 10);
XA START 'xid1';
UPDATE t1 SET col2 = 20;
XA END 'xid1';
XA PREPARE 'xid1';
include/start_transaction_high_prio.inc
START TRANSACTION /* HIGH PRIORITY */;
UPDATE t1 SET col2 = 30 WHERE col2 = 10;;
XA COMMIT 'xid1';
COMMIT;
SELECT * FROM t1;
col1	col2
1	20
DROP TABLE t1;
