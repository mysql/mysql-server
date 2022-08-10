CREATE TABLE t1(id INT PRIMARY KEY) ENGINE=InnoDB;
INSERT INTO t1 (id) VALUES (1),(3);
BEGIN;
INSERT INTO t1 (id) VALUES (2);
BEGIN;
SET DEBUG_SYNC = "before_lock_rec_convert_impl_to_expl_for_trx
        SIGNAL c2_decided
        WAIT_FOR c2_can_convert";
SET DEBUG_SYNC = "lock_wait_will_wait SIGNAL c2_will_wait";
SELECT * FROM t1 WHERE id=2 FOR UPDATE;
SET DEBUG_SYNC = "now WAIT_FOR c2_decided";
SET DEBUG_SYNC = "before_lock_trx_release_locks
        SIGNAL c1_will_commit
        WAIT_FOR c1_can_commit";
COMMIT;
BEGIN;
SET DEBUG_SYNC = "now WAIT_FOR c1_will_commit";
SELECT * FROM t1 WHERE id=2 FOR UPDATE;
id
2
SET DEBUG_SYNC = "now SIGNAL c2_can_convert";
SET DEBUG_SYNC = "now WAIT_FOR c2_will_wait";
SET DEBUG_SYNC = "now SIGNAL c1_can_commit";
COMMIT;
id
2
COMMIT;
DROP TABLE t1;
CREATE TABLE t1(id INT PRIMARY KEY) ENGINE=InnoDB;
INSERT INTO t1 (id) VALUES (1),(3);
XA START "xid0";
INSERT INTO t1 (id) VALUES (2);
BEGIN;
SET DEBUG_SYNC = "before_lock_rec_convert_impl_to_expl_for_trx
        SIGNAL c2_decided
        WAIT_FOR c2_can_convert";
SET DEBUG_SYNC = "lock_wait_will_wait SIGNAL c2_will_wait";
SELECT * FROM t1 WHERE id=2 FOR UPDATE;
SET DEBUG_SYNC = "now WAIT_FOR c2_decided";
SET DEBUG_SYNC = "before_lock_trx_release_locks
        SIGNAL c1_will_commit
        WAIT_FOR c1_can_commit";
XA END "xid0";
XA PREPARE "xid0";
XA COMMIT "xid0";
BEGIN;
SET DEBUG_SYNC = "now WAIT_FOR c1_will_commit";
SELECT * FROM t1 WHERE id=2 FOR UPDATE;
id
2
SET DEBUG_SYNC = "now SIGNAL c2_can_convert";
SET DEBUG_SYNC = "now WAIT_FOR c2_will_wait";
SET DEBUG_SYNC = "now SIGNAL c1_can_commit";
COMMIT;
id
2
COMMIT;
DROP TABLE t1;
CREATE TABLE t1(id INT PRIMARY KEY) ENGINE=InnoDB;
INSERT INTO t1 (id) VALUES (1),(3);
BEGIN;
INSERT INTO t1 (id) VALUES (2);
BEGIN;
SET DEBUG_SYNC = "before_lock_rec_convert_impl_to_expl_for_trx
        SIGNAL c2_decided
        WAIT_FOR c2_can_convert";
SET DEBUG_SYNC = "after_lock_rec_convert_impl_to_expl_for_trx
        SIGNAL c2_done_conversion";
SET DEBUG_SYNC = "after_lock_clust_rec_read_check_and_lock
        SIGNAL c2_done_locking";
SELECT * FROM t1 WHERE id=2 FOR UPDATE;
SET DEBUG_SYNC = "now WAIT_FOR c2_decided";
SET DEBUG_SYNC = "before_lock_trx_release_locks
        SIGNAL c1_will_commit
        WAIT_FOR c1_can_commit";
COMMIT;
BEGIN;
SET DEBUG_SYNC = "now WAIT_FOR c1_will_commit";
SET DEBUG_SYNC = "lock_wait_will_wait SIGNAL c3_will_wait";
SET DEBUG_SYNC = "after_lock_clust_rec_read_check_and_lock_impl_to_expl
        SIGNAL c3_will_lock
        WAIT_FOR c3_can_lock";
SELECT * FROM t1 WHERE id=2 FOR UPDATE;
SET DEBUG_SYNC = "now WAIT_FOR c3_will_lock";
SET DEBUG_SYNC = "now SIGNAL c2_can_convert";
SET DEBUG_SYNC = "now WAIT_FOR c2_done_conversion";
SET DEBUG_SYNC = "now WAIT_FOR c2_done_locking";
SET DEBUG_SYNC = "now SIGNAL c3_can_lock";
SET DEBUG_SYNC = "now WAIT_FOR c3_will_wait";
SET DEBUG_SYNC = "now SIGNAL c1_can_commit";
COMMIT;
id
2
COMMIT;
id
2
COMMIT;
DROP TABLE t1;
CREATE TABLE t1(id INT PRIMARY KEY) ENGINE=InnoDB;
INSERT INTO t1 (id) VALUES (1),(3);
XA START "xid0";
INSERT INTO t1 (id) VALUES (2);
BEGIN;
SET DEBUG_SYNC = "before_lock_rec_convert_impl_to_expl_for_trx
        SIGNAL c2_decided
        WAIT_FOR c2_can_convert";
SET DEBUG_SYNC = "after_lock_rec_convert_impl_to_expl_for_trx
        SIGNAL c2_done_conversion";
SET DEBUG_SYNC = "after_lock_clust_rec_read_check_and_lock
        SIGNAL c2_done_locking";
SELECT * FROM t1 WHERE id=2 FOR UPDATE;
SET DEBUG_SYNC = "now WAIT_FOR c2_decided";
SET DEBUG_SYNC = "before_lock_trx_release_locks
        SIGNAL c1_will_commit
        WAIT_FOR c1_can_commit";
XA END "xid0";
XA PREPARE "xid0";
XA COMMIT "xid0";
BEGIN;
SET DEBUG_SYNC = "now WAIT_FOR c1_will_commit";
SET DEBUG_SYNC = "lock_wait_will_wait SIGNAL c3_will_wait";
SET DEBUG_SYNC = "after_lock_clust_rec_read_check_and_lock_impl_to_expl
        SIGNAL c3_will_lock
        WAIT_FOR c3_can_lock";
SELECT * FROM t1 WHERE id=2 FOR UPDATE;
SET DEBUG_SYNC = "now WAIT_FOR c3_will_lock";
SET DEBUG_SYNC = "now SIGNAL c2_can_convert";
SET DEBUG_SYNC = "now WAIT_FOR c2_done_conversion";
SET DEBUG_SYNC = "now WAIT_FOR c2_done_locking";
SET DEBUG_SYNC = "now SIGNAL c3_can_lock";
SET DEBUG_SYNC = "now WAIT_FOR c3_will_wait";
SET DEBUG_SYNC = "now SIGNAL c1_can_commit";
COMMIT;
id
2
COMMIT;
id
2
COMMIT;
DROP TABLE t1;
CREATE TABLE t1(id INT PRIMARY KEY,val INT, KEY val(val)) ENGINE=InnoDB;
INSERT INTO t1 (id,val) VALUES (1,1),(3,3);
BEGIN;
INSERT INTO t1 (id,val) VALUES (2,2);
BEGIN;
SET DEBUG_SYNC = "lock_rec_convert_impl_to_expl_will_validate
        SIGNAL c2_decided
        WAIT_FOR c2_can_validate";
SET DEBUG_SYNC = "lock_wait_will_wait SIGNAL c2_will_wait";
SELECT * FROM t1 WHERE val=2 FOR UPDATE;
SET DEBUG_SYNC = "now WAIT_FOR c2_decided";
SET DEBUG_SYNC = "before_lock_trx_release_locks
        SIGNAL c1_will_commit
        WAIT_FOR c1_can_commit";
COMMIT;
BEGIN;
SET DEBUG_SYNC = "now WAIT_FOR c1_will_commit";
SELECT * FROM t1 WHERE val=2 FOR UPDATE;
id	val
2	2
SET DEBUG_SYNC = "now SIGNAL c2_can_validate";
SET DEBUG_SYNC = "now WAIT_FOR c2_will_wait";
SET DEBUG_SYNC = "now SIGNAL c1_can_commit";
COMMIT;
id	val
2	2
COMMIT;
DROP TABLE t1;
CREATE TABLE t1(id INT PRIMARY KEY) ENGINE=InnoDB;
INSERT INTO t1 (id) VALUES (1),(3);
BEGIN;
INSERT INTO t1 (id) VALUES (2);
BEGIN;
INSERT INTO t1 (id) VALUES (4);
BEGIN;
SET DEBUG_SYNC = "before_lock_rec_convert_impl_to_expl_for_trx
        SIGNAL c2_decided
        WAIT_FOR c2_can_convert";
SET DEBUG_SYNC = "after_lock_clust_rec_read_check_and_lock SIGNAL c2_will_wait";
SELECT * FROM t1 WHERE id=2 FOR UPDATE;
SET DEBUG_SYNC = "now WAIT_FOR c2_decided";
SET DEBUG_SYNC = "after_trx_erase_lists
        SIGNAL c1_will_commit
        WAIT_FOR c1_can_commit";
COMMIT;
SET DEBUG_SYNC = "now WAIT_FOR c1_will_commit";
SET DEBUG_SYNC = "after_lock_clust_rec_read_check_and_lock SIGNAL c2_can_convert WAIT_FOR c3_can_verify";
SELECT * FROM t1 WHERE id=2 FOR UPDATE;;
SET DEBUG_SYNC = "now WAIT_FOR c2_will_wait";
SET DEBUG_SYNC = "now SIGNAL c1_can_commit";
SET DEBUG_SYNC = "now SIGNAL c3_can_verify";
id
2
COMMIT;
id
2
COMMIT;
DROP TABLE t1;
