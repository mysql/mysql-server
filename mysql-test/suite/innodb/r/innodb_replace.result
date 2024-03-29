#
#Bug#11759688 52020: InnoDB can still deadlock
#on just INSERT...ON DUPLICATE KEY
#a.k.a. Bug#7975 deadlock without any locking, simple select and update
#
CREATE TABLE t1 (a INT PRIMARY KEY, b INT NOT NULL) ENGINE=InnoDB;
INSERT INTO t1 VALUES(3,1);
BEGIN;
SET DEBUG_SYNC='write_row_noreplace SIGNAL insert1 WAIT_FOR select1';
INSERT INTO t1 VALUES(3,2);
SET DEBUG_SYNC='now WAIT_FOR insert1';
SELECT * FROM t1 LOCK IN SHARE MODE;
a	b
3	1
SET DEBUG_SYNC='lock_wait_will_wait SIGNAL default_will_wait';
SELECT * FROM t1 FOR UPDATE;
SET DEBUG_SYNC='now WAIT_FOR default_will_wait';
SET DEBUG_SYNC='now SIGNAL select1';
ERROR 23000: Duplicate entry '3' for key 't1.PRIMARY'
INSERT INTO t1 VALUES(3,3) ON DUPLICATE KEY UPDATE b=b+10;
COMMIT;
a	b
3	11
SET DEBUG_SYNC='write_row_replace SIGNAL insert2 WAIT_FOR select2';
REPLACE INTO t1 VALUES(3,4);
SET DEBUG_SYNC='now WAIT_FOR insert2';
SET DEBUG_SYNC='lock_wait_will_wait SIGNAL default_will_wait';
SELECT * FROM t1;
a	b
3	11
SELECT * FROM t1 LOCK IN SHARE MODE;
SET DEBUG_SYNC='now WAIT_FOR default_will_wait';
SET DEBUG_SYNC='now SIGNAL select2';
SET DEBUG_SYNC='write_row_replace SIGNAL insert3 WAIT_FOR select3';
INSERT INTO t1 VALUES(3,5) ON DUPLICATE KEY UPDATE b=b+20;
a	b
3	4
SET DEBUG_SYNC='now WAIT_FOR insert3';
SET DEBUG_SYNC='lock_wait_will_wait SIGNAL default_will_wait';
SELECT b FROM t1 LOCK IN SHARE MODE;
SET DEBUG_SYNC='now WAIT_FOR default_will_wait';
SET DEBUG_SYNC='now SIGNAL select3';
b
24
SET DEBUG_SYNC='write_row_noreplace SIGNAL insert4 WAIT_FOR select4';
LOAD DATA INFILE '../../std_data/loaddata5.dat' INTO TABLE t1 FIELDS TERMINATED BY '' ENCLOSED BY '' (a, b);
SET DEBUG_SYNC='now WAIT_FOR insert4';
SELECT b FROM t1 WHERE a=3 LOCK IN SHARE MODE;
b
24
SET DEBUG_SYNC='lock_wait_will_wait SIGNAL default_will_wait';
SELECT b FROM t1 WHERE a=3 FOR UPDATE;
SET DEBUG_SYNC='now WAIT_FOR default_will_wait';
SET DEBUG_SYNC='now SIGNAL select4';
b
24
ERROR 23000: Duplicate entry '3' for key 't1.PRIMARY'
SET DEBUG_SYNC='write_row_noreplace SIGNAL insert5 WAIT_FOR select5';
LOAD DATA INFILE '../../std_data/loaddata5.dat' IGNORE INTO TABLE t1 FIELDS TERMINATED BY '' ENCLOSED BY '' (a, b);
SET DEBUG_SYNC='now WAIT_FOR insert5';
SELECT * FROM t1;
a	b
3	24
SELECT * FROM t1 WHERE a=3 LOCK IN SHARE MODE;
a	b
3	24
SET DEBUG_SYNC='lock_wait_will_wait SIGNAL default_will_wait';
SELECT * FROM t1 WHERE a=3 FOR UPDATE;
SET DEBUG_SYNC='now WAIT_FOR default_will_wait';
SET DEBUG_SYNC='now SIGNAL select5';
Warnings:
Warning	1062	Duplicate entry '3' for key 't1.PRIMARY'
a	b
3	24
SET DEBUG_SYNC='write_row_replace SIGNAL insert6 WAIT_FOR select6';
LOAD DATA INFILE '../../std_data/loaddata5.dat' REPLACE INTO TABLE t1 FIELDS TERMINATED BY '' ENCLOSED BY '' (a, b);
SET DEBUG_SYNC='now WAIT_FOR insert6';
SELECT * FROM t1;
a	b
1	2
3	24
5	6
SET DEBUG_SYNC='lock_wait_will_wait SIGNAL default_will_wait';
SELECT a,b FROM t1 LOCK IN SHARE MODE;
SET DEBUG_SYNC='now WAIT_FOR default_will_wait';
SET DEBUG_SYNC='now SIGNAL select6';
a	b
1	2
3	4
5	6
SET DEBUG_SYNC='RESET';
DROP TABLE t1;
START TRANSACTION;
CREATE TEMPORARY TABLE t1 (a INT, b CHAR(10), d VARCHAR(100), c INT, UNIQUE INDEX(a), UNIQUE INDEX(b), UNIQUE INDEX(d)) ENGINE = InnoDB;
INSERT INTO t1 VALUES(2, 'abcde', 'qwerty', 200);
REPLACE INTO t1 VALUES(2, 'QWERTY', 'ZXCVBB', 300);
SELECT * FROM t1;
a	b	d	c
2	QWERTY	ZXCVBB	300
COMMIT;
DROP TABLE t1;
#
# Bug#22884631 FAILING ASSERTION: TRX->DUPLICATES == 0
#
SELECT @@table_open_cache_instances;
@@table_open_cache_instances
1
CREATE TABLE t1(a INT);
CREATE TABLE t2(a INT);
INSERT INTO t1 VALUES(1);
INSERT INTO t2 VALUES(2);
# Connection con1
INSERT INTO t1 VALUES(3);
SET DEBUG_SYNC = 'RESET';
SET DEBUG_SYNC = 'ib_after_row_insert WAIT_FOR go_ahead';
REPLACE INTO t2 VALUES(3);
# Connection con2
INSERT INTO t1 VALUES(70, 80);
ERROR 21S01: Column count doesn't match value count at row 1
SET DEBUG_SYNC= 'now SIGNAL go_ahead';
# Connection con1
DROP TABLE t1;
DROP TABLE t2;
SET DEBUG_SYNC= 'RESET';
