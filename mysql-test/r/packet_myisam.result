set @max_allowed_packet=@@global.max_allowed_packet;
set @net_buffer_length=@@global.net_buffer_length;
#
# Bug #20376498: MAX_ALLOWED_PACKET ERROR DESTROYS
#                ORIGINAL DATA
CREATE TABLE t3 (c31 INT NOT NULL, c32 LONGTEXT,
PRIMARY KEY (c31)) charset latin1 ENGINE=MYISAM;
CREATE TABLE t4 (c41 INT NOT NULL, c42 LONGTEXT,
PRIMARY KEY (c41)) charset latin1 ENGINE=MYISAM;
INSERT INTO t3 VALUES(100,'a');
INSERT INTO t3 VALUES(111,'abcd');
INSERT INTO t3 VALUES(122,'b');
INSERT INTO t4
SELECT c31, CONCAT(c32,
REPEAT('a', @max_allowed_packet-1))
FROM t3;
Warnings:
Warning	1301	Result of concat() was larger than max_allowed_packet (67108864) - truncated
SELECT c41, LENGTH(c42) FROM t4;
c41	LENGTH(c42)
100	67108864
111	NULL
122	67108864
UPDATE t3
SET c32= CONCAT(c32,
REPEAT('a', @max_allowed_packet-1));
ERROR HY000: Result of concat() was larger than max_allowed_packet (67108864) - truncated
SELECT c31, LENGTH(c32) FROM t3;
c31	LENGTH(c32)
100	67108864
111	4
122	1
DROP TABLE t3, t4;
