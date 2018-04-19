#
# Bug#20872655 XA ROLLBACK IS NOT CRASH-SAFE
#
CREATE TABLE t(a INT PRIMARY KEY, b INT UNIQUE) ENGINE=INNODB;
XA START 'zombie';
insert into t(a) values (0);
update t set b=a;
SELECT COUNT(*) FROM t;
COUNT(*)
1
XA END 'zombie';
XA PREPARE 'zombie';
SET DEBUG_SYNC='trx_xa_rollback SIGNAL s1 WAIT_FOR s2';
XA ROLLBACK 'zombie';
SET DEBUG_SYNC='now WAIT_FOR s1';
SET GLOBAL innodb_log_checkpoint_now=ON;
# Kill and restart
XA COMMIT 'zombie';
ERROR XAE04: XAER_NOTA: Unknown XID
SELECT COUNT(*) FROM t;
COUNT(*)
0
DROP TABLE t;
