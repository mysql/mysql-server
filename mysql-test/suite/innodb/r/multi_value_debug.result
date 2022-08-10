CREATE TABLE t1 (id INT NOT NULL KEY AUTO_INCREMENT, f1 JSON);
CREATE INDEX i1 ON t1((CAST(f1->"$[*]" AS DECIMAL(4,2) ARRAY)));
CREATE INDEX i2 ON t1((CAST(f1->"$[*]" AS CHAR(10) ARRAY)));
set debug = '+d, row_ins_sec_index_entry_lock_wait';
INSERT INTO t1(f1) VALUES(CAST('["11.11", "22.22", "33.33", "44.44", "55.55", "66.66", "77.77", "88.88", "99.99"]' AS JSON));
SELECT * FROM t1;
id	f1
1	["11.11", "22.22", "33.33", "44.44", "55.55", "66.66", "77.77", "88.88", "99.99"]
SELECT * FROM t1 WHERE 11.11 MEMBER OF (f1->'$[*]');
id	f1
1	["11.11", "22.22", "33.33", "44.44", "55.55", "66.66", "77.77", "88.88", "99.99"]
SELECT * FROM t1 WHERE 22.22 MEMBER OF (f1->'$[*]');
id	f1
1	["11.11", "22.22", "33.33", "44.44", "55.55", "66.66", "77.77", "88.88", "99.99"]
SELECT * FROM t1 WHERE 33.33 MEMBER OF (f1->'$[*]');
id	f1
1	["11.11", "22.22", "33.33", "44.44", "55.55", "66.66", "77.77", "88.88", "99.99"]
SELECT * FROM t1 WHERE 44.44 MEMBER OF (f1->'$[*]');
id	f1
1	["11.11", "22.22", "33.33", "44.44", "55.55", "66.66", "77.77", "88.88", "99.99"]
SELECT * FROM t1 WHERE 55.55 MEMBER OF (f1->'$[*]');
id	f1
1	["11.11", "22.22", "33.33", "44.44", "55.55", "66.66", "77.77", "88.88", "99.99"]
SELECT * FROM t1 WHERE 66.66 MEMBER OF (f1->'$[*]');
id	f1
1	["11.11", "22.22", "33.33", "44.44", "55.55", "66.66", "77.77", "88.88", "99.99"]
SELECT * FROM t1 WHERE 77.77 MEMBER OF (f1->'$[*]');
id	f1
1	["11.11", "22.22", "33.33", "44.44", "55.55", "66.66", "77.77", "88.88", "99.99"]
SELECT * FROM t1 WHERE 88.88 MEMBER OF (f1->'$[*]');
id	f1
1	["11.11", "22.22", "33.33", "44.44", "55.55", "66.66", "77.77", "88.88", "99.99"]
SELECT * FROM t1 WHERE 99.99 MEMBER OF (f1->'$[*]');
id	f1
1	["11.11", "22.22", "33.33", "44.44", "55.55", "66.66", "77.77", "88.88", "99.99"]
TRUNCATE TABLE t1;
INSERT INTO t1(f1) VALUES(CAST('["13.33","13.21"]' AS JSON));
INSERT INTO t1(f1) VALUES(CAST('["15.67","18.55"]' AS JSON));
SELECT * FROM t1 WHERE 13.33 MEMBER OF (f1->'$[*]');
id	f1
1	["13.33", "13.21"]
SELECT * FROM t1 WHERE 13.21 MEMBER OF (f1->'$[*]');
id	f1
1	["13.33", "13.21"]
SELECT * FROM t1 WHERE 15.67 MEMBER OF (f1->'$[*]');
id	f1
2	["15.67", "18.55"]
SELECT * FROM t1 WHERE 18.55 MEMBER OF (f1->'$[*]');
id	f1
2	["15.67", "18.55"]
SELECT * FROM t1 WHERE "13.33" MEMBER OF (f1->'$[*]');
id	f1
1	["13.33", "13.21"]
SELECT * FROM t1 WHERE "13.21" MEMBER OF (f1->'$[*]');
id	f1
1	["13.33", "13.21"]
SELECT * FROM t1 WHERE "15.67" MEMBER OF (f1->'$[*]');
id	f1
2	["15.67", "18.55"]
SELECT * FROM t1 WHERE "18.55" MEMBER OF (f1->'$[*]');
id	f1
2	["15.67", "18.55"]
set debug = '+d, row_upd_sec_index_entry_lock_wait';
DELETE FROM t1;
SELECT * FROM t1 WHERE 13.33 MEMBER OF (f1->'$[*]');
id	f1
SELECT * FROM t1 WHERE 13.21 MEMBER OF (f1->'$[*]');
id	f1
SELECT * FROM t1 WHERE 15.67 MEMBER OF (f1->'$[*]');
id	f1
SELECT * FROM t1 WHERE 18.55 MEMBER OF (f1->'$[*]');
id	f1
SELECT * FROM t1 WHERE "13.33" MEMBER OF (f1->'$[*]');
id	f1
SELECT * FROM t1 WHERE "13.21" MEMBER OF (f1->'$[*]');
id	f1
SELECT * FROM t1 WHERE "15.67" MEMBER OF (f1->'$[*]');
id	f1
SELECT * FROM t1 WHERE "18.55" MEMBER OF (f1->'$[*]');
id	f1
INSERT INTO t1(f1) VALUES(CAST('["13.33","13.21","13.45"]' AS JSON));
INSERT INTO t1(f1) VALUES(CAST('["15.67","18.55","19.32"]' AS JSON));
set debug = '-d, row_upd_sec_index_entry_lock_wait';
set debug = '+d, delete_one_multi_sec_index_entry_lock_wait';
UPDATE t1 SET f1 = CAST('["98.76","87.65","76.54"]' AS JSON) WHERE 13.33 MEMBER OF (f1->'$[*]');
SELECT * FROM t1;
id	f1
3	["98.76", "87.65", "76.54"]
4	["15.67", "18.55", "19.32"]
SELECT * FROM t1 WHERE 98.76 MEMBER OF (f1->'$[*]');
id	f1
3	["98.76", "87.65", "76.54"]
SELECT * FROM t1 WHERE 87.65 MEMBER OF (f1->'$[*]');
id	f1
3	["98.76", "87.65", "76.54"]
SELECT * FROM t1 WHERE 76.54 MEMBER OF (f1->'$[*]');
id	f1
3	["98.76", "87.65", "76.54"]
SELECT * FROM t1 WHERE 15.67 MEMBER OF (f1->'$[*]');
id	f1
4	["15.67", "18.55", "19.32"]
SELECT * FROM t1 WHERE 18.55 MEMBER OF (f1->'$[*]');
id	f1
4	["15.67", "18.55", "19.32"]
SELECT * FROM t1 WHERE 19.32 MEMBER OF (f1->'$[*]');
id	f1
4	["15.67", "18.55", "19.32"]
SELECT * FROM t1 WHERE 13.45 MEMBER OF (f1->'$[*]');
id	f1
SELECT * FROM t1 WHERE 13.21 MEMBER OF (f1->'$[*]');
id	f1
SELECT * FROM t1 WHERE 13.33 MEMBER OF (f1->'$[*]');
id	f1
SELECT * FROM t1 WHERE "98.76" MEMBER OF (f1->'$[*]');
id	f1
3	["98.76", "87.65", "76.54"]
SELECT * FROM t1 WHERE "87.65" MEMBER OF (f1->'$[*]');
id	f1
3	["98.76", "87.65", "76.54"]
SELECT * FROM t1 WHERE "76.54" MEMBER OF (f1->'$[*]');
id	f1
3	["98.76", "87.65", "76.54"]
SELECT * FROM t1 WHERE "15.67" MEMBER OF (f1->'$[*]');
id	f1
4	["15.67", "18.55", "19.32"]
SELECT * FROM t1 WHERE "18.55" MEMBER OF (f1->'$[*]');
id	f1
4	["15.67", "18.55", "19.32"]
SELECT * FROM t1 WHERE "19.32" MEMBER OF (f1->'$[*]');
id	f1
4	["15.67", "18.55", "19.32"]
SELECT * FROM t1 WHERE "13.45" MEMBER OF (f1->'$[*]');
id	f1
SELECT * FROM t1 WHERE "13.21" MEMBER OF (f1->'$[*]');
id	f1
SELECT * FROM t1 WHERE "13.33" MEMBER OF (f1->'$[*]');
id	f1
UPDATE t1 SET f1 = CAST('["11.11","22.22","33.33"]' AS JSON) WHERE "18.55" MEMBER OF (f1->'$[*]');
SELECT * FROM t1;
id	f1
3	["98.76", "87.65", "76.54"]
4	["11.11", "22.22", "33.33"]
SELECT * FROM t1 WHERE 98.76 MEMBER OF (f1->'$[*]');
id	f1
3	["98.76", "87.65", "76.54"]
SELECT * FROM t1 WHERE 87.65 MEMBER OF (f1->'$[*]');
id	f1
3	["98.76", "87.65", "76.54"]
SELECT * FROM t1 WHERE 76.54 MEMBER OF (f1->'$[*]');
id	f1
3	["98.76", "87.65", "76.54"]
SELECT * FROM t1 WHERE 11.11 MEMBER OF (f1->'$[*]');
id	f1
4	["11.11", "22.22", "33.33"]
SELECT * FROM t1 WHERE 22.22 MEMBER OF (f1->'$[*]');
id	f1
4	["11.11", "22.22", "33.33"]
SELECT * FROM t1 WHERE 33.33 MEMBER OF (f1->'$[*]');
id	f1
4	["11.11", "22.22", "33.33"]
SELECT * FROM t1 WHERE 19.32 MEMBER OF (f1->'$[*]');
id	f1
SELECT * FROM t1 WHERE 15.67 MEMBER OF (f1->'$[*]');
id	f1
SELECT * FROM t1 WHERE 18.55 MEMBER OF (f1->'$[*]');
id	f1
SELECT * FROM t1 WHERE "98.76" MEMBER OF (f1->'$[*]');
id	f1
3	["98.76", "87.65", "76.54"]
SELECT * FROM t1 WHERE "87.65" MEMBER OF (f1->'$[*]');
id	f1
3	["98.76", "87.65", "76.54"]
SELECT * FROM t1 WHERE "76.54" MEMBER OF (f1->'$[*]');
id	f1
3	["98.76", "87.65", "76.54"]
SELECT * FROM t1 WHERE "11.11" MEMBER OF (f1->'$[*]');
id	f1
4	["11.11", "22.22", "33.33"]
SELECT * FROM t1 WHERE "22.22" MEMBER OF (f1->'$[*]');
id	f1
4	["11.11", "22.22", "33.33"]
SELECT * FROM t1 WHERE "33.33" MEMBER OF (f1->'$[*]');
id	f1
4	["11.11", "22.22", "33.33"]
SELECT * FROM t1 WHERE "19.32" MEMBER OF (f1->'$[*]');
id	f1
SELECT * FROM t1 WHERE "15.67" MEMBER OF (f1->'$[*]');
id	f1
SELECT * FROM t1 WHERE "18.55" MEMBER OF (f1->'$[*]');
id	f1
CHECK TABLE t1;
Table	Op	Msg_type	Msg_text
test.t1	check	status	OK
DELETE FROM t1;
INSERT INTO t1(f1) VALUES(CAST('["13.33","13.21","13.45"]' AS JSON));
INSERT INTO t1(f1) VALUES(CAST('["15.67","18.55","19.32"]' AS JSON));
set debug = '-d, delete_one_multi_sec_index_entry_lock_wait';
set debug = '+d, row_ins_sec_index_entry_lock_wait';
UPDATE t1 SET f1 = CAST('["98.76","87.65","76.54"]' AS JSON) WHERE 13.33 MEMBER OF (f1->'$[*]');
SELECT * FROM t1;
id	f1
5	["98.76", "87.65", "76.54"]
6	["15.67", "18.55", "19.32"]
SELECT * FROM t1 WHERE 98.76 MEMBER OF (f1->'$[*]');
id	f1
5	["98.76", "87.65", "76.54"]
SELECT * FROM t1 WHERE 87.65 MEMBER OF (f1->'$[*]');
id	f1
5	["98.76", "87.65", "76.54"]
SELECT * FROM t1 WHERE 76.54 MEMBER OF (f1->'$[*]');
id	f1
5	["98.76", "87.65", "76.54"]
SELECT * FROM t1 WHERE 15.67 MEMBER OF (f1->'$[*]');
id	f1
6	["15.67", "18.55", "19.32"]
SELECT * FROM t1 WHERE 18.55 MEMBER OF (f1->'$[*]');
id	f1
6	["15.67", "18.55", "19.32"]
SELECT * FROM t1 WHERE 19.32 MEMBER OF (f1->'$[*]');
id	f1
6	["15.67", "18.55", "19.32"]
SELECT * FROM t1 WHERE 13.45 MEMBER OF (f1->'$[*]');
id	f1
SELECT * FROM t1 WHERE 13.21 MEMBER OF (f1->'$[*]');
id	f1
SELECT * FROM t1 WHERE 13.33 MEMBER OF (f1->'$[*]');
id	f1
SELECT * FROM t1 WHERE "98.76" MEMBER OF (f1->'$[*]');
id	f1
5	["98.76", "87.65", "76.54"]
SELECT * FROM t1 WHERE "87.65" MEMBER OF (f1->'$[*]');
id	f1
5	["98.76", "87.65", "76.54"]
SELECT * FROM t1 WHERE "76.54" MEMBER OF (f1->'$[*]');
id	f1
5	["98.76", "87.65", "76.54"]
SELECT * FROM t1 WHERE "15.67" MEMBER OF (f1->'$[*]');
id	f1
6	["15.67", "18.55", "19.32"]
SELECT * FROM t1 WHERE "18.55" MEMBER OF (f1->'$[*]');
id	f1
6	["15.67", "18.55", "19.32"]
SELECT * FROM t1 WHERE "19.32" MEMBER OF (f1->'$[*]');
id	f1
6	["15.67", "18.55", "19.32"]
SELECT * FROM t1 WHERE "13.45" MEMBER OF (f1->'$[*]');
id	f1
SELECT * FROM t1 WHERE "13.21" MEMBER OF (f1->'$[*]');
id	f1
SELECT * FROM t1 WHERE "13.33" MEMBER OF (f1->'$[*]');
id	f1
UPDATE t1 SET f1 = CAST('["11.11","22.22","33.33"]' AS JSON) WHERE "18.55" MEMBER OF (f1->'$[*]');
SELECT * FROM t1;
id	f1
5	["98.76", "87.65", "76.54"]
6	["11.11", "22.22", "33.33"]
SELECT * FROM t1 WHERE 98.76 MEMBER OF (f1->'$[*]');
id	f1
5	["98.76", "87.65", "76.54"]
SELECT * FROM t1 WHERE 87.65 MEMBER OF (f1->'$[*]');
id	f1
5	["98.76", "87.65", "76.54"]
SELECT * FROM t1 WHERE 76.54 MEMBER OF (f1->'$[*]');
id	f1
5	["98.76", "87.65", "76.54"]
SELECT * FROM t1 WHERE 11.11 MEMBER OF (f1->'$[*]');
id	f1
6	["11.11", "22.22", "33.33"]
SELECT * FROM t1 WHERE 22.22 MEMBER OF (f1->'$[*]');
id	f1
6	["11.11", "22.22", "33.33"]
SELECT * FROM t1 WHERE 33.33 MEMBER OF (f1->'$[*]');
id	f1
6	["11.11", "22.22", "33.33"]
SELECT * FROM t1 WHERE 19.32 MEMBER OF (f1->'$[*]');
id	f1
SELECT * FROM t1 WHERE 15.67 MEMBER OF (f1->'$[*]');
id	f1
SELECT * FROM t1 WHERE 18.55 MEMBER OF (f1->'$[*]');
id	f1
SELECT * FROM t1 WHERE "98.76" MEMBER OF (f1->'$[*]');
id	f1
5	["98.76", "87.65", "76.54"]
SELECT * FROM t1 WHERE "87.65" MEMBER OF (f1->'$[*]');
id	f1
5	["98.76", "87.65", "76.54"]
SELECT * FROM t1 WHERE "76.54" MEMBER OF (f1->'$[*]');
id	f1
5	["98.76", "87.65", "76.54"]
SELECT * FROM t1 WHERE "11.11" MEMBER OF (f1->'$[*]');
id	f1
6	["11.11", "22.22", "33.33"]
SELECT * FROM t1 WHERE "22.22" MEMBER OF (f1->'$[*]');
id	f1
6	["11.11", "22.22", "33.33"]
SELECT * FROM t1 WHERE "33.33" MEMBER OF (f1->'$[*]');
id	f1
6	["11.11", "22.22", "33.33"]
SELECT * FROM t1 WHERE "19.32" MEMBER OF (f1->'$[*]');
id	f1
SELECT * FROM t1 WHERE "15.67" MEMBER OF (f1->'$[*]');
id	f1
SELECT * FROM t1 WHERE "18.55" MEMBER OF (f1->'$[*]');
id	f1
set debug = '-d, row_ins_sec_index_entry_lock_wait';
CHECK TABLE t1;
Table	Op	Msg_type	Msg_text
test.t1	check	status	OK
DELETE FROM t1;
DROP TABLE t1;
CREATE TABLE t1(id INT NOT NULL PRIMARY KEY AUTO_INCREMENT, f1 JSON);
INSERT INTO t1(f1) VALUES (CAST('[1,3]' AS JSON));
INSERT INTO t1(f1) VALUES (CAST('[1,3,9,8]' AS JSON));
INSERT INTO t1(f1) VALUES (CAST('[1,3,9,8,10,11,20,30]' AS JSON));
SET DEBUG='+d, ddl_add_multi_value';
CREATE INDEX idx1 ON t1((CAST(f1->"$[*]" AS UNSIGNED ARRAY)));
SELECT * FROM t1 WHERE 9 MEMBER OF (f1->'$[*]');
id	f1
2	[1, 3, 9, 8]
3	[1, 3, 9, 8, 10, 11, 20, 30]
SELECT * FROM t1 WHERE 11 MEMBER OF (f1->'$[*]');
id	f1
3	[1, 3, 9, 8, 10, 11, 20, 30]
SELECT * FROM t1 WHERE 30 MEMBER OF (f1->'$[*]');
id	f1
3	[1, 3, 9, 8, 10, 11, 20, 30]
SET DEBUG='-d, ddl_add_multi_value';
CHECK TABLE t1;
Table	Op	Msg_type	Msg_text
test.t1	check	status	OK
DROP TABLE t1;
CREATE TABLE t1(j JSON);
INSERT INTO t1 VALUES ('["test"]'), ('[]'), ('null');
SET DEBUG_SYNC='innodb_commit_inplace_alter_table_enter SIGNAL rolling_back WAIT_FOR drop_created';
CREATE INDEX mv_idx_date ON t1 ((CAST(j->'$' AS UNSIGNED ARRAY)));;
SET DEBUG_SYNC='now WAIT_FOR rolling_back';
SET DEBUG_SYNC='row_search_for_mysql_before_return SIGNAL scanning WAIT_FOR continue_scan';
SELECT * FROM t1;;
SET DEBUG_SYNC='now WAIT_FOR scanning';
SET DEBUG_SYNC='now SIGNAL drop_created';
ERROR 22018: Invalid JSON value for CAST for functional index 'mv_idx_date'.
SET DEBUG_SYNC='now SIGNAL continue_scan';
j
["test"]
[]
null
INSERT INTO t1 VALUES ('["Hello"]');
CHECK TABLE t1;
Table	Op	Msg_type	Msg_text
test.t1	check	status	OK
DROP TABLE t1;
CREATE TABLE t1 (
i INT NOT NULL PRIMARY KEY,
j JSON DEFAULT (CAST('["HelloWorld"]' AS JSON)),
KEY mv_idx_binary (( CAST(j->'$[*]' AS BINARY(10) ARRAY) )) );
INSERT INTO t1 VALUES(1, CAST('["HelloWorld", "Hello"]' AS JSON));
INSERT INTO t1 VALUES(2, CAST('["HelloMySQL", "Hello"]' AS JSON));
SELECT * FROM t1 WHERE "HelloMySQL" MEMBER OF (j->'$[*]');
i	j
2	["HelloMySQL", "Hello"]
SELECT * FROM t1 WHERE "Hello" MEMBER OF (j->'$[*]');
i	j
1	["HelloWorld", "Hello"]
2	["HelloMySQL", "Hello"]
SET GLOBAL innodb_purge_stop_now = ON;
DELETE FROM t1;
START TRANSACTION;
INSERT INTO t1 VALUES(1, CAST('["HelloMySQL", "Hello"]' AS JSON));
SELECT * FROM t1 WHERE "HelloMySQL" MEMBER OF (j->'$[*]');
i	j
1	["HelloMySQL", "Hello"]
SELECT * FROM t1 WHERE "Hello" MEMBER OF (j->'$[*]');
i	j
1	["HelloMySQL", "Hello"]
ROLLBACK;
SET GLOBAL innodb_purge_run_now=ON;
SELECT * FROM t1 WHERE "HelloMySQL" MEMBER OF (j->'$[*]');
i	j
SELECT * FROM t1 WHERE "Hello" MEMBER OF (j->'$[*]');
i	j
CHECK TABLE t1;
Table	Op	Msg_type	Msg_text
test.t1	check	status	OK
DROP TABLE t1;
CREATE TABLE t1 (id INT NOT NULL KEY AUTO_INCREMENT, f1 JSON, f2 JSON, f3 JSON, f4 JSON , f5 JSON, f6 JSON,
KEY mv_idx_binary (( CAST(f1->'$[*]' AS CHAR(10) ARRAY) )),
KEY mv_idx_binary1 (( CAST(f2->'$[*]' AS CHAR(10) ARRAY) )),
KEY mv_idx_binary2 (( CAST(f3->'$[*]' AS CHAR(10) ARRAY) )),
KEY mv_idx_binary3 (( CAST(f4->'$[*]' AS CHAR(10) ARRAY) )),
KEY mv_idx_binary4 (( CAST(f5->'$[*]' AS CHAR(10) ARRAY) )),
KEY mv_idx_binary5 (( CAST(f6->'$[*]' AS CHAR(10) ARRAY) )));
INSERT INTO t1 VALUES(1, CAST('["HelloWorld", "Hello"]' AS JSON), CAST('["HelloWorld", "Hello"]' AS JSON), CAST('["HelloWorld", "Hello"]' AS JSON), CAST('["HelloWorld", "Hello"]' AS JSON), CAST('["HelloWorld", "Hello"]' AS JSON), CAST('["HelloWorld", "Hello"]' AS JSON));
SET GLOBAL innodb_purge_stop_now = ON;
DELETE FROM t1;
INSERT INTO t1 VALUES(1, CAST('["Helloworl", "hell"]' AS JSON), CAST('["Helloworl", "hell"]' AS JSON), CAST('["helloworl", "hell"]' AS JSON), CAST('["helloworl", "hell"]' AS JSON), CAST('["helloworl", "well"]' AS JSON), CAST('["helloworl", "hell"]' AS JSON));
CHECK TABLE t1;
Table	Op	Msg_type	Msg_text
test.t1	check	status	OK
DELETE FROM t1;
CHECK TABLE t1;
Table	Op	Msg_type	Msg_text
test.t1	check	status	OK
DROP TABLE t1;
SET GLOBAL innodb_purge_run_now=ON;
CREATE TABLE IF NOT EXISTS t1 (
i INT NOT NULL PRIMARY KEY,
j JSON DEFAULT (CAST('["100"]' AS JSON)),
UNIQUE KEY mv_idx_binary (( CAST(j->'$[*]' AS UNSIGNED ARRAY) )) );
INSERT INTO t1 VALUES(1, CAST('["90"]' AS JSON));
START TRANSACTION;
INSERT INTO t1 VALUES(2, CAST('["100"]' AS JSON));
START TRANSACTION;
SET DEBUG_SYNC='lock_rec_convert_impl_to_expl SIGNAL converting WAIT_FOR go';
INSERT INTO t1 VALUES(3, CAST('["100"]' AS JSON));;
SET DEBUG_SYNC='now WAIT_FOR converting';
COMMIT;
SET DEBUG_SYNC='now SIGNAL go';
ERROR 23000: Duplicate entry '[100]' for key 't1.mv_idx_binary'
START TRANSACTION;
SET DEBUG_SYNC='row_update_for_mysql_error SIGNAL del_fail WAIT_FOR delete_go';
DELETE FROM t1;;
SET DEBUG_SYNC='now WAIT_FOR del_fail';
INSERT INTO t1 VALUES(4, CAST('["100"]' AS JSON));
ERROR 23000: Duplicate entry '[100]' for key 't1.mv_idx_binary'
SET DEBUG_SYNC='now SIGNAL delete_go';
COMMIT;
COMMIT;
CHECK TABLE t1;
Table	Op	Msg_type	Msg_text
test.t1	check	status	OK
SELECT * FROM t1;
i	j
DROP TABLE t1;
CREATE TABLE t1 (a INT, b INT, f JSON,
KEY mv_idx_binary ((( CAST(f->'$[*]' AS BINARY(10) ARRAY ))))
) ENGINE=InnoDB;
INSERT INTO t1 (a,b,f) VALUES(1, 1, '["foobar"]');
CREATE TABLE t0 (a INT) ENGINE=InnoDB;
BEGIN;
SELECT * FROM t0;
a
UPDATE t1 SET f = NULL;
ALTER TABLE t1 DROP INDEX mv_idx_binary, ALGORITHM=INPLACE;
COMMIT;
UPDATE t1 SET a = 1;
set global innodb_purge_run_now=ON;
CHECK TABLE t1;
Table	Op	Msg_type	Msg_text
test.t1	check	status	OK
SELECT * FROM t1;
a	b	f
1	1	NULL
DROP TABLE t0, t1;
CREATE TABLE t1(a INT PRIMARY KEY, b INT, f1 JSON) ENGINE=InnoDB;
CREATE INDEX i1 ON t1((CAST(f1->"$[*]" AS CHAR(10) ARRAY)));
INSERT INTO t1 VALUES(1, 1, CAST('["1111", "2222"]' AS JSON));
INSERT INTO t1 VALUES(2, 2, CAST('["11111", "2222", "22222", "3333"]' AS JSON));
INSERT INTO t1 VALUES(3, 3, CAST('["1111", "2222", "22222", "3333", "444"]' AS JSON));
INSERT INTO t1 VALUES(4, 4, CAST('["22222", "3333", "444", "5555"]' AS JSON));
SET DEBUG_SYNC='row_log_table_apply1_before SIGNAL before_apply WAIT_FOR apply';
OPTIMIZE TABLE t1;;
connection con1
SET DEBUG_SYNC='now WAIT_FOR before_apply';
INSERT INTO t1 VALUES(5, 5, CAST('["2222", "5555"]' AS JSON));
DELETE FROM t1 WHERE "444" MEMBER OF (f1->"$[*]");
INSERT INTO t1 VALUES(6, 6, CAST('["1111", "444"]' AS JSON));
UPDATE t1 SET f1 = CAST('["11111", "2222"]' AS JSON) WHERE a = 1;
UPDATE t1 SET f1 = CAST('["11111", "22222", "33333", "44444"]' AS JSON) WHERE a = 3;
UPDATE t1 SET f1 = CAST('["2222", "5555", "6666"]' AS JSON) WHERE a = 5;
SET DEBUG_SYNC='now SIGNAL apply';
connection default
/* reap */ OPTIMIZE TABLE t1;
Table	Op	Msg_type	Msg_text
test.t1	optimize	note	Table does not support optimize, doing recreate + analyze instead
test.t1	optimize	status	OK
CHECK TABLE t1;
Table	Op	Msg_type	Msg_text
test.t1	check	status	OK
SELECT * FROM t1;
a	b	f1
1	1	["11111", "2222"]
2	2	["11111", "2222", "22222", "3333"]
5	5	["2222", "5555", "6666"]
6	6	["1111", "444"]
SELECT * FROM t1 WHERE "1111" MEMBER OF (f1->"$[*]");
a	b	f1
6	6	["1111", "444"]
SELECT * FROM t1 WHERE "5555" MEMBER OF (f1->"$[*]");
a	b	f1
5	5	["2222", "5555", "6666"]
SELECT * FROM t1 WHERE "444" MEMBER OF (f1->"$[*]");
a	b	f1
6	6	["1111", "444"]
SELECT * FROM t1 WHERE "11111" MEMBER OF (f1->"$[*]");
a	b	f1
1	1	["11111", "2222"]
2	2	["11111", "2222", "22222", "3333"]
SELECT * FROM t1 WHERE "22222" MEMBER OF (f1->"$[*]");
a	b	f1
2	2	["11111", "2222", "22222", "3333"]
SET DEBUG_SYNC='row_log_table_apply1_before SIGNAL before_apply_1 WAIT_FOR apply_1';
OPTIMIZE TABLE t1;;
SET DEBUG_SYNC='now WAIT_FOR before_apply_1';
INSERT INTO t1 VALUES(7, 7, CAST('["6666", "1111"]' AS JSON));
DELETE FROM t1 WHERE "22222" MEMBER OF (f1->"$[*]");
INSERT INTO t1 VALUES(8, 8, CAST('["1111", "3333", "444", "6666"]' AS JSON));
UPDATE t1 SET f1 = CAST('["5555", "2222", "6666", "777"]' AS JSON) WHERE a = 5;
UPDATE t1 SET f1 = CAST('["666", "777", "6666", "7777"]' AS JSON) WHERE a = 6;
SET DEBUG_SYNC='now SIGNAL apply_1';
connection default
/* reap */ OPTIMIZE TABLE t1;
Table	Op	Msg_type	Msg_text
test.t1	optimize	note	Table does not support optimize, doing recreate + analyze instead
test.t1	optimize	status	OK
CHECK TABLE t1;
Table	Op	Msg_type	Msg_text
test.t1	check	status	OK
SELECT * FROM t1;
a	b	f1
1	1	["11111", "2222"]
5	5	["5555", "2222", "6666", "777"]
6	6	["666", "777", "6666", "7777"]
7	7	["6666", "1111"]
8	8	["1111", "3333", "444", "6666"]
SELECT * FROM t1 WHERE "1111" MEMBER OF (f1->"$[*]");
a	b	f1
7	7	["6666", "1111"]
8	8	["1111", "3333", "444", "6666"]
SELECT * FROM t1 WHERE "5555" MEMBER OF (f1->"$[*]");
a	b	f1
5	5	["5555", "2222", "6666", "777"]
SELECT * FROM t1 WHERE "444" MEMBER OF (f1->"$[*]");
a	b	f1
8	8	["1111", "3333", "444", "6666"]
SELECT * FROM t1 WHERE "11111" MEMBER OF (f1->"$[*]");
a	b	f1
1	1	["11111", "2222"]
SELECT * FROM t1 WHERE "22222" MEMBER OF (f1->"$[*]");
a	b	f1
SELECT * FROM t1 WHERE "666" MEMBER OF (f1->"$[*]");
a	b	f1
6	6	["666", "777", "6666", "7777"]
SELECT * FROM t1 WHERE "6666" MEMBER OF (f1->"$[*]");
a	b	f1
5	5	["5555", "2222", "6666", "777"]
6	6	["666", "777", "6666", "7777"]
7	7	["6666", "1111"]
8	8	["1111", "3333", "444", "6666"]
SELECT * FROM t1 WHERE "777" MEMBER OF (f1->"$[*]");
a	b	f1
5	5	["5555", "2222", "6666", "777"]
6	6	["666", "777", "6666", "7777"]
SELECT * FROM t1 WHERE "7777" MEMBER OF (f1->"$[*]");
a	b	f1
6	6	["666", "777", "6666", "7777"]
SELECT * FROM t1 WHERE "3333" MEMBER OF (f1->"$[*]");
a	b	f1
8	8	["1111", "3333", "444", "6666"]
DROP TABLE t1;
CREATE TABLE `t1` (
`id` int(11) NOT NULL AUTO_INCREMENT,
`j1` JSON DEFAULT (CAST(_utf8mb4'[5,-1,0]' AS JSON)),
`j2` JSON DEFAULT (CAST(_utf8mb4'[9,8,0,1]' AS JSON)),
`j3` JSON DEFAULT (CAST(_utf8mb4'["foobar"]' AS JSON)),
`j4` JSON DEFAULT (CAST(_utf8mb4'["HelloWorld"]' AS JSON)),
`j5` JSON DEFAULT (CAST(_utf8mb4'["HelloMySQL"]' AS JSON)),
PRIMARY KEY (`id`),
KEY `mv_idx_binary` ((JSON_LENGTH(`j3`)), (CAST(JSON_EXTRACT(`j4`,_utf8mb4'$[*]') AS BINARY(10) ARRAY)), (JSON_DEPTH(`j2`)))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;
Warnings:
Warning	1681	Integer display width is deprecated and will be removed in a future release.
INSERT INTO t1 VALUES(0, CAST(_utf8mb4'[5,-1,0]' AS JSON), CAST(_utf8mb4'[9,8,0,1]' AS JSON), CAST(_utf8mb4'["foobar"]' AS JSON), CAST(_utf8mb4'["HelloWorld"]' AS JSON), CAST(_utf8mb4'["HelloMySQL"]' AS JSON));
INSERT INTO t1 VALUES(0, CAST(_utf8mb4'[5,-1,0]' AS JSON), CAST(_utf8mb4'[9,8,0,1]' AS JSON), CAST(_utf8mb4'["foobar"]' AS JSON), CAST(_utf8mb4'["HelloWorld"]' AS JSON), '[]');
SELECT * FROM t1;
id	j1	j2	j3	j4	j5
1	[5, -1, 0]	[9, 8, 0, 1]	["foobar"]	["HelloWorld"]	["HelloMySQL"]
2	[5, -1, 0]	[9, 8, 0, 1]	["foobar"]	["HelloWorld"]	[]
SET DEBUG_SYNC='innodb_commit_inplace_alter_table_enter SIGNAL rollback WAIT_FOR try_drop';
CREATE INDEX mv_idx_date ON t1 ((CAST(j5->'$' AS UNSIGNED ARRAY)));;
SET DEBUG_SYNC='now WAIT_FOR rollback';
SET DEBUG_SYNC='row_search_for_mysql_before_return SIGNAL scanning WAIT_FOR continue';
SELECT * FROM t1;;
SET DEBUG_SYNC='now WAIT_FOR scanning';
SET DEBUG_SYNC='now SIGNAL try_drop';
ERROR 22018: Invalid JSON value for CAST for functional index 'mv_idx_date'.
SET DEBUG_SYNC='now SIGNAL continue';
id	j1	j2	j3	j4	j5
1	[5, -1, 0]	[9, 8, 0, 1]	["foobar"]	["HelloWorld"]	["HelloMySQL"]
2	[5, -1, 0]	[9, 8, 0, 1]	["foobar"]	["HelloWorld"]	[]
SET DEBUG_SYNC='innodb_inplace_alter_table_enter SIGNAL inplace_done WAIT_FOR upgrade_lock';
ALTER TABLE t1 DROP INDEX mv_idx_binary;;
SET DEBUG_SYNC='now WAIT_FOR inplace_done';
SET DEBUG_SYNC='innodb_row_upd_clust_step_enter SIGNAL upgrade_lock';
DELETE FROM t1;;
SELECT * FROM t1;
id	j1	j2	j3	j4	j5
DROP TABLE t1;
#
# Bug#31474182 THE MULTI-VALUED INDEX RETURNS ERROR WHEN --DEBUG OPTION SPECIFIED.
#
set debug = 'd:t';
CREATE TABLE t1 (f1 JSON, id VARCHAR(64) AS (f1->"$[0]") STORED NOT NULL PRIMARY KEY);
CREATE INDEX i1 ON t1((CAST(f1->"$[*]" AS UNSIGNED ARRAY)));
ANALYZE TABLE t1;
Table	Op	Msg_type	Msg_text
test.t1	analyze	status	OK
SELECT * FROM t1 WHERE JSON_OVERLAPS(f1->'$[*]', '[4,3,7]') ORDER BY id;
f1	id
DROP TABLE t1;
set debug = default;
