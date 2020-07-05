SET @innodb_lock_wait_timeout_saved = @@global.innodb_lock_wait_timeout;
SET @@global.innodb_lock_wait_timeout = 20;
CREATE TABLE t0 (
id INT PRIMARY KEY,
trx_mysql_thread_id BIGINT
) ENGINE=InnoDB;
CREATE PROCEDURE show_locks ()
BEGIN
SELECT
t0.id, object_name, index_name, lock_type, lock_mode, lock_status, lock_data
FROM t0
JOIN INFORMATION_SCHEMA.INNODB_TRX USING (trx_mysql_thread_id)
JOIN performance_schema.data_locks ON (trx_id=engine_transaction_id)
ORDER BY 1,2,3,4,5,6,7;
END //
CREATE TABLE t1 (id INT NOT NULL PRIMARY KEY, val CHAR(200));
INSERT INTO t1 (id,val) VALUES (1, "a"),  (3, "c"), (1998,"x"), (2000, "z");
INSERT INTO t0 VALUES (1, CONNECTION_ID());
BEGIN;
SELECT * FROM t1 WHERE id<=3 FOR SHARE;
id	val
1	a
3	c
INSERT INTO t0 VALUES (2, CONNECTION_ID());
BEGIN;
SET DEBUG_SYNC='lock_wait_will_wait SIGNAL con2_will_wait';
INSERT INTO t1 VALUES (2,"b");
SET DEBUG_SYNC='now WAIT_FOR con2_will_wait';
INSERT INTO t0 VALUES (3, CONNECTION_ID());
BEGIN;
SELECT * FROM t1 WHERE id=2000 FOR UPDATE;
id	val
2000	z
INSERT INTO t0 VALUES (4, CONNECTION_ID());
BEGIN;
SET DEBUG_SYNC='lock_wait_will_wait SIGNAL con4_will_wait';
SELECT * FROM t1 WHERE 1999<=id FOR SHARE;
SET DEBUG_SYNC='now WAIT_FOR con4_will_wait';
CALL show_locks();
id	object_name	index_name	lock_type	lock_mode	lock_status	lock_data
1	t1	NULL	TABLE	IS	GRANTED	NULL
1	t1	PRIMARY	RECORD	S	GRANTED	1
1	t1	PRIMARY	RECORD	S	GRANTED	3
2	t1	NULL	TABLE	IX	GRANTED	NULL
2	t1	PRIMARY	RECORD	X,GAP,INSERT_INTENTION	WAITING	3
3	t1	NULL	TABLE	IX	GRANTED	NULL
3	t1	PRIMARY	RECORD	X,REC_NOT_GAP	GRANTED	2000
4	t1	NULL	TABLE	IS	GRANTED	NULL
4	t1	PRIMARY	RECORD	S	WAITING	2000
INSERT INTO t1 (id,val) VALUES (10,"x");
INSERT INTO t1 (id,val) VALUES (11,"x");
INSERT INTO t1 (id,val) VALUES (12,"x");
INSERT INTO t1 (id,val) VALUES (13,"x");
INSERT INTO t1 (id,val) VALUES (14,"x");
INSERT INTO t1 (id,val) VALUES (15,"x");
INSERT INTO t1 (id,val) VALUES (16,"x");
INSERT INTO t1 (id,val) VALUES (17,"x");
INSERT INTO t1 (id,val) VALUES (18,"x");
INSERT INTO t1 (id,val) VALUES (19,"x");
INSERT INTO t1 (id,val) VALUES (20,"x");
INSERT INTO t1 (id,val) VALUES (21,"x");
INSERT INTO t1 (id,val) VALUES (22,"x");
INSERT INTO t1 (id,val) VALUES (23,"x");
INSERT INTO t1 (id,val) VALUES (24,"x");
INSERT INTO t1 (id,val) VALUES (25,"x");
INSERT INTO t1 (id,val) VALUES (26,"x");
INSERT INTO t1 (id,val) VALUES (27,"x");
INSERT INTO t1 (id,val) VALUES (28,"x");
INSERT INTO t1 (id,val) VALUES (29,"x");
INSERT INTO t1 (id,val) VALUES (30,"x");
INSERT INTO t1 (id,val) VALUES (31,"x");
INSERT INTO t1 (id,val) VALUES (32,"x");
INSERT INTO t1 (id,val) VALUES (33,"x");
INSERT INTO t1 (id,val) VALUES (34,"x");
INSERT INTO t1 (id,val) VALUES (35,"x");
INSERT INTO t1 (id,val) VALUES (36,"x");
INSERT INTO t1 (id,val) VALUES (37,"x");
INSERT INTO t1 (id,val) VALUES (38,"x");
INSERT INTO t1 (id,val) VALUES (39,"x");
INSERT INTO t1 (id,val) VALUES (40,"x");
INSERT INTO t1 (id,val) VALUES (41,"x");
INSERT INTO t1 (id,val) VALUES (42,"x");
INSERT INTO t1 (id,val) VALUES (43,"x");
INSERT INTO t1 (id,val) VALUES (44,"x");
INSERT INTO t1 (id,val) VALUES (45,"x");
INSERT INTO t1 (id,val) VALUES (46,"x");
INSERT INTO t1 (id,val) VALUES (47,"x");
INSERT INTO t1 (id,val) VALUES (48,"x");
INSERT INTO t1 (id,val) VALUES (49,"x");
INSERT INTO t1 (id,val) VALUES (50,"x");
INSERT INTO t1 (id,val) VALUES (51,"x");
INSERT INTO t1 (id,val) VALUES (52,"x");
INSERT INTO t1 (id,val) VALUES (53,"x");
INSERT INTO t1 (id,val) VALUES (54,"x");
INSERT INTO t1 (id,val) VALUES (55,"x");
INSERT INTO t1 (id,val) VALUES (56,"x");
INSERT INTO t1 (id,val) VALUES (57,"x");
INSERT INTO t1 (id,val) VALUES (58,"x");
INSERT INTO t1 (id,val) VALUES (59,"x");
INSERT INTO t1 (id,val) VALUES (60,"x");
INSERT INTO t1 (id,val) VALUES (61,"x");
INSERT INTO t1 (id,val) VALUES (62,"x");
INSERT INTO t1 (id,val) VALUES (63,"x");
INSERT INTO t1 (id,val) VALUES (64,"x");
INSERT INTO t1 (id,val) VALUES (65,"x");
INSERT INTO t1 (id,val) VALUES (66,"x");
INSERT INTO t1 (id,val) VALUES (67,"x");
INSERT INTO t1 (id,val) VALUES (68,"x");
INSERT INTO t1 (id,val) VALUES (69,"x");
INSERT INTO t1 (id,val) VALUES (70,"x");
INSERT INTO t1 (id,val) VALUES (71,"x");
INSERT INTO t1 (id,val) VALUES (72,"x");
INSERT INTO t1 (id,val) VALUES (73,"x");
INSERT INTO t1 (id,val) VALUES (74,"x");
INSERT INTO t1 (id,val) VALUES (75,"x");
INSERT INTO t1 (id,val) VALUES (76,"x");
INSERT INTO t1 (id,val) VALUES (77,"x");
INSERT INTO t1 (id,val) VALUES (78,"x");
INSERT INTO t1 (id,val) VALUES (79,"x");
INSERT INTO t1 (id,val) VALUES (80,"x");
INSERT INTO t1 (id,val) VALUES (81,"x");
INSERT INTO t1 (id,val) VALUES (82,"x");
INSERT INTO t1 (id,val) VALUES (83,"x");
INSERT INTO t1 (id,val) VALUES (84,"x");
INSERT INTO t1 (id,val) VALUES (85,"x");
INSERT INTO t1 (id,val) VALUES (86,"x");
INSERT INTO t1 (id,val) VALUES (87,"x");
INSERT INTO t1 (id,val) VALUES (88,"x");
INSERT INTO t1 (id,val) VALUES (89,"x");
INSERT INTO t1 (id,val) VALUES (90,"x");
INSERT INTO t1 (id,val) VALUES (91,"x");
INSERT INTO t1 (id,val) VALUES (92,"x");
INSERT INTO t1 (id,val) VALUES (93,"x");
INSERT INTO t1 (id,val) VALUES (94,"x");
INSERT INTO t1 (id,val) VALUES (95,"x");
INSERT INTO t1 (id,val) VALUES (96,"x");
INSERT INTO t1 (id,val) VALUES (97,"x");
INSERT INTO t1 (id,val) VALUES (98,"x");
INSERT INTO t1 (id,val) VALUES (99,"x");
CALL show_locks();
id	object_name	index_name	lock_type	lock_mode	lock_status	lock_data
1	t1	NULL	TABLE	IS	GRANTED	NULL
1	t1	PRIMARY	RECORD	S	GRANTED	1
1	t1	PRIMARY	RECORD	S	GRANTED	3
2	t1	NULL	TABLE	IX	GRANTED	NULL
2	t1	PRIMARY	RECORD	X,GAP,INSERT_INTENTION	WAITING	3
3	t1	NULL	TABLE	IX	GRANTED	NULL
3	t1	PRIMARY	RECORD	X,REC_NOT_GAP	GRANTED	2000
4	t1	NULL	TABLE	IS	GRANTED	NULL
4	t1	PRIMARY	RECORD	S	WAITING	2000
COMMIT;
id	val
2000	z
SELECT * FROM t1 WHERE id<=3 FOR SHARE;
id	val
1	a
3	c
CALL show_locks();
id	object_name	index_name	lock_type	lock_mode	lock_status	lock_data
1	t1	NULL	TABLE	IS	GRANTED	NULL
1	t1	PRIMARY	RECORD	S	GRANTED	1
1	t1	PRIMARY	RECORD	S	GRANTED	3
2	t1	NULL	TABLE	IX	GRANTED	NULL
2	t1	PRIMARY	RECORD	X,GAP,INSERT_INTENTION	WAITING	3
4	t1	NULL	TABLE	IS	GRANTED	NULL
4	t1	PRIMARY	RECORD	S	GRANTED	1
4	t1	PRIMARY	RECORD	S	GRANTED	2000
4	t1	PRIMARY	RECORD	S	GRANTED	3
4	t1	PRIMARY	RECORD	S	GRANTED	supremum pseudo-record
COMMIT;
COMMIT;
COMMIT;
DROP TABLE t1;
DROP PROCEDURE show_locks;
DROP TABLE t0;
SET @@global.innodb_lock_wait_timeout = @innodb_lock_wait_timeout_saved ;
