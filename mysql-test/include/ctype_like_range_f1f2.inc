#
# Bug#32510 LIKE search fails with indexed 'eucjpms' and 'ujis' char column
#
# Testing my_ctype_like_range_xxx
# (used in LIKE optimization for an indexed column)
#

# Create table using @@character_set_connection and @@collation_connection
# for the string columns.

CREATE TABLE t1 AS
SELECT 10 AS a, REPEAT('a',20) AS b, REPEAT('a',8) AS c, REPEAT('a',8) AS d;
ALTER TABLE t1 ADD PRIMARY KEY(a), ADD KEY(b);

INSERT INTO t1 (a, b) VALUES (1, repeat(0xF1F2,5));
INSERT INTO t1 (a, b) VALUES (2, repeat(0xF1F2,10));
INSERT INTO t1 (a, b) VALUES (3, repeat(0xF1F2,11));
INSERT INTO t1 (a, b) VALUES (4, repeat(0xF1F2,12));

# Check pattern (important for ucs2, utf16, utf32)
SELECT hex(concat(repeat(0xF1F2, 10), '%'));

--echo 3 rows expected
SELECT a, hex(b), c FROM t1 WHERE b LIKE concat(repeat(0xF1F2,10), '%');
DROP TABLE t1;
