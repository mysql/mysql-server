--source include/force_myisam_default.inc
--source include/have_myisam.inc


# Const table behavior, table order is not changed, hint is applicable

CREATE TABLE t1(f1 INT) ENGINE=MyISAM;
INSERT INTO t1 VALUES (1);

CREATE TABLE t2(f1 INT) ENGINE=InnoDB;
INSERT INTO t2 VALUES (1);

EXPLAIN SELECT /*+ JOIN_PREFIX(t1, t2)  */ 1 FROM t1 JOIN t2 ON t1.f1 = t2.f1;
EXPLAIN SELECT /*+ JOIN_PREFIX(t2, t1)  */ 1 FROM t1 JOIN t2 ON t1.f1 = t2.f1;

DROP TABLE t1, t2; 

--echo #
--echo # Bug#23144274 WL9158:ASSERTION `JOIN->BEST_READ < DOUBLE(1.79769313486231570815E+308L)' FAILED
--echo #

CREATE TABLE t1
(
  f1 DATETIME,
  f2 DATE,
  f3 VARCHAR(1),
  KEY (f1)
) ENGINE=myisam;

CREATE TABLE t2
(
  f1 VARCHAR(1),
  f2 INT,
  f3 VARCHAR(1),
  KEY (f1)
) ENGINE=innodb;

CREATE TABLE t3
(
  f1 VARCHAR(1),
  f2 DATE,
  f3 DATETIME,
  f4 INT
) ENGINE=myisam;

EXPLAIN
UPDATE /*+ JOIN_ORDER(t2, als1, als3) JOIN_FIXED_ORDER()  */ t3 AS als1
   JOIN t1 AS als2 ON (als1.f3 = als2 .f1)
     JOIN t1 AS als3 ON (als1.f1 = als3.f3)
       RIGHT OUTER JOIN t3 AS als4 ON (als1.f3 = als4.f2)
SET als1.f4 = 'eogqjvbhzodzimqahyzlktkbexkhdwxwgifikhcgblhgswxyutepc'
WHERE ('i','b') IN (SELECT f3, f1 FROM t2 WHERE f2 <> f2 AND als2.f2 IS NULL);

DROP TABLE t1, t2, t3;

CREATE TABLE t1(
f1 VARCHAR(1)) ENGINE=myisam;

CREATE TABLE t2(
f1 VARCHAR(1),
f2 VARCHAR(1),
f3 DATETIME,
KEY(f2)) ENGINE=innodb;

CREATE TABLE t3(
f1 INT,
f2 DATE,
f3 VARCHAR(1),
KEY(f3)) ENGINE=myisam;

CREATE TABLE t4(
f1 VARCHAR(1),
KEY(f1)) ENGINE=innodb;
ALTER TABLE t4 DISABLE KEYS;
INSERT INTO t4 VALUES ('x'), (NULL), ('d'), ('x'), ('u');
ALTER TABLE t4 ENABLE KEYS;

CREATE TABLE t5(
f1 VARCHAR(1),
KEY(f1) ) ENGINE=myisam;
INSERT INTO t5 VALUES  (NULL), ('s'), ('c'), ('x'), ('z');

EXPLAIN UPDATE /*+ JOIN_ORDER(t4, alias1, alias3)  */ t3 AS alias1
  JOIN t5 ON (alias1.f3 = t5.f1)
    JOIN t3 AS alias3 ON (alias1.f2 = alias3.f2 )
      RIGHT OUTER JOIN t1 ON (alias1.f3 = t1.f1)
SET alias1.f1 = -1
WHERE ( 'v', 'o' )  IN
(SELECT DISTINCT t2.f1, t2.f2 FROM t4 RIGHT OUTER JOIN t2 ON (t4.f1 = t2.f1)
 WHERE t2.f3 BETWEEN '2001-10-04' AND '2003-05-15');

DROP TABLE t1, t2, t3, t4, t5;

CREATE TABLE t1 (
  f1 INT(11) DEFAULT NULL,
  f3 VARCHAR(1) DEFAULT NULL,
  f2 INT(11) DEFAULT NULL,
  KEY (f1)
) ENGINE=MyISAM;

CREATE TABLE t2(
  f1 INT(11) DEFAULT NULL
) ENGINE=MyISAM;

CREATE TABLE t3 (
  f1 VARCHAR(1) DEFAULT NULL,
  f2 VARCHAR(1) DEFAULT NULL,
  KEY (f2)
) ENGINE=InnoDB;


EXPLAIN UPDATE /*+ JOIN_SUFFIX(ta1, t2) */
  t1 AS ta1 JOIN t1 AS ta2 ON ta1.f1 = ta2.f1 RIGHT JOIN t2 ON (ta1.f1 = t2.f1)
SET ta1.f2 = '', ta2.f3 = ''
WHERE ('n', 'r') IN (SELECT f2, f1 FROM t3 WHERE f1 <> f2 XOR ta2.f3 IS NULL);

DROP TABLE t1, t2, t3;

CREATE TABLE t2(f1 VARCHAR(255) DEFAULT NULL, f2 INT(11) DEFAULT NULL,
  KEY (f1), KEY (f2)) charset latin1 ENGINE=MyISAM;

CREATE TABLE t4(f1 INT(11) DEFAULT NULL, f2 INT(11) DEFAULT NULL, KEY (f1))
charset latin1 ENGINE=MyISAM;
CREATE TABLE t5(f1 INT(11) NOT NULL AUTO_INCREMENT, f2 INT(11) DEFAULT NULL, PRIMARY KEY (f1))
charset latin1 ENGINE=InnoDB;
CREATE TABLE t6(f1 INT(11) NOT NULL AUTO_INCREMENT, PRIMARY KEY (f1))
charset latin1 ENGINE=InnoDB;
CREATE TABLE t7 (f1 VARCHAR(255) DEFAULT NULL)
charset latin1 ENGINE=InnoDB;

CREATE TABLE t10(f1 INT(11) NOT NULL AUTO_INCREMENT,f2 INT(11) DEFAULT NULL,f3 VARCHAR(10) DEFAULT NULL,
  PRIMARY KEY (f1),KEY (f2),KEY (f3)) charset latin1 ENGINE=MyISAM;

CREATE TABLE t11(f1 INT(11) DEFAULT NULL,f2 VARCHAR(10) DEFAULT NULL,
  KEY (f1),KEY (f2)) charset latin1 ENGINE=InnoDB;

EXPLAIN
SELECT /*+ JOIN_ORDER(alias11, alias8) */ 1
FROM t4 AS alias4
  LEFT JOIN t5 AS alias5 JOIN t6 AS alias6 ON alias5.f2 = alias6.f1
  LEFT JOIN t7 AS alias7 JOIN t2 AS alias8 ON alias7.f1 = alias8.f1
    ON alias5.f1 = alias8.f2 ON alias4.f2 = alias6.f1
  JOIN t10 AS alias10 JOIN t11 AS alias11 ON alias10.f1 = alias11.f1
    ON alias4.f2 = alias11.f2;

EXPLAIN
SELECT /*+ JOIN_ORDER(alias11, alias10, alias8, alias7) */ 1
FROM t4 AS alias4
  LEFT JOIN t5 AS alias5 JOIN t6 AS alias6 ON alias5.f2 = alias6.f1
  LEFT JOIN t7 AS alias7 JOIN t2 AS alias8 ON alias7.f1 = alias8.f1
    ON alias5.f1 = alias8.f2 ON alias4.f2 = alias6.f1
  JOIN t10 AS alias10 JOIN t11 AS alias11 ON alias10.f1 = alias11.f1
    ON alias4.f2 = alias11.f2;

DROP TABLES t2, t4, t5, t6, t7, t10, t11;

--echo #
--echo # Bug#23651098 WL#9158 : ASSERTION `!(SJ_NEST->SJ_INNER_TABLES & JOIN->CONST_TABLE_MAP)' FAILED
--echo #

CREATE TABLE t1
(
  f1 INT(11) NOT NULL AUTO_INCREMENT,
  PRIMARY KEY (f1)
) ENGINE=InnoDB;

CREATE TABLE t2
(
  f1 VARCHAR(1) DEFAULT NULL
) ENGINE=MyISAM;

CREATE TABLE t3
(
  f1 VARCHAR(1) DEFAULT NULL
) ENGINE=MyISAM;

EXPLAIN SELECT /*+ JOIN_PREFIX(t2, t1) */ t1.f1 FROM t1, t2
WHERE t2.f1 IN (SELECT t3.f1 FROM t3) AND t1.f1 = 183;

DROP TABLE t1, t2, t3;

