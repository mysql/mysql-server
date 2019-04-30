#
# BUG#46077 "wrong result: HAVING + ORDER BY + MyISAM + ICP
# returns extra rows"
#
CREATE TABLE `t1` (
`pk` int(11) NOT NULL AUTO_INCREMENT,
`int_key` int(11) NOT NULL,
PRIMARY KEY (`pk`),
KEY `int_key` (`int_key`)
) AUTO_INCREMENT=3 DEFAULT CHARSET=latin1;
Warnings:
Warning	1681	Integer display width is deprecated and will be removed in a future release.
Warning	1681	Integer display width is deprecated and will be removed in a future release.
INSERT INTO `t1` VALUES (1,7),(2,9);
SELECT `pk`, `int_key` field1
FROM t1 WHERE  `pk`  <  3
HAVING  field1  <  8
ORDER  BY field1;
pk	field1
1	7
drop table `t1`;
#
# Bug#13406172: HAVING CLAUSE ON AGGREGATED COLUMN RETURNS
#               WRONG RESULT WITH MYISAM
#
CREATE TABLE ot (i int) ENGINE=MyISAM;
INSERT INTO ot VALUES (1);
CREATE TABLE it (i int) ENGINE=MyISAM;
INSERT INTO it VALUES (7), (8);
SELECT COUNT(i) AS x
FROM ot
WHERE (i) IN (SELECT i FROM it WHERE it.i <= 4) OR i IS NULL
HAVING x > 1;
x
CREATE FUNCTION f(a INTEGER) RETURNS INTEGER DETERMINISTIC RETURN a*a;
SELECT BIT_AND(i) AS x
FROM ot
WHERE f(2) < 2
HAVING x < 2;
x
DROP TABLE it,ot;
DROP FUNCTION f;
# Bug#21067109: Assert 'join == __NULL' failed in ::optimize()
CREATE TABLE t1 (
col_int_key INT,
pk INT NOT NULL,
PRIMARY KEY (pk),
KEY col_int_key (col_int_key)
) ENGINE=MyISAM;
INSERT INTO t1 VALUES (6, 1);
CREATE TABLE t2 (
pk INT NOT NULL,
PRIMARY KEY (pk)
) ENGINE=MyISAM;
INSERT INTO t2 VALUES (5), (6), (7), (8);
SELECT col_int_key
FROM t1 AS alias1
HAVING (2, 7) NOT IN
(SELECT pk, SUM(pk)
FROM t1
WHERE pk <> (SELECT t2_alias1 .pk
FROM t2 AS t2_alias1 JOIN t2 AS t2_alias2
)
GROUP BY pk
);
ERROR 21000: Subquery returns more than 1 row
DROP TABLE t1, t2;
