CALL mtr.add_suppression("Statement is unsafe because it is being used inside a XA transaction");
SET @save_session_debug= @@SESSION.debug;
CREATE TABLE t1 (a INT PRIMARY KEY);
XA START 'xa1';
INSERT INTO t1 VALUES (1);
XA END 'xa1';
SET @@SESSION.debug = "+d,simulate_xa_prepare_failure_in_cache_finalize";
XA PREPARE 'xa1';
Got one of the listed errors
XA ROLLBACK 'xa1';
ERROR XAE04: XAER_NOTA: Unknown XID
XA COMMIT 'xa1';
ERROR XAE04: XAER_NOTA: Unknown XID
XA START 'xa1';
INSERT INTO t1 VALUES (1);
XA END 'xa1';
SET @@SESSION.debug = "+d,simulate_failure_in_before_commit_hook";
XA PREPARE 'xa1';
Got one of the listed errors
XA ROLLBACK 'xa1';
ERROR XAE04: XAER_NOTA: Unknown XID
XA COMMIT 'xa1';
ERROR XAE04: XAER_NOTA: Unknown XID
XA START 'xa1';
INSERT INTO t1 VALUES (1);
XA END 'xa1';
SET @@SESSION.debug = "+d,simulate_transaction_rollback_request";
XA PREPARE 'xa1';
Got one of the listed errors
XA ROLLBACK 'xa1';
ERROR XAE04: XAER_NOTA: Unknown XID
XA COMMIT 'xa1';
ERROR XAE04: XAER_NOTA: Unknown XID
SET @@SESSION.debug= @save_session_debug;
DROP TABLE t1;
