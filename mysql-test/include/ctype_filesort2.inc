#
# Testing filesort for full Unicode character sets
# with supplementary characters.
#

--echo #
--echo # Bug#55980 Character sets: supplementary character _bin ordering is wrong
--echo #
CREATE TABLE t1 AS SELECT REPEAT('a',1) AS a LIMIT 0;
SHOW CREATE TABLE t1;
INSERT INTO t1 VALUES (_utf8mb4 0xEFBE9D),(_utf8mb4 0xF0908E84);
INSERT INTO t1 VALUES (_utf8mb4 0xCE85),(_utf8mb4 0xF4808080);
SELECT HEX(a), HEX(CONVERT(a USING utf8mb4)) FROM t1 ORDER BY a;
ALTER TABLE t1 ADD KEY(a);
SELECT HEX(a), HEX(CONVERT(a USING utf8mb4)) FROM t1 ORDER BY a;
DROP TABLE IF EXISTS t1;
