# Test needs MyISAM storage engine
--source include/force_myisam_default.inc
--source include/have_myisam.inc

SET @sav_dpi= @@div_precision_increment;
SET div_precision_increment= 5;
SHOW VARIABLES LIKE 'div_precision_increment';


--echo # Bug#25727892: Refactor Item::const_item() as a non-virtual function

--echo Extended coverage for ROLLUP and const expressions

CREATE TABLE t1 (a INTEGER) ENGINE=InnoDB;
CREATE TABLE t2 (c INTEGER) ENGINE=MyISAM;

INSERT INTO t1 VALUES (7),(8);
INSERT INTO t2 VALUES (-3);

SELECT c, a, SUM(c) OVER () FROM t1,t2 GROUP BY c,a WITH ROLLUP;

DROP TABLE t1, t2;

--echo #
--echo # Bug#27530568: SIG11 IN FIELD::REAL_MAYBE_NULL IN SQL/FIELD.H
--echo #

CREATE TABLE t1 (pk INTEGER, PRIMARY KEY (pk))ENGINE= MYISAM;
CREATE TABLE t2 (pk INTEGER, PRIMARY KEY (pk))ENGINE= MYISAM;

INSERT INTO t2 VALUES (1),(2);

SELECT GROUPING(alias2.pk) AS field2 FROM t2 AS alias1 LEFT JOIN t1 AS alias2
  ON 0 GROUP BY alias2.pk WITH ROLLUP ORDER BY GROUPING(alias2.pk);

DROP TABLE t1,t2;

CREATE TABLE t1 (
pk INTEGER NOT NULL AUTO_INCREMENT,
col_varchar_key varchar(1) DEFAULT NULL,
PRIMARY KEY (pk),
KEY col_varchar_key (col_varchar_key)
);

INSERT INTO t1 VALUES (12,'a'),(16,'c'),(3,'d'),(20,'g'),(6,'h'),(15,'h'),
(4,'i'),(19,'l'),(7,'p'),(9,'p'),(17,'q'),(13,'r'),(5,'t'),(14,'u'),(8,'v'),
(10,'w'),(2,'x'),(18,'x'),(1,'z'),(11,'z');

CREATE TABLE t2(
pk INTEGER NOT NULL AUTO_INCREMENT,
PRIMARY KEY (pk)
) ENGINE=MyISAM AUTO_INCREMENT=2 DEFAULT CHARSET=utf8mb4;

INSERT INTO t2 VALUES (1);

SELECT  CONCAT(alias1.col_varchar_key,1) AS field1 FROM t1 AS alias1
 RIGHT JOIN t2 ON (alias1.pk = 1) GROUP BY field1 WITH ROLLUP HAVING
 field1 >= 'n' ORDER BY field1;

DROP TABLE t1,t2;

CREATE TABLE t1 (
  pk INTEGER NOT NULL AUTO_INCREMENT,
  col_int INTEGER DEFAULT NULL,
  PRIMARY KEY (pk)
) ENGINE=MyISAM AUTO_INCREMENT=2 DEFAULT CHARSET=utf8mb4;

INSERT INTO t1 VALUES (1,1);

SELECT ((table1.col_int) * (table1.col_int)) AS field2 FROM (t1 AS table1)
  WHERE (table1.col_int != 1 OR table1.pk) GROUP BY field2 WITH ROLLUP
  HAVING (field2 <> 239 ) ORDER BY GROUPING(field2);

DROP TABLE t1;
set div_precision_increment= @sav_dpi;

