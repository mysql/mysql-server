include/only_with_option.inc [GLOBAL.xa_detach_on_prepare = 1]
#
# WL#7155: Test 1: check that prepared transaction branch
# is recovered after abnormal server termination.
#
CREATE TABLE t1 (a INT) ENGINE=INNODB;
XA START 'xid1';
INSERT INTO t1 VALUES (1);
XA END 'xid1';
XA PREPARE 'xid1';
XA RECOVER;
ERROR HY000: Lost connection to MySQL server during query
# restart
XA RECOVER;
formatID	gtrid_length	bqual_length	data
1	4	0	xid1
SELECT * FROM t1;
a
XA COMMIT 'xid1';
SELECT * FROM t1;
a
1
DROP TABLE t1;
#
# WL#7155: Test 2: check that in case of abnormal server termination
# the changes done by unprepared transaction branch will be rolled back
# after the server restart.
# 
CREATE TABLE t1 (a INT) ENGINE=INNODB;
XA START 'xid1';
INSERT INTO t1 VALUES (1);
XA END 'xid1';
XA RECOVER;
ERROR HY000: Lost connection to MySQL server during query
# restart
XA RECOVER;
formatID	gtrid_length	bqual_length	data
SELECT * FROM t1;
a
DROP TABLE t1;
#
# WL#7155: Test 3: Check that escaped "'" symbol for xid value
# is handled correctly during recovering XA transaction. 
#
CREATE TABLE t1 (a INT) ENGINE=INNODB;
XA START 'xid1', 'br\'_1';
INSERT INTO t1 VALUES (1);
XA END 'xid1', 'br\'_1';
XA PREPARE 'xid1', 'br\'_1';
XA RECOVER;
ERROR HY000: Lost connection to MySQL server during query
# restart
XA RECOVER;
formatID	gtrid_length	bqual_length	data
1	4	5	xid1br'_1
XA ROLLBACK 'xid1', 'br\'_1';
DROP TABLE t1;
#
# Bug#18068253 - XA START WITH THE EXIST XID, AND THEN XA COMMIT WITH THE XID WILL FAILED
#
CREATE TABLE t1 (a INT) ENGINE=INNODB;
XA START 'xid1';
INSERT INTO t1 VALUES (1);
XA END 'xid1';
XA PREPARE 'xid1';
set session debug="+d,crash_after_xa_recover";
XA RECOVER;
ERROR HY000: Lost connection to MySQL server during query
# restart
set session debug="-d,crash_after_xa_recover";
XA RECOVER;
formatID	gtrid_length	bqual_length	data
1	4	0	xid1
XA START 'xid1';
ERROR XAE08: XAER_DUPID: The XID already exists
XA COMMIT 'xid1';
DROP TABLE t1;
#
# Bug#20538956 - XA PREPARE ERROR BRANCH LEAVES THE TRANSACTION IN SCREWED STATE
#
CREATE TABLE t1 (a INT);
XA START 'xid1';
INSERT INTO t1 VALUES (1);
XA END 'xid1';
SET @@session.debug = '+d,simulate_xa_failure_prepare';
XA PREPARE 'xid1';
ERROR XA100: XA_RBROLLBACK: Transaction branch was rolled back
SET @@session.debug = '-d,simulate_xa_failure_prepare';
XA ROLLBACK 'xid1';
ERROR XAE04: XAER_NOTA: Unknown XID
XA START 'trx_another_one';
#
# Bug#20488921 - ERROR PROPAGATION DOES NOT FULLY WORK IN XA
#
SET SESSION xa_detach_on_prepare = OFF;
XA START 'xid1';
INSERT INTO t1 VALUES (1);
XA END 'xid1';
XA PREPARE 'xid1';
SET @@session.debug= '+d,simulate_xa_commit_log_failure';
XA COMMIT 'xid1';
ERROR XAE03: XAER_RMERR: Fatal error occurred in the transaction branch - check your data for consistency
SET @@session.debug= '-d,simulate_xa_commit_log_failure';
INSERT INTO t1 VALUES (2);
DROP TABLE t1;
SET SESSION xa_detach_on_prepare = OLD_VALUE_xa_detach_on_prepare;
#
# Bug#23264552 - XA: ASSERT `m_status == da_error' IN mysql_errno:sql_error.h:385
#
CREATE TABLE t1 (a INT);
XA START 'xid1';
INSERT INTO t1 VALUES (1);
XA END 'xid1';
SET @@session.debug = '+d,simulate_xa_failure_prepare_in_engine';
XA PREPARE 'xid1';
ERROR XA100: XA_RBROLLBACK: Transaction branch was rolled back
SET @@session.debug = '-d,simulate_xa_failure_prepare_in_engine';
XA ROLLBACK 'xid1';
ERROR XAE04: XAER_NOTA: Unknown XID
XA START 'trx_another_one';
DROP TABLE t1;
#
# BUG#29293279 -- XA COMMIT MAY LEAD REPLICATION BROKEN
#
Establish session con1
CREATE TABLE t1(a INT);
XA START 'xid1';
INSERT INTO t1 VALUES (1);
XA END 'xid1';
XA PREPARE 'xid1';
Disconnect con1 to save the XA transaction with xid1 into the cache
connect  con2,localhost,root,,;
set debug_sync='detached_xa_commit_after_acquire_commit_lock SIGNAL parked WAIT_FOR go';
XA COMMIT 'xid1';
connection default;
set debug_sync='now WAIT_FOR parked';
set debug_sync='detached_xa_commit_before_acquire_xa_lock SIGNAL go';
XA COMMIT 'xid1';
ERROR XAE04: XAER_NOTA: Unknown XID
DROP TABLE t1;
#
# Bug#32025408: MAIN.XA_PREPARED_BINLOG_OFF,BINLOG.BINLOG_XA_PREPARED_DISCON FAILS ON ASAN/UBSAN
#
CREATE TABLE t1(i INT);
# Create a connection which as non-prepared XA trans.
XA START 'trx1';
INSERT INTO t1 set i=0;
XA END 'trx1';
# Create new connection which will execute XA COMMIT and be blocked between search in transaction cache and
# accessing xid_state (to see if txn can be committed).
SET DEBUG_SYNC="before_accessing_xid_state SIGNAL proceed_disconnect WAIT_FOR proceed_check_xid_state";
XA COMMIT 'trx1';
# Terminate conA which will delete the Transaction_ctx and with it the XID_STATE
SET DEBUG_SYNC="now WAIT_FOR proceed_disconnect";
# Unblock XA COMMIT. Without fix this will access the now freed memory, and trigger ASAN error.
SET DEBUG_SYNC="now SIGNAL proceed_check_xid_state";
ERROR XAE04: XAER_NOTA: Unknown XID
DROP TABLE t1;
# Test error handling in XA PREPARE.
CREATE TABLE t1(d VARCHAR(128));
XA START 'xid1';
INSERT INTO t1 VALUES ('I: The first string'), ('I: The second string');
# Test errors which leave the XA transaction in place
XA PREPARE 'xid1';
ERROR XAE07: XAER_RMFAIL: The command cannot be executed when global transaction is in the  ACTIVE state
XA END 'xid1';
XA PREPARE 'xidX';
ERROR XAE04: XAER_NOTA: Unknown XID
XA ROLLBACK 'xid1';
SELECT * FROM t1;
d
XA START 'xid2';
INSERT INTO t1 VALUES ('II: The first string'), ('II: The second string');
XA END 'xid2';
SET DEBUG = "+d,xaprep_mdl_fail";
XA PREPARE 'xid2';
ERROR XA100: XA_RBROLLBACK: Transaction branch was rolled back
SET DEBUG = "-d,xaprep_mdl_fail";
SELECT * FROM t1;
d
XA START 'xid3';
INSERT INTO t1 VALUES ('III: The first string'), ('III: The second string');
XA END 'xid3';
SET DEBUG = "+d,xaprep_ha_xa_prepare_fail";
XA PREPARE 'xid3';
ERROR XA100: XA_RBROLLBACK: Transaction branch was rolled back
SET DEBUG = "-d,xaprep_ha_xa_prepare_fail";
SELECT * FROM t1;
d
XA START 'xid4';
INSERT INTO t1 VALUES ('IV: The first string'), ('IV: The second string');
XA END 'xid4';
SET DEBUG = "+d,xaprep_create_mdl_backup_fail";
XA PREPARE 'xid4';
ERROR HY000: Out of memory; check if mysqld or some other process uses all available memory; if not, you may have to use 'ulimit' to allow mysqld to use more memory or you can add more swap space
SHOW ERRORS;
Level	Code	Message
Error	1041	Out of memory; check if mysqld or some other process uses all available memory; if not, you may have to use 'ulimit' to allow mysqld to use more memory or you can add more swap space
Error	1402	XA_RBROLLBACK: Transaction branch was rolled back
SET DEBUG = "-d,xaprep_create_mdl_backup_fail";
SELECT * FROM t1;
d
XA START 'xid5';
INSERT INTO t1 VALUES ('V: The first string'), ('V: The second string');
XA END 'xid5';
SET DEBUG = "+d,xaprep_trans_detach_fail";
XA PREPARE 'xid5';
ERROR XA100: XA_RBROLLBACK: Transaction branch was rolled back
SET DEBUG = "-d,xaprep_trans_detach_fail";
SELECT * FROM t1;
d
# Cleanup
XA RECOVER;
formatID	gtrid_length	bqual_length	data
DROP TABLE t1;
