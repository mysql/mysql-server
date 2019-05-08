#
# Bug #28560650 SERVER CRASH ON EXECUTING UPDATE QUERY USING
# REPEAT FUNC(SIGSEGV VIOLATION)
#
CREATE TABLE t1
( a INT PRIMARY KEY,
d INT NOT NULL,
b VARCHAR(5000) NOT NULL,
c VARCHAR(5000),
vcol1 INT AS (a + 3) VIRTUAL,
vcol2 INT AS (a + 5) VIRTUAL,
vcol3 INT AS (a + 7) VIRTUAL,
vcol4 INT AS (a + 7) VIRTUAL,
vadcol INT AS (a + LENGTH(d)) STORED,
vbcol CHAR(2) AS (SUBSTR(b,2,2)) VIRTUAL,
vbidxcol CHAR(3) AS (SUBSTR(b,1,3)) VIRTUAL,
INDEX(vcol1),
INDEX(vcol2),
INDEX(vcol3),
INDEX(vcol4),
INDEX(vbidxcol));
INSERT INTO t1 (a, d, b, c)
VALUES (1, 1, REPEAT('x', 4000), REPEAT('y', 4000));
INSERT INTO t1 (a, d, b, c)
VALUES (2, 1, REPEAT('i', 100), REPEAT('z', 5000));
SELECT a, d, LEFT(b, 10), LEFT(c, 10), vcol1, vcol2, vcol3, vcol4,
vadcol, vbcol, vbidxcol FROM t1;
a	d	LEFT(b, 10)	LEFT(c, 10)	vcol1	vcol2	vcol3	vcol4	vadcol	vbcol	vbidxcol
1	1	xxxxxxxxxx	yyyyyyyyyy	4	6	8	8	2	xx	xxx
2	1	iiiiiiiiii	zzzzzzzzzz	5	7	9	9	3	ii	iii
UPDATE t1
SET b = REPEAT('a', 4000),
vbidxcol = DEFAULT
WHERE a = 2;
SELECT a, d, LEFT(b, 10), LEFT(c, 10), vcol1, vcol2, vcol3, vcol4,
vadcol, vbcol, vbidxcol FROM t1;
a	d	LEFT(b, 10)	LEFT(c, 10)	vcol1	vcol2	vcol3	vcol4	vadcol	vbcol	vbidxcol
1	1	xxxxxxxxxx	yyyyyyyyyy	4	6	8	8	2	xx	xxx
2	1	aaaaaaaaaa	zzzzzzzzzz	5	7	9	9	3	aa	aaa
DROP TABLE t1;
