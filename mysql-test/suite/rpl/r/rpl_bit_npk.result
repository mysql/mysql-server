include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
DROP TABLE IF EXISTS test.t1;
CREATE TABLE test.t1 (
dummyKey INTEGER NOT NULL,
f01 TINYINT,
f10 TINYINT,
f12 TINYINT,
f15 TINYINT,
f16 TINYINT,
f7 TINYINT,
f9 TINYINT,
f29 TINYINT,
f0 TINYINT,
fA1 TINYINT,
C32 TINYINT,
A42 TINYINT,
CA3 TINYINT,
A044 TINYINT,
f001 TINYINT,
A3002 TINYINT,
fC003 TINYINT,
CA300 TINYINT,
A305 TINYINT,
CA321 TINYINT,
r001 TINYINT,
bit1 BIT(6),
bit2 BIT(6),
bit3 BIT(6),
State1 TINYINT,
State2 TINYINT,
State3 TINYINT,
State4 TINYINT,
SubState TINYINT,
gState TINYINT,
oSupp TINYINT,
tSupp TINYINT,
sSuppD TINYINT,
mSuppf TINYINT,
GSuppDf TINYINT,
VNotSupp TINYINT,
x034 TINYINT);
LOCK TABLES test.t1 WRITE;
INSERT INTO test.t1  VALUES (6,1,1,1,1,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,b'111111',b'111110',b'110101',4,5,5,5,5,5,5,5,5,5,3,NULL,1);
INSERT INTO  test.t1 VALUES (1,1,1,1,1,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,b'111111',b'000000',b'100100',4,5,5,5,5,5,5,5,5,5,3,2,1);
INSERT INTO  test.t1 VALUES (2,1,1,1,1,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,b'000000',b'101010',b'010101',4,5,5,5,5,5,5,5,5,5,3,2,1);
INSERT INTO  test.t1 VALUES (3,1,1,1,1,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,b'101010',b'111111',b'000000',4,5,5,5,5,5,5,5,5,5,3,2,1);
INSERT INTO  test.t1 VALUES (4,1,1,1,1,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,b'0',1,1,4,5,5,5,5,5,5,5,5,5,3,2,1);
INSERT INTO  test.t1 VALUES (5,1,1,1,1,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,4,5,5,5,5,5,5,5,5,5,3,2,1);
INSERT INTO  test.t1 VALUES (7,1,1,1,1,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,4,5,5,5,5,5,5,5,5,5,3,2,1);
INSERT INTO  test.t1 VALUES (8,1,1,1,1,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,4,5,5,5,5,5,5,5,5,5,3,2,1);
UNLOCK TABLES;
UPDATE test.t1 set x034 = 50 where bit3 = b'000000';
UPDATE test.t1 set VNotSupp = 33 where bit1 = b'0';
SELECT  oSupp, sSuppD, GSuppDf, VNotSupp, x034
FROM test.t1
ORDER BY oSupp, sSuppD, GSuppDf, VNotSupp, x034;
oSupp	sSuppD	GSuppDf	VNotSupp	x034
5	5	3	NULL	1
5	5	3	2	1
5	5	3	2	50
5	5	3	33	1
5	5	3	33	1
5	5	3	33	1
5	5	3	33	1
5	5	3	33	1
SELECT hex(bit1) from test.t1 ORDER BY bit1;
hex(bit1)
0
0
0
0
0
2A
3F
3F
SELECT hex(bit2) from test.t1 ORDER BY bit2;
hex(bit2)
0
1
1
1
1
2A
3E
3F
SELECT hex(bit3) from test.t1 ORDER BY bit3;
hex(bit3)
0
1
1
1
1
15
24
35
include/sync_slave_sql_with_master.inc
SELECT  oSupp, sSuppD, GSuppDf, VNotSupp, x034
FROM test.t1
ORDER BY oSupp, sSuppD, GSuppDf, VNotSupp, x034;
oSupp	sSuppD	GSuppDf	VNotSupp	x034
5	5	3	NULL	1
5	5	3	2	1
5	5	3	2	50
5	5	3	33	1
5	5	3	33	1
5	5	3	33	1
5	5	3	33	1
5	5	3	33	1
SELECT hex(bit1) from test.t1 ORDER BY bit1;
hex(bit1)
0
0
0
0
0
2A
3F
3F
SELECT hex(bit2) from test.t1 ORDER BY bit2;
hex(bit2)
0
1
1
1
1
2A
3E
3F
SELECT hex(bit3) from test.t1 ORDER BY bit3;
hex(bit3)
0
1
1
1
1
15
24
35
CREATE TABLE test.t2 (a INT, b BIT(1));
INSERT INTO test.t2 VALUES (1, b'0');
INSERT INTO test.t2 VALUES (1, b'1');
UPDATE test.t2 SET a = 2 WHERE b = b'1';
CREATE TABLE test.t3 (a INT, b INT);
INSERT INTO test.t3 VALUES (1, NULL);
INSERT INTO test.t3 VALUES (1, 0);
UPDATE test.t3 SET a = 2 WHERE b = 0;
SELECT a, hex(b) FROM test.t2 ORDER BY a,b;
a	hex(b)
1	0
2	1
SELECT * FROM test.t3 ORDER BY a,b;
a	b
1	NULL
2	0
include/sync_slave_sql_with_master.inc
SELECT a, hex(b) FROM test.t2 ORDER BY a,b;
a	hex(b)
1	0
2	1
SELECT * FROM test.t3 ORDER BY a,b;
a	b
1	NULL
2	0
DROP TABLE IF EXISTS test.t1;
DROP TABLE IF EXISTS test.t2;
DROP TABLE IF EXISTS test.t3;
include/sync_slave_sql_with_master.inc
include/rpl_end.inc
