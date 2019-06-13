#
# Bug #21025880	DUPLICATE UK VALUES IN READ-COMMITTED(AGAIN)
#
CREATE TABLE t1 (
a INT NOT NULL,
b INT NOT NULL,
PRIMARY KEY(b),
UNIQUE KEY(a))
ENGINE=INNODB;
SET @old_innodb_stats_auto_recalc = @@innodb_stats_auto_recalc;
SET GLOBAL innodb_stats_auto_recalc = OFF;
SET GLOBAL innodb_purge_stop_now = ON;
SET @old_transaction_isolation = @@transaction_isolation;
SET GLOBAL transaction_isolation = 'READ-COMMITTED';
SET @old_innodb_lock_wait_timeout = @@innodb_lock_wait_timeout;
SET GLOBAL innodb_lock_wait_timeout = 1;
INSERT INTO t1 VALUES (1,1),(2,2);
DELETE FROM t1;
SET debug_sync = 'row_ins_sec_index_entry_dup_locks_created SIGNAL
con1_locks_done WAIT_FOR con1_go';
SET debug_sync = 'ha_commit_trans_after_acquire_commit_lock SIGNAL
con1_insert_done WAIT_FOR con1_finish';
REPLACE INTO t1 VALUES (1,2);
SET debug_sync = 'now WAIT_FOR con1_locks_done';
SET debug_sync = 'lock_wait_suspend_thread_enter SIGNAL con2_blocked
WAIT_FOR con2_go';
SET debug_sync = 'ha_commit_trans_after_acquire_commit_lock SIGNAL
con2_insert_done WAIT_FOR con2_finish';
SET debug_sync = 'ib_after_row_insert SIGNAL con2_insert_done';
REPLACE INTO t1 VALUES (1,3);
SET debug_sync = 'now WAIT_FOR con2_blocked';
SET GLOBAL innodb_purge_run_now=ON;
SET debug_sync = 'now SIGNAL con2_go WAIT_FOR con2_insert_done';
SET debug_sync = 'now SIGNAL con1_go WAIT_FOR con1_insert_done';
SET debug_sync = 'now SIGNAL con1_finish';
SET debug_sync = 'now SIGNAL con2_finish';
SELECT * FROM t1;
a	b
1	2
CHECK TABLE t1;
Table	Op	Msg_type	Msg_text
test.t1	check	status	OK
DROP TABLE t1;
SET GLOBAL innodb_stats_auto_recalc = @old_innodb_stats_auto_recalc;
SET GLOBAL transaction_isolation = @old_transaction_isolation;
SET GLOBAL innodb_lock_wait_timeout = @old_innodb_lock_wait_timeout;
SET DEBUG_SYNC= 'RESET';
CREATE TABLE t1 (
a INT NOT NULL,
b INT NOT NULL,
PRIMARY KEY(b),
UNIQUE KEY(a))
ENGINE=INNODB;
SET @old_innodb_stats_auto_recalc = @@innodb_stats_auto_recalc;
SET GLOBAL innodb_stats_auto_recalc = OFF;
SET GLOBAL innodb_purge_stop_now = ON;
SET @old_transaction_isolation = @@transaction_isolation;
SET GLOBAL transaction_isolation = 'READ-COMMITTED';
SET @old_innodb_lock_wait_timeout = @@innodb_lock_wait_timeout;
SET GLOBAL innodb_lock_wait_timeout = 1;
INSERT INTO t1 VALUES (1,1),(2,2);
DELETE FROM t1;
SET debug_sync = 'row_ins_sec_index_entry_dup_locks_created SIGNAL
con1_locks_done WAIT_FOR con1_go';
SET debug_sync = 'ha_commit_trans_after_acquire_commit_lock SIGNAL
con1_insert_done WAIT_FOR con1_finish';
INSERT INTO t1 values (1,2) ON DUPLICATE KEY UPDATE a=2;
SET debug_sync = 'now WAIT_FOR con1_locks_done';
SET debug_sync = 'lock_wait_suspend_thread_enter SIGNAL con2_blocked
WAIT_FOR con2_go';
SET debug_sync = 'ha_commit_trans_after_acquire_commit_lock SIGNAL
con2_insert_done WAIT_FOR con2_finish';
SET debug_sync = 'ib_after_row_insert SIGNAL con2_insert_done';
REPLACE INTO t1 VALUES (1,3);
SET debug_sync = 'now WAIT_FOR con2_blocked';
SET GLOBAL innodb_purge_run_now=ON;
SET debug_sync = 'now SIGNAL con2_go WAIT_FOR con2_insert_done';
SET debug_sync = 'now SIGNAL con1_go WAIT_FOR con1_insert_done';
SET debug_sync = 'now SIGNAL con1_finish';
SET debug_sync = 'now SIGNAL con2_finish';
SELECT * FROM t1;
a	b
1	2
CHECK TABLE t1;
Table	Op	Msg_type	Msg_text
test.t1	check	status	OK
DROP TABLE t1;
SET GLOBAL innodb_stats_auto_recalc = @old_innodb_stats_auto_recalc;
SET GLOBAL transaction_isolation = @old_transaction_isolation;
SET GLOBAL innodb_lock_wait_timeout = @old_innodb_lock_wait_timeout;
SET DEBUG_SYNC= 'RESET';
CREATE TABLE t1 (
a INT NOT NULL,
b INT NOT NULL,
PRIMARY KEY(b),
UNIQUE KEY(a))
ENGINE=INNODB;
SET @old_innodb_stats_auto_recalc = @@innodb_stats_auto_recalc;
SET GLOBAL innodb_stats_auto_recalc = OFF;
SET GLOBAL innodb_purge_stop_now = ON;
SET @old_transaction_isolation = @@transaction_isolation;
SET GLOBAL transaction_isolation = 'READ-COMMITTED';
SET @old_innodb_lock_wait_timeout = @@innodb_lock_wait_timeout;
SET GLOBAL innodb_lock_wait_timeout = 1;
INSERT INTO t1 VALUES (1,1),(2,2);
DELETE FROM t1;
SET debug_sync = 'row_ins_sec_index_entry_dup_locks_created SIGNAL
con1_locks_done WAIT_FOR con1_go';
SET debug_sync = 'ha_commit_trans_after_acquire_commit_lock SIGNAL
con1_insert_done WAIT_FOR con1_finish';
INSERT INTO t1 VALUES (1,2);
SET debug_sync = 'now WAIT_FOR con1_locks_done';
SET debug_sync = 'lock_wait_suspend_thread_enter SIGNAL con2_blocked
WAIT_FOR con2_go';
SET debug_sync = 'ha_commit_trans_after_acquire_commit_lock SIGNAL
con2_insert_done WAIT_FOR con2_finish';
SET debug_sync = 'ib_after_row_insert SIGNAL con2_insert_done';
REPLACE INTO t1 VALUES (1,3);
SET debug_sync = 'now WAIT_FOR con2_blocked';
SET GLOBAL innodb_purge_run_now=ON;
SET debug_sync = 'now SIGNAL con2_go WAIT_FOR con2_insert_done';
SET debug_sync = 'now SIGNAL con1_go WAIT_FOR con1_insert_done';
SET debug_sync = 'now SIGNAL con1_finish';
SET debug_sync = 'now SIGNAL con2_finish';
SELECT * FROM t1;
a	b
1	2
CHECK TABLE t1;
Table	Op	Msg_type	Msg_text
test.t1	check	status	OK
DROP TABLE t1;
SET GLOBAL innodb_stats_auto_recalc = @old_innodb_stats_auto_recalc;
SET GLOBAL transaction_isolation = @old_transaction_isolation;
SET GLOBAL innodb_lock_wait_timeout = @old_innodb_lock_wait_timeout;
SET DEBUG_SYNC= 'RESET';
#
# Bug#24344131 ASSERT !OTHER_LOCK IN LOCK_REC_ADD_TO_QUEUE AT LOCK0LOCK.CC:1641
#
CREATE TABLE t1(a INT, b INT, primary key(a));
INSERT INTO t1 VALUES(101, 20);
SET DEBUG_SYNC="before_lock_trx_release_locks SIGNAL s3 WAIT_FOR s5";
BEGIN;
INSERT INTO t1 VALUES(10, 20);
SET DEBUG_SYNC="before_lock_rec_convert_impl_to_expl_for_trx SIGNAL s2 WAIT_FOR s4";
SET DEBUG_SYNC="before_lock_wait_suspend SIGNAL s5 WAIT_FOR s7";
UPDATE t1 SET b=25 WHERE a=10;
SET DEBUG_SYNC="now WAIT_FOR s2";
COMMIT;
SET DEBUG_SYNC="now WAIT_FOR s3";
SET DEBUG_SYNC="after_lock_clust_rec_read_check_and_lock SIGNAL s4 WAIT_FOR s6";
UPDATE t1 SET b=30 WHERE a=10;
SET DEBUG_SYNC="now SIGNAL s6";
SET DEBUG_SYNC="now SIGNAL s7";
DROP TABLE t1;
#
# Bug #29004362  LOCK->TRX->DUPLICATES CAN NOT BE USED TO DETERMINE IF
#                LOCK IS CREATED BY IODKU
#
CREATE TABLE t1 (
a INT NOT NULL,
b INT NOT NULL,
c INT NOT NULL,
PRIMARY KEY(b),
UNIQUE KEY(a),
UNIQUE KEY(c))
ENGINE=INNODB;
SET @old_innodb_stats_auto_recalc = @@innodb_stats_auto_recalc;
SET GLOBAL innodb_stats_auto_recalc = OFF;
SET GLOBAL innodb_purge_stop_now = ON;
SET @old_transaction_isolation = @@transaction_isolation;
SET GLOBAL transaction_isolation = 'READ-COMMITTED';
SET @old_innodb_lock_wait_timeout = @@innodb_lock_wait_timeout;
SET GLOBAL innodb_lock_wait_timeout = 1;
INSERT INTO t1 VALUES (1,1,1),(2,2,2);
DELETE FROM t1;
INSERT INTO t1 VALUES (5,5,5),(7,7,7);
BEGIN;
INSERT INTO t1 VALUES (1,4,7) ON DUPLICATE KEY UPDATE b=6;
SELECT MAX(engine_transaction_id) INTO @con1_trx_id FROM performance_schema.data_locks WHERE object_schema='test' AND object_name = 't1';
# LOCKS AFTER IODKU
SELECT IF(engine_transaction_id=@con1_trx_id, "con1", "other") as con,
index_name,lock_type,lock_mode,lock_status,lock_data
FROM performance_schema.data_locks
WHERE object_schema='test' AND object_name = 't1'
  ORDER BY 1,2,3,4,5,6;
con	index_name	lock_type	lock_mode	lock_status	lock_data
con1	NULL	TABLE	IX	GRANTED	NULL
con1	a	RECORD	X	GRANTED	1, 1
con1	a	RECORD	X	GRANTED	2, 2
con1	a	RECORD	X	GRANTED	supremum pseudo-record
con1	a	RECORD	X,GAP	GRANTED	2, 2
con1	a	RECORD	X,GAP	GRANTED	7, 6
con1	a	RECORD	X,GAP	GRANTED	7, 7
con1	a	RECORD	X,REC_NOT_GAP	GRANTED	7, 7
con1	c	RECORD	X	GRANTED	7, 7
con1	c	RECORD	X	GRANTED	supremum pseudo-record
con1	c	RECORD	X,GAP	GRANTED	7, 6
con1	PRIMARY	RECORD	X,REC_NOT_GAP	GRANTED	7
SET debug_sync = 'row_ins_sec_index_entry_dup_locks_created SIGNAL
  con1_locks_done WAIT_FOR con1_go';
INSERT INTO t1 VALUES (1,2,3);COMMIT;
SET debug_sync = 'now WAIT_FOR con1_locks_done';
# LOCKS AFTER con1_locks_done
SELECT IF(engine_transaction_id=@con1_trx_id, "con1", "other") as con,
index_name,lock_type,lock_mode,lock_status,lock_data
FROM performance_schema.data_locks
WHERE object_schema='test' AND object_name = 't1'
  ORDER BY 1,2,3,4,5,6;
con	index_name	lock_type	lock_mode	lock_status	lock_data
con1	NULL	TABLE	IX	GRANTED	NULL
con1	a	RECORD	X	GRANTED	1, 1
con1	a	RECORD	X	GRANTED	2, 2
con1	a	RECORD	X	GRANTED	supremum pseudo-record
con1	a	RECORD	X,GAP	GRANTED	2, 2
con1	a	RECORD	X,GAP	GRANTED	7, 6
con1	a	RECORD	X,GAP	GRANTED	7, 7
con1	a	RECORD	X,REC_NOT_GAP	GRANTED	7, 7
con1	c	RECORD	X	GRANTED	7, 7
con1	c	RECORD	X	GRANTED	supremum pseudo-record
con1	c	RECORD	X,GAP	GRANTED	7, 6
con1	PRIMARY	RECORD	S,REC_NOT_GAP	GRANTED	2
con1	PRIMARY	RECORD	X,REC_NOT_GAP	GRANTED	7
SELECT COUNT(*) FROM performance_schema.data_locks
WHERE object_schema='test'
        AND object_name='t1'
        AND index_name='a'
        AND lock_type='RECORD'
        AND lock_data='5, 5';
COUNT(*)
0
SET GLOBAL innodb_purge_run_now=ON;
# LOCKS AFTER purge
SELECT IF(engine_transaction_id=@con1_trx_id, "con1", "other") as con,
index_name,lock_type,lock_mode,lock_status,lock_data
FROM performance_schema.data_locks
WHERE object_schema='test' AND object_name = 't1'
  ORDER BY 1,2,3,4,5,6;
con	index_name	lock_type	lock_mode	lock_status	lock_data
con1	NULL	TABLE	IX	GRANTED	NULL
con1	a	RECORD	X	GRANTED	supremum pseudo-record
con1	a	RECORD	X,GAP	GRANTED	5, 5
con1	a	RECORD	X,GAP	GRANTED	7, 6
con1	a	RECORD	X,GAP	GRANTED	7, 7
con1	a	RECORD	X,REC_NOT_GAP	GRANTED	7, 7
con1	c	RECORD	X	GRANTED	7, 7
con1	c	RECORD	X	GRANTED	supremum pseudo-record
con1	c	RECORD	X,GAP	GRANTED	7, 6
con1	PRIMARY	RECORD	S,REC_NOT_GAP	GRANTED	2
con1	PRIMARY	RECORD	X,REC_NOT_GAP	GRANTED	7
INSERT t1 VALUES (1,10,4);
# LOCKS AFTER con2
SELECT IF(engine_transaction_id=@con1_trx_id, "con1", "other") as con,
index_name,lock_type,lock_mode,lock_status,lock_data
FROM performance_schema.data_locks
WHERE object_schema='test' AND object_name = 't1'
  ORDER BY 1,2,3,4,5,6;
con	index_name	lock_type	lock_mode	lock_status	lock_data
con1	NULL	TABLE	IX	GRANTED	NULL
con1	a	RECORD	X	GRANTED	supremum pseudo-record
con1	a	RECORD	X,GAP	GRANTED	5, 5
con1	a	RECORD	X,GAP	GRANTED	7, 6
con1	a	RECORD	X,GAP	GRANTED	7, 7
con1	a	RECORD	X,REC_NOT_GAP	GRANTED	7, 7
con1	c	RECORD	X	GRANTED	7, 7
con1	c	RECORD	X	GRANTED	supremum pseudo-record
con1	c	RECORD	X,GAP	GRANTED	7, 6
con1	PRIMARY	RECORD	S,REC_NOT_GAP	GRANTED	2
con1	PRIMARY	RECORD	X,REC_NOT_GAP	GRANTED	7
SET debug_sync = 'now SIGNAL con1_go';
SELECT * FROM t1;
a	b	c
1	2	3
5	5	5
7	6	7
CHECK TABLE t1;
Table	Op	Msg_type	Msg_text
test.t1	check	status	OK
DROP TABLE t1;
SET GLOBAL innodb_stats_auto_recalc = @old_innodb_stats_auto_recalc;
SET GLOBAL transaction_isolation = @old_transaction_isolation;
SET GLOBAL innodb_lock_wait_timeout = @old_innodb_lock_wait_timeout;
SET DEBUG_SYNC= 'RESET';
