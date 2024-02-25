SET DEBUG_SYNC='reset';
CREATE TABLE t1 (
id INT UNIQUE NOT NULL,
a INT,
b BLOB,
ag INT GENERATED ALWAYS AS (a) VIRTUAL,
PRIMARY KEY(id),
INDEX bugidx (b(100),a)
) ENGINE=InnoDB;
INSERT INTO t1 (id, a, b) VALUES (1001, 1, REPEAT('a', 50001));
INSERT INTO t1 (id, a, b) VALUES (1002, 2, REPEAT('b', 50002));
SET GLOBAL innodb_purge_stop_now = ON;
DELETE FROM t1 WHERE id = 1001;
SET DEBUG_SYNC='before_row_ins_upd_pessimistic SIGNAL s1 WAIT_FOR s2 TIMEOUT 10';
INSERT INTO t1 (id, a, b) VALUES (1001, 1, REPEAT('z', 60001));
SET DEBUG_SYNC='now WAIT_FOR s1';
SET GLOBAL innodb_purge_run_now = ON;
SET DEBUG_SYNC='now SIGNAL s2';
SET DEBUG_SYNC='reset';
SET DEBUG_SYNC='reset';
drop table t1;
CREATE TABLE t1 (
id INT UNIQUE NOT NULL,
a INT,
b BLOB,
ag INT GENERATED ALWAYS AS (a) VIRTUAL,
PRIMARY KEY(id),
INDEX bugidx (b(100),ag)
) ENGINE=InnoDB;
INSERT INTO t1 (id, a, b) VALUES (1001, 1, REPEAT('a', 50001));
INSERT INTO t1 (id, a, b) VALUES (1002, 2, REPEAT('b', 50002));
SET GLOBAL innodb_purge_stop_now = ON;
DELETE FROM t1 WHERE id = 1001;
SET DEBUG_SYNC='before_row_ins_upd_pessimistic SIGNAL s1 WAIT_FOR s2 TIMEOUT 10';
INSERT INTO t1 (id, a, b) VALUES (1001, 1, REPEAT('z', 60001));
SET DEBUG_SYNC='now WAIT_FOR s1';
SET GLOBAL innodb_purge_run_now = ON;
SET DEBUG_SYNC='now SIGNAL s2';
SET DEBUG_SYNC='reset';
SET DEBUG_SYNC='reset';
drop table t1;
