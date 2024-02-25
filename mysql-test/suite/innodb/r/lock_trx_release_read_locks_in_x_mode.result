CREATE TABLE t0 (id INT PRIMARY KEY) ENGINE=InnoDB;
CREATE TABLE t1 (id INT PRIMARY KEY) ENGINE=InnoDB;
CREATE TABLE t2 (id INT PRIMARY KEY) ENGINE=InnoDB;
CREATE TABLE t3 (id INT PRIMARY KEY) ENGINE=InnoDB;
CREATE TABLE t4 (id INT PRIMARY KEY) ENGINE=InnoDB;
CREATE TABLE t5 (id INT PRIMARY KEY) ENGINE=InnoDB;
INSERT INTO t0 (id) VALUES (1);
SET TRANSACTION ISOLATION LEVEL READ COMMITTED;
XA START 'x';
INSERT INTO t1 (id) VALUES (1);;
INSERT INTO t2 (id) VALUES (1);;
INSERT INTO t3 (id) VALUES (1);;
INSERT INTO t4 (id) VALUES (1);;
INSERT INTO t5 (id) VALUES (1);;
SELECT * FROM t0 WHERE id=1 FOR UPDATE;
id
1
XA END 'x';
SET DEBUG_SYNC='lock_trx_release_read_locks_in_x_mode_will_release
    SIGNAL c0_releases_in_xmode';
SET DEBUG_SYNC='try_relatch_trx_and_shard_and_do_noted_expected_version
    SIGNAL c0_noted_expected_version
    WAIT_FOR c0_can_go
    EXECUTE 5';
XA PREPARE 'x';
BEGIN;
SET DEBUG_SYNC = 'now WAIT_FOR c0_noted_expected_version';
SET DEBUG_SYNC='lock_wait_will_wait SIGNAL c0_can_go';
SELECT * FROM t1 FOR SHARE;
BEGIN;
SET DEBUG_SYNC = 'now WAIT_FOR c0_noted_expected_version';
SET DEBUG_SYNC='lock_wait_will_wait SIGNAL c0_can_go';
SELECT * FROM t2 FOR SHARE;
BEGIN;
SET DEBUG_SYNC = 'now WAIT_FOR c0_noted_expected_version';
SET DEBUG_SYNC='lock_wait_will_wait SIGNAL c0_can_go';
SELECT * FROM t3 FOR SHARE;
BEGIN;
SET DEBUG_SYNC = 'now WAIT_FOR c0_noted_expected_version';
SET DEBUG_SYNC='lock_wait_will_wait SIGNAL c0_can_go';
SELECT * FROM t4 FOR SHARE;
BEGIN;
SET DEBUG_SYNC = 'now WAIT_FOR c0_noted_expected_version';
SET DEBUG_SYNC='lock_wait_will_wait SIGNAL c0_can_go';
SELECT * FROM t5 FOR SHARE;
SET DEBUG_SYNC='now WAIT_FOR c0_releases_in_xmode';
XA COMMIT 'x';
id
1
COMMIT;
id
1
COMMIT;
id
1
COMMIT;
id
1
COMMIT;
id
1
COMMIT;
DROP TABLE t0;
DROP TABLE t1;
DROP TABLE t2;
DROP TABLE t3;
DROP TABLE t4;
DROP TABLE t5;
