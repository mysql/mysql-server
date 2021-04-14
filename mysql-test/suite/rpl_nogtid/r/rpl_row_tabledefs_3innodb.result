include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
STOP SLAVE;
Warnings:
Warning	1287	'STOP SLAVE' is deprecated and will be removed in a future release. Please use STOP REPLICA instead
START SLAVE;
Warnings:
Warning	1287	'START SLAVE' is deprecated and will be removed in a future release. Please use START REPLICA instead
CREATE TABLE t1_int (a INT PRIMARY KEY, b INT) ENGINE='InnoDB';
CREATE TABLE t1_bit (a INT PRIMARY KEY, b INT) ENGINE='InnoDB';
CREATE TABLE t1_char (a INT PRIMARY KEY, b INT) ENGINE='InnoDB';
CREATE TABLE t1_nodef (a INT PRIMARY KEY, b INT) ENGINE='InnoDB';
CREATE TABLE t2 (a INT PRIMARY KEY, b INT) ENGINE='InnoDB';
CREATE TABLE t3 (a INT PRIMARY KEY, b INT) ENGINE='InnoDB';
CREATE TABLE t4 (a INT) ENGINE='InnoDB';
CREATE TABLE t5 (a INT, b INT, c INT) ENGINE='InnoDB';
CREATE TABLE t6 (a INT, b INT, c INT) ENGINE='InnoDB';
CREATE TABLE t7 (a INT NOT NULL) ENGINE='InnoDB';
CREATE TABLE t8 (a INT NOT NULL) ENGINE='InnoDB';
CREATE TABLE t9 (a INT) ENGINE='InnoDB';
ALTER TABLE t1_int ADD x INT DEFAULT 42;
ALTER TABLE t1_bit
ADD x BIT(3) DEFAULT b'011',
ADD y BIT(5) DEFAULT b'10101',
ADD z BIT(2) DEFAULT b'10';
ALTER TABLE t1_char ADD x CHAR(20) DEFAULT 'Just a test';
ALTER TABLE t1_nodef ADD x INT NOT NULL, ADD y INT NOT NULL, ADD z INT NOT NULL;
ALTER TABLE t2 DROP b;
ALTER TABLE t4 MODIFY a FLOAT;
ALTER TABLE t5 MODIFY b FLOAT;
ALTER TABLE t6 MODIFY c FLOAT;
ALTER TABLE t7 ADD e1 INT, ADD e2 INT, ADD e3 INT, ADD e4 INT,
ADD e5 INT, ADD e6 INT, ADD e7 INT, ADD e8 INT;
ALTER TABLE t8 ADD e1 INT NOT NULL DEFAULT 0, ADD e2 INT NOT NULL DEFAULT 0,
ADD e3 INT NOT NULL DEFAULT 0, ADD e4 INT NOT NULL DEFAULT 0,
ADD e5 INT NOT NULL DEFAULT 0, ADD e6 INT NOT NULL DEFAULT 0,
ADD e7 INT NOT NULL DEFAULT 0, ADD e8 INT NOT NULL DEFAULT 0;
set @@global.replica_exec_mode= 'IDEMPOTENT';
INSERT INTO t1_int  VALUES (2, 4, 4711);
INSERT INTO t1_char VALUES (2, 4, 'Foo is a bar');
INSERT INTO t1_bit  VALUES (2, 4, b'101', b'11100', b'01');
**** On Master ****
INSERT INTO t1_int VALUES (1,2);
INSERT INTO t1_int VALUES (2,5);
INSERT INTO t1_bit VALUES (1,2);
INSERT INTO t1_bit VALUES (2,5);
INSERT INTO t1_char VALUES (1,2);
INSERT INTO t1_char VALUES (2,5);
SELECT * FROM t1_int ORDER BY a;
a	b
1	2
2	5
SELECT * FROM t1_bit ORDER BY a;
a	b
1	2
2	5
SELECT * FROM t1_char ORDER BY a;
a	b
1	2
2	5
**** On Slave ****
set @@global.replica_exec_mode= default;
SELECT a,b,x FROM t1_int ORDER BY a;
a	b	x
1	2	42
2	5	4711
SELECT a,b,HEX(x),HEX(y),HEX(z) FROM t1_bit ORDER BY a;
a	b	HEX(x)	HEX(y)	HEX(z)
1	2	3	15	2
2	5	5	1C	1
SELECT a,b,x FROM t1_char ORDER BY a;
a	b	x
1	2	Just a test
2	5	Foo is a bar
**** On Master ****
UPDATE t1_int  SET b=2*b WHERE a=2;
UPDATE t1_char SET b=2*b WHERE a=2;
UPDATE t1_bit  SET b=2*b WHERE a=2;
SELECT * FROM t1_int ORDER BY a;
a	b
1	2
2	10
SELECT * FROM t1_bit ORDER BY a;
a	b
1	2
2	10
SELECT * FROM t1_char ORDER BY a;
a	b
1	2
2	10
**** On Slave ****
SELECT a,b,x FROM t1_int ORDER BY a;
a	b	x
1	2	42
2	10	4711
SELECT a,b,HEX(x),HEX(y),HEX(z) FROM t1_bit ORDER BY a;
a	b	HEX(x)	HEX(y)	HEX(z)
1	2	3	15	2
2	10	5	1C	1
SELECT a,b,x FROM t1_char ORDER BY a;
a	b	x
1	2	Just a test
2	10	Foo is a bar
INSERT INTO t9 VALUES (2);
INSERT INTO t1_nodef VALUES (1,2);
select count(*) from t1_nodef;
count(*)
1
INSERT INTO t9 VALUES (2);
**** On Master ****
INSERT INTO t2 VALUES (2,4);
SELECT * FROM t2;
a	b
2	4
**** On Slave ****
SELECT * FROM t2;
a
2
include/check_slave_is_running.inc
INSERT INTO t9 VALUES (4);
INSERT INTO t4 VALUES (4);
call mtr.add_suppression("Slave SQL.*Table definition on master and slave does not match: Column [012] type mismatch.* Error_code: MY-001535");
call mtr.add_suppression("Slave SQL.*Column [0-9] of table .test.t[0-9]. cannot be converted from type.* Error_code: MY-013146");
call mtr.add_suppression("The slave coordinator and worker threads are stopped, possibly leaving data in inconsistent state");
include/wait_for_slave_sql_error_and_skip.inc [errno=13146]
INSERT INTO t9 VALUES (5);
INSERT INTO t5 VALUES (5,10,25);
include/wait_for_slave_sql_error_and_skip.inc [errno=13146]
INSERT INTO t9 VALUES (6);
INSERT INTO t6 VALUES (6,12,36);
include/wait_for_slave_sql_error_and_skip.inc [errno=13146]
INSERT INTO t9 VALUES (6);
include/check_slave_is_running.inc
INSERT INTO t7 VALUES (1),(2),(3);
INSERT INTO t8 VALUES (1),(2),(3);
SELECT * FROM t7 ORDER BY a;
a
1
2
3
SELECT * FROM t8 ORDER BY a;
a
1
2
3
SELECT * FROM t7 ORDER BY a;
a	e1	e2	e3	e4	e5	e6	e7	e8
1	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL
2	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL
3	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL
SELECT * FROM t8 ORDER BY a;
a	e1	e2	e3	e4	e5	e6	e7	e8
1	0	0	0	0	0	0	0	0
2	0	0	0	0	0	0	0	0
3	0	0	0	0	0	0	0	0
**** On Master ****
TRUNCATE t1_nodef;
SET SQL_LOG_BIN=0;
INSERT INTO t1_nodef VALUES (1,2);
INSERT INTO t1_nodef VALUES (2,4);
SET SQL_LOG_BIN=1;
**** On Slave ****
INSERT INTO t1_nodef VALUES (1,2,3,4,5);
INSERT INTO t1_nodef VALUES (2,4,6,8,10);
**** On Master ****
UPDATE t1_nodef SET b=2*b WHERE a=1;
SELECT * FROM t1_nodef ORDER BY a;
a	b
1	4
2	4
**** On Slave ****
SELECT * FROM t1_nodef ORDER BY a;
a	b	x	y	z
1	4	3	4	5
2	4	6	8	10
**** On Master ****
DELETE FROM t1_nodef WHERE a=2;
SELECT * FROM t1_nodef ORDER BY a;
a	b
1	4
**** On Slave ****
SELECT * FROM t1_nodef ORDER BY a;
a	b	x	y	z
1	4	3	4	5
**** Cleanup ****
DROP TABLE IF EXISTS t1_int,t1_bit,t1_char,t1_nodef;
DROP TABLE IF EXISTS t2,t3,t4,t5,t6,t7,t8,t9;
include/rpl_end.inc
