
#
# BUG #24037: Lossy Hebrew to Unicode conversion
#
# Test if LRM and RLM characters are correctly converted to UTF-8
--disable_warnings
DROP TABLE IF EXISTS t1;
--enable_warnings

SET NAMES hebrew;
CREATE TABLE t1 (a char(1)) DEFAULT CHARSET=hebrew;
INSERT INTO t1 VALUES (0xFD),(0xFE);
ALTER TABLE t1 CONVERT TO CHARACTER SET utf8mb3;
SELECT HEX(a) FROM t1;
DROP TABLE t1;

--echo End of 4.1 tests
