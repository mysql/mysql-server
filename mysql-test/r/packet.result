set @max_allowed_packet=@@global.max_allowed_packet;
set @net_buffer_length=@@global.net_buffer_length;
set global max_allowed_packet=100;
Warnings:
Warning	1292	Truncated incorrect max_allowed_packet value: '100'
Warning	1708	The value of 'max_allowed_packet' should be no less than the value of 'net_buffer_length'
set global net_buffer_length=100;
Warnings:
Warning	1292	Truncated incorrect net_buffer_length value: '100'
SELECT length("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa") as len;
len
1024
select repeat('a',2000);
repeat('a',2000)
aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa
select @@net_buffer_length, @@max_allowed_packet;
@@net_buffer_length	@@max_allowed_packet
1024	1024
SELECT length("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa") as len;
ERROR 08S01: Got a packet bigger than 'max_allowed_packet' bytes
set global max_allowed_packet=default;
set global net_buffer_length=default;
SELECT length("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa") as len;
len
100
select length(repeat('a',2000));
length(repeat('a',2000))
2000
set global max_allowed_packet=@max_allowed_packet;
set global net_buffer_length=@net_buffer_length;
#
# Bug #20376498: MAX_ALLOWED_PACKET ERROR DESTROYS
#                ORIGINAL DATA
CREATE TABLE t1 (c11 INT NOT NULL, c12 LONGTEXT,
PRIMARY KEY (c11)) charset latin1;
CREATE TABLE t2 (c21 INT NOT NULL, c22 LONGTEXT,
PRIMARY KEY (c21)) charset latin1;
INSERT INTO t1 VALUES(100,'abcd');
INSERT INTO t2 VALUES(100,'xyz');
UPDATE t1
SET c12= REPEAT('ab', @max_allowed_packet);
ERROR HY000: Result of repeat() was larger than max_allowed_packet (67108864) - truncated
UPDATE IGNORE t1
SET c12= REPEAT('ab', @max_allowed_packet);
Warnings:
Warning	1301	Result of repeat() was larger than max_allowed_packet (67108864) - truncated
UPDATE t1, t2
SET c12= REPEAT('ab', @max_allowed_packet),
c22= 'ab';
ERROR HY000: Result of repeat() was larger than max_allowed_packet (67108864) - truncated
UPDATE IGNORE t1, t2
SET c12= REPEAT('ab', @max_allowed_packet),
c22= 'ab';
Warnings:
Warning	1301	Result of repeat() was larger than max_allowed_packet (67108864) - truncated
INSERT INTO t1
VALUES (101, REPEAT('ab', @max_allowed_packet));
ERROR HY000: Result of repeat() was larger than max_allowed_packet (67108864) - truncated
INSERT INTO t1
SELECT 101, REPEAT('ab', @max_allowed_packet);
ERROR HY000: Result of repeat() was larger than max_allowed_packet (67108864) - truncated
INSERT IGNORE INTO t1
SELECT 101, REPEAT('ab', @max_allowed_packet);
Warnings:
Warning	1301	Result of repeat() was larger than max_allowed_packet (67108864) - truncated
REPLACE INTO t1
VALUES (102, REPEAT('ab', @max_allowed_packet));
ERROR HY000: Result of repeat() was larger than max_allowed_packet (67108864) - truncated
REPLACE INTO t1
SET c11= 102,
c12= REPEAT('ab', @max_allowed_packet);
ERROR HY000: Result of repeat() was larger than max_allowed_packet (67108864) - truncated
REPLACE INTO t1
SELECT 102, REPEAT('ab', @max_allowed_packet);
ERROR HY000: Result of repeat() was larger than max_allowed_packet (67108864) - truncated
set names latin1;
DELETE FROM t1
WHERE c12 <=> REPEAT('ab', @max_allowed_packet);
ERROR HY000: Result of repeat() was larger than max_allowed_packet (67108864) - truncated
DELETE FROM t1, t2
USING t1 INNER JOIN t2
WHERE t1.c11 = t2.c21 AND
t2.c22 <=> REPEAT('ab', @max_allowed_packet);
ERROR HY000: Result of repeat() was larger than max_allowed_packet (67108864) - truncated
set names utf8mb4;
DELETE IGNORE FROM t1, t2
USING t1 INNER JOIN t2
WHERE t1.c11 = t2.c21 AND
t2.c22 <=> REPEAT('ab', @max_allowed_packet);
Warnings:
Warning	1301	Result of repeat() was larger than max_allowed_packet (67108864) - truncated
DELETE FROM t1;
DELETE FROM t2;
INSERT INTO t1 VALUES(100,'abcd');
INSERT INTO t2 VALUES(100,'xyz');
SET @@sql_mode= '';
SELECT @@sql_mode;
@@sql_mode

UPDATE t1
SET c12= REPEAT('ab', @max_allowed_packet);
Warnings:
Warning	1301	Result of repeat() was larger than max_allowed_packet (67108864) - truncated
SELECT c11, LENGTH(c12) FROM t1;
c11	LENGTH(c12)
100	NULL
INSERT INTO t1
VALUES (101, REPEAT('ab', @max_allowed_packet));
Warnings:
Warning	1301	Result of repeat() was larger than max_allowed_packet (67108864) - truncated
SELECT c11, LENGTH(c12) FROM t1;
c11	LENGTH(c12)
100	NULL
101	NULL
INSERT INTO t1
SELECT 102, REPEAT('ab', @max_allowed_packet);
Warnings:
Warning	1301	Result of repeat() was larger than max_allowed_packet (67108864) - truncated
SELECT c11, LENGTH(c12) FROM t1;
c11	LENGTH(c12)
100	NULL
101	NULL
102	NULL
DELETE FROM t1
WHERE c12 <=> REPEAT('ab', @max_allowed_packet);
Warnings:
Warning	1301	Result of repeat() was larger than max_allowed_packet (67108864) - truncated
Warning	1301	Result of repeat() was larger than max_allowed_packet (67108864) - truncated
Warning	1301	Result of repeat() was larger than max_allowed_packet (67108864) - truncated
SELECT c11, LENGTH(c12) FROM t1;
c11	LENGTH(c12)
SET @@sql_mode= default;
DROP TABLE t1, t2;
