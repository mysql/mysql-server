#
# Bug#32390 Character sets: casting utf32 to/from date doesn't work
#
CREATE TABLE t1 AS SELECT repeat('a',20) AS s1 LIMIT 0;
SET timestamp=1216359724;
INSERT INTO t1 VALUES (current_date);
INSERT INTO t1 VALUES (current_time);
INSERT INTO t1 VALUES (current_timestamp);
SELECT s1, hex(s1) FROM t1;
DROP TABLE t1;
SET timestamp=0;
