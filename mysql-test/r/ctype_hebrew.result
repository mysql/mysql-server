DROP TABLE IF EXISTS t1;
SET NAMES hebrew;
CREATE TABLE t1 (a char(1)) DEFAULT CHARSET=hebrew;
INSERT INTO t1 VALUES (0xFD),(0xFE);
ALTER TABLE t1 CONVERT TO CHARACTER SET utf8;
Warnings:
Warning	3719	'utf8' is currently an alias for the character set UTF8MB3, but will be an alias for UTF8MB4 in a future release. Please consider using UTF8MB4 in order to be unambiguous.
SELECT HEX(a) FROM t1;
HEX(a)
E2808E
E2808F
DROP TABLE t1;
End of 4.1 tests
