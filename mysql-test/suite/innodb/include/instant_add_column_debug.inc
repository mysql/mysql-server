
connect (conn1, localhost, root,,);

connection default;

--eval CREATE TABLE t1 (a INT NOT NULL PRIMARY KEY, b INT, c INT) ROW_FORMAT=$row_format;

INSERT INTO t1 VALUES(1, 1, 1), (2, 2, 2), (3, 3, 3), (6, 6, 6);

ALTER TABLE t1 ADD COLUMN d INT DEFAULT 10;

SELECT * FROM t1;

SET DEBUG_SYNC = 'row_log_table_apply1_before SIGNAL altered WAIT_FOR dmls_done';

--send ALTER TABLE t1 DROP COLUMN c;

connection conn1;

SET DEBUG_SYNC = 'now WAIT_FOR altered';

INSERT INTO t1(a, b, c) VALUES(7, 7, 7);

UPDATE t1 SET a = 10 WHERE a = 1;

DELETE FROM t1 WHERE a = 2;

UPDATE t1 SET c = 10 WHERE a = 3;

UPDATE t1 SET d = 20 WHERE a = 5;

UPDATE t1 SET a = 8, d = 20 WHERE a = 6;

SET DEBUG_SYNC = 'now SIGNAL dmls_done';

disconnect conn1;

connection default;

reap;

SET DEBUG_SYNC = 'RESET';

CHECK TABLE t1;

SHOW CREATE TABLE t1;

SELECT * FROM t1;

DROP TABLE t1;
