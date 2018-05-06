CREATE TABLE t1 (a CHAR(1)) CHARACTER SET utf8;
INSERT INTO t1 VALUES ('0'),('1'),('2'),('3'),('4'),('5'),('6'),('7');
INSERT INTO t1 VALUES ('8'),('9'),('A'),('B'),('C'),('D'),('E'),('F');

--disable_warnings
#
# Populate tables head and tail with values '00'-'FF'
#
CREATE TEMPORARY TABLE head   AS SELECT concat(b1.a, b2.a) AS head   FROM t1 b1, t1 b2;
CREATE TEMPORARY TABLE tail   AS SELECT concat(b1.a, b2.a) AS tail   FROM t1 b1, t1 b2;
CREATE TEMPORARY TABLE middle AS SELECT concat(b1.a, b2.a) AS middle FROM t1 b1, t1 b2;
DROP TABLE t1;

CREATE TABLE t1 (a varchar(1)) CHARACTER SET utf8;

#
# Populate single byte characters
#

INSERT INTO t1 SELECT UNHEX(head)
FROM head WHERE (head BETWEEN '00' AND '7F') ORDER BY head;

#
# Populate 2-byte byte characters: U+80..U+7FF:  [C2-DF][80-BF]
#
INSERT INTO t1
SELECT UNHEX(CONCAT(head,tail))
FROM head, tail
WHERE (head BETWEEN 'C2' AND 'DF') AND (tail BETWEEN '80' AND 'BF')
ORDER BY head, tail;


# 
# Populate 3-byte characters: U+800..U+FFFF: [E0-EF][80-BF][80-BF] 
# excluding overlong [E0][80-9F][80-BF] and surrogate code points
# [ED][A0-BF][80-BF]
#
INSERT INTO t1
SELECT UNHEX(CONCAT(head, middle, tail))
FROM head, middle, tail
WHERE (head BETWEEN 'E0' AND 'EF') 
AND (middle BETWEEN '80' AND 'BF')
AND (tail BETWEEN '80' AND 'BF')
AND NOT (head='E0' AND middle BETWEEN '80' AND '9F')
AND NOT (head='ED' AND middle BETWEEN 'A0' AND 'BF')
ORDER BY head, middle, tail;
--disable_warnings

SELECT count(*) FROM t1;
