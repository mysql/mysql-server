CREATE TABLE t1 (id INT PRIMARY KEY);
INSERT INTO t1 (id) VALUES (1),(2),  (4),(5);
ANALYZE TABLE t1;
Table	Op	Msg_type	Msg_text
test.t1	analyze	status	OK
BEGIN;
DELETE FROM t1 WHERE id=2;
BEGIN;
SELECT * FROM t1 FORCE INDEX (PRIMARY) WHERE id=3 FOR SHARE;
id
SET DEBUG_SYNC = 'lock_wait_will_wait SIGNAL c2_will_wait WAIT_FOR c2_can_go';
SELECT * FROM t1 FORCE INDEX (PRIMARY) WHERE id=2 FOR SHARE;
SET DEBUG_SYNC = 'now WAIT_FOR c2_will_wait';
SELECT trx_id
FROM information_schema.innodb_trx
WHERE trx_query =
'SELECT * FROM t1 FORCE INDEX (PRIMARY) WHERE id=2 FOR SHARE'
    INTO @c2_trx_id;
COMMIT;
SET DEBUG_SYNC = 'now SIGNAL c2_can_go';
DROP TABLE t1;
