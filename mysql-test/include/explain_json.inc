# This file is a collection of regression and coverage tests
# for WL#5855: Print EXPLAIN in JSON format

--source include/have_innodb.inc

set @save_storage_engine= @@default_storage_engine;
set default_storage_engine=MyISAM;
set end_markers_in_json=on;

--echo # new "FORMAT" keyword doesn't conflict with the FORMAT() function name:

SELECT FORMAT(1, 2), FORMAT(1, 2, 3);

--echo # new "FORMAT" keyword is a valid identifier:

SET @FORMAT=10;
SELECT @FORMAT;

CREATE TABLE t1 (format INT);
SELECT format FROM t1;
DROP TABLE t1;

--echo # different ways of format name writing:

EXPLAIN FORMAT=traditional SELECT 1;
EXPLAIN FORMAT='TrAdItIoNaL' SELECT 1;
EXPLAIN FORMAT=JSON SELECT 1;

--error ER_UNKNOWN_EXPLAIN_FORMAT
EXPLAIN FORMAT=foo SELECT 1;

--echo # various EXPLAIN output

CREATE TABLE t1 (i INT);
CREATE TABLE t2 (i INT);
CREATE TABLE t3 (i INT);
CREATE TABLE t4 (i INT);

--echo # no end markers in JSON:

set end_markers_in_json=off;
EXPLAIN FORMAT=JSON SELECT * FROM t1;
set end_markers_in_json=on;

EXPLAIN             INSERT INTO t1 VALUES (10);
EXPLAIN FORMAT=JSON INSERT INTO t1 VALUES (10);

                   EXPLAIN             SELECT * FROM t1;
PREPARE stmt FROM 'EXPLAIN FORMAT=JSON SELECT * FROM t1';
EXECUTE stmt;
EXECUTE stmt;

INSERT INTO t1 VALUES (1), (2), (3), (4), (5), (6), (7);
INSERT INTO t2 VALUES (1), (2);

EXPLAIN
SELECT * FROM (SELECT * FROM (SELECT * FROM (SELECT a1.i FROM (SELECT * FROM t1) a1, t2) a2) a3) a4;
EXPLAIN FORMAT=JSON
SELECT * FROM (SELECT * FROM (SELECT * FROM (SELECT a1.i FROM (SELECT * FROM t1) a1, t2) a2) a3) a4;

--echo # subquery in WHERE
EXPLAIN             SELECT * FROM t1 WHERE i IN (SELECT i FROM t2 WHERE t1.i = 10 ORDER BY RAND());
EXPLAIN FORMAT=JSON SELECT * FROM t1 WHERE i IN (SELECT i FROM t2 WHERE t1.i = 10 ORDER BY RAND());

--echo # two subqueries in WHERE
EXPLAIN             SELECT * FROM t1
  WHERE i IN (SELECT i FROM t2 WHERE t1.i = 10 ORDER BY RAND())
     OR i IN (SELECT i FROM t4 ORDER BY RAND());
EXPLAIN FORMAT=JSON SELECT * FROM t1
  WHERE i IN (SELECT i FROM t2 WHERE t1.i = 10 ORDER BY RAND())
     OR i IN (SELECT i FROM t4 ORDER BY RAND());

--echo # simple UNION

EXPLAIN             SELECT * FROM t1 UNION SELECT * FROM t2 UNION SELECT * FROM t3;
EXPLAIN FORMAT=JSON SELECT * FROM t1 UNION SELECT * FROM t2 UNION SELECT * FROM t3;

--echo # more complex UNION

EXPLAIN             (SELECT t1.i FROM t1 JOIN t2) UNION ALL (SELECT * FROM t3 WHERE i IN (SELECT i FROM t4 ORDER BY RAND()));
EXPLAIN FORMAT=JSON (SELECT t1.i FROM t1 JOIN t2) UNION ALL (SELECT * FROM t3 WHERE i IN (SELECT i FROM t4 ORDER BY RAND()));

--echo # UNION with subquery in outer ORDER BY

EXPLAIN             (SELECT * FROM t1) UNION (SELECT * FROM t2) ORDER BY (SELECT i LIMIT 1);
EXPLAIN FORMAT=JSON (SELECT * FROM t1) UNION (SELECT * FROM t2) ORDER BY (SELECT i LIMIT 1);

--echo # optimizer-time subquery

--let $query=SELECT * FROM t1 ORDER BY (SELECT LENGTH(1) FROM t2 LIMIT 1)
--eval EXPLAIN $query
--eval EXPLAIN FORMAT=JSON $query

--echo # subquery in the HAVING clause

--let $query=SELECT * FROM t1 HAVING i > ALL (SELECT i FROM t2) OR i < ALL (SELECT i FROM t2);
--eval EXPLAIN $query
--eval EXPLAIN FORMAT=JSON $query

--echo # subquery in the GROUP BY clause

--let $query=SELECT * FROM t1 GROUP BY i > ALL (SELECT i FROM t2) OR i < ALL (SELECT i FROM t2);
--eval EXPLAIN $query
--eval EXPLAIN FORMAT=JSON $query

--echo # subquery in the SELECT list

--let $query=SELECT (SELECT i + 1 FROM t1 ORDER BY RAND() LIMIT 1), i FROM t1;
--eval EXPLAIN $query
--eval EXPLAIN FORMAT=JSON $query

DROP TABLE t1, t2, t3, t4;

--echo # derived table that is optimized out

CREATE TABLE t1 (i INT);
EXPLAIN SELECT 1 FROM (SELECT 1 AS x FROM t1) tt WHERE x;
EXPLAIN FORMAT= JSON SELECT 1 FROM (SELECT 1 AS x FROM t1) tt WHERE x;
DROP TABLE t1;

--echo # complex subqueries

CREATE TABLE t1 (a INT, b INT);
CREATE TABLE t2 (c INT, d INT);
CREATE TABLE t3 (e INT);
CREATE TABLE t4 (f INT, g INT);
INSERT INTO t1 VALUES (1,10), (2,10);
INSERT INTO t2 VALUES (2,10), (2,20);
INSERT INTO t3 VALUES (10), (30);
INSERT INTO t4 VALUES (2,10), (2,10);

--let $query = SELECT * FROM t1 WHERE t1.a IN (SELECT c FROM t2 WHERE (SELECT e FROM t3) < SOME(SELECT e FROM t3 WHERE t1.b));
--eval EXPLAIN $query
--eval EXPLAIN FORMAT=JSON $query

DROP TABLE t1, t2, t3, t4;

--echo # semi-join materialization (if enabled)

CREATE TABLE t1 (a INT);
INSERT INTO t1 VALUES (1), (1), (1);
CREATE TABLE t2 (a INT) SELECT * FROM t1;
CREATE TABLE t3 (a INT) SELECT * FROM t1;
CREATE TABLE t4 (a INT) SELECT * FROM t1;

EXPLAIN FORMAT=JSON
SELECT * FROM t1
WHERE t1.a IN (SELECT t2.a FROM t2 WHERE t2.a >  0) AND
      t1.a IN (SELECT t3.a FROM t3 WHERE t3.a IN
	       (SELECT t4.a FROM t4 WHERE a > 0));

DROP TABLE t1, t2, t3, t4;

--echo # the same subquery is associated with two different JOIN_TABs

CREATE TABLE t1 (
  i1 INTEGER NOT NULL,
  c1 VARCHAR(1) NOT NULL
) ENGINE=InnoDB;

INSERT INTO t1 VALUES (2,'w');

CREATE TABLE t2 (
  i1 INTEGER NOT NULL,
  c1 VARCHAR(1) NOT NULL,
  c2 VARCHAR(1) NOT NULL,
  KEY (c1, i1)
) ENGINE=InnoDB;

INSERT INTO t2 VALUES (8,'d','d');
INSERT INTO t2 VALUES (4,'v','v');

CREATE TABLE t3 (
  c1 VARCHAR(1) NOT NULL
) ENGINE=InnoDB;

INSERT INTO t3 VALUES ('v');

EXPLAIN FORMAT=json
SELECT i1
FROM t1
WHERE EXISTS (SELECT t2.c1
              FROM (t2 INNER JOIN t3 ON (t3.c1 = t2.c1)) 
              WHERE t2.c2 != t1.c1 AND t2.c2 = (SELECT MIN(t3.c1)
                                                FROM t3)); 

DROP TABLE t1, t2, t3;

--echo # multiple materialization groups

CREATE TABLE t1 (c_key INT, KEY c_key (c_key));
INSERT INTO t1 VALUES (1), (2), (3);
CREATE TABLE t2 (c INT, c_key INT);
INSERT INTO t2 VALUES (8,5),(4,5),(8,1);
CREATE TABLE t3 LIKE t1;
INSERT INTO t3 SELECT * FROM t1;
CREATE TABLE t4 LIKE t2;
INSERT INTO t4 SELECT * FROM t2;
CREATE TABLE t5 (c INT);
INSERT INTO t5 VALUES (1), (2), (3);

--echo # This should show two materialization groups where applicable

let $query=SELECT * FROM t5 
  WHERE c IN (SELECT t2.c FROM t1 JOIN t2 ON t2.c_key = t1.c_key) 
    AND c IN (SELECT t4.c FROM t3 JOIN t4 ON t4.c_key = t3.c_key);
--eval EXPLAIN $query
--eval EXPLAIN FORMAT=JSON $query

DROP TABLE t1, t2, t3, t4, t5;

CREATE TABLE t1 (i INT);
CREATE TABLE t2 (i INT);
CREATE TABLE t3 (i INT);

INSERT INTO t1 VALUES (1);
INSERT INTO t2 VALUES (1);
INSERT INTO t3 VALUES (1);

--echo # Subqueries in UPDATE values list

let $query=UPDATE t1 SET i=(SELECT i FROM t2);
--eval EXPLAIN $query
--eval EXPLAIN FORMAT=JSON $query
let $query=UPDATE t1, t2 SET t1.i=(SELECT i FROM t3);
--eval EXPLAIN $query
--eval EXPLAIN FORMAT=JSON $query

--echo # INSERT ... ON DUPLICATE KEY UPDATE x=(SELECT ...) value list

let $query=INSERT INTO t1 (i)
             SELECT i FROM t2 ON DUPLICATE KEY UPDATE i=(SELECT i FROM t2);
--eval EXPLAIN $query
--eval EXPLAIN FORMAT=JSON $query

let $query=INSERT INTO t1 VALUES (1)
             ON DUPLICATE KEY UPDATE i = (SELECT i FROM t2);
--eval EXPLAIN $query
--eval EXPLAIN FORMAT=JSON $query

--echo # Subqueries in INSERT VALUES tuples:

let $query=INSERT INTO t3 VALUES((SELECT i FROM t1)), ((SELECT i FROM t2));
--eval EXPLAIN $query
--eval EXPLAIN FORMAT=JSON $query

DROP TABLE t1, t2, t3;

--echo # Various queries

let $query=
  SELECT a, b FROM 
    (SELECT 1 AS a, 2 AS b
     UNION ALL
     SELECT 1 AS a, 2 AS b) t1
  GROUP BY a
  ORDER BY b DESC;
--eval EXPLAIN $query
--eval EXPLAIN FORMAT=JSON $query

--echo #

CREATE TABLE t1(a INT, b INT);
INSERT INTO t1 VALUES (), ();
let $query=SELECT 1 FROM t1 GROUP BY GREATEST(t1.a, (SELECT 1 FROM (SELECT t1.b FROM t1, t1 t2 ORDER BY t1.a, t1.a LIMIT 1) AS d));
--eval EXPLAIN $query
--eval EXPLAIN FORMAT=JSON $query
DROP TABLE t1;

--echo #

CREATE TABLE t1(f1 INT);
INSERT INTO t1 VALUES (1),(1);
let $query=SELECT 1 FROM t1 WHERE (SELECT (SELECT 1 FROM t1 GROUP BY f1));
--eval EXPLAIN $query
--eval EXPLAIN FORMAT=JSON $query
DROP TABLE t1;

--echo #

CREATE TABLE t1 (i INT);
CREATE TABLE t2 (i INT, j INT);
INSERT INTO t1 VALUES (1), (2), (3), (4), (5), (6), (7), (8), (9), (10);
INSERT INTO t2 SELECT i, i * 10 FROM t1;
let $query=SELECT * FROM t1 ORDER BY (SELECT t2.j FROM t2 WHERE t2.i = t1.i);
--eval EXPLAIN $query
--eval EXPLAIN FORMAT=JSON $query
let $query=SELECT * FROM t1 GROUP BY (SELECT t2.j FROM t2 WHERE t2.i = t1.i);
--eval EXPLAIN $query
--eval EXPLAIN FORMAT=JSON $query
DROP TABLE t1, t2;

CREATE TABLE t1 (a INT NOT NULL, b INT NOT NULL, PRIMARY KEY(a, b));
INSERT INTO t1 VALUES (10,1),(10,2),(10,3),(20,4),(20,5),(20,6),
  (30,7),(30,8),(30,9),(40,10),(40,11),(40,12),(40,13);

EXPLAIN FORMAT=JSON SELECT a, MIN(b) AS b FROM t1 GROUP BY a ORDER BY b;

DROP TABLE t1;

--echo #

CREATE TABLE t1 (a INT NOT NULL, b CHAR(3) NOT NULL, PRIMARY KEY (a));
INSERT INTO t1 VALUES (1,'ABC'), (2,'EFG'), (3,'HIJ');
CREATE TABLE t2 (a INT NOT NULL,b CHAR(3) NOT NULL,PRIMARY KEY (a, b));
INSERT INTO t2 VALUES (1,'a'),(1,'b'),(3,'F');

EXPLAIN FORMAT=JSON SELECT t1.a, GROUP_CONCAT(t2.b) AS b FROM t1 LEFT JOIN t2 ON t1.a=t2.a GROUP BY t1.a ORDER BY t1.b;

DROP TABLE t1;
DROP TABLE t2;

--echo #

CREATE TABLE t1 (a INT, b INT);

INSERT INTO t1 VALUES
  (1,4),
  (2,2), (2,2),
  (4,1), (4,1), (4,1), (4,1),
  (2,1), (2,1);

EXPLAIN FORMAT=JSON SELECT SUM(b) FROM t1 GROUP BY a WITH ROLLUP;

DROP TABLE t1;

--echo # Composition of DISTINCT, GROUP BY and ORDER BY

CREATE TABLE t1 (a INT, b INT);
INSERT INTO t1 VALUES (1, 1), (1, 2), (2, 1), (2, 2), (3, 1);

let $query=SELECT DISTINCT SUM(b) s FROM t1 GROUP BY a ORDER BY s;
--eval EXPLAIN FORMAT=JSON $query
FLUSH STATUS;
--eval $query
SHOW SESSION STATUS WHERE (Variable_name LIKE 'Sort_%' OR Variable_name LIKE 'Created_%_tables') AND Value > 0;

DROP TABLE t1;

--echo # "buffer_result" node

CREATE TABLE t1 (a INT NOT NULL);
CREATE TABLE t2 (a INT NOT NULL, PRIMARY KEY (a));
INSERT INTO t1 VALUES (1);
INSERT INTO t2 VALUES (1),(2);
EXPLAIN FORMAT=JSON SELECT SQL_BIG_RESULT DISTINCT t1.a FROM t1,t2 ORDER BY t2.a;
DROP TABLE t1, t2;

--echo #

CREATE TABLE t1 (a INT NOT NULL, b INT, PRIMARY KEY (a));
CREATE TABLE t2 (a INT NOT NULL, PRIMARY KEY (a));
INSERT INTO t1 VALUES (1,10), (2,20), (3,30),  (4,40);
INSERT INTO t2 VALUES (2), (3), (4), (5);
EXPLAIN FORMAT=JSON SELECT * FROM t2 WHERE t2.a IN (SELECT a FROM t1 WHERE t1.b <> 30);
DROP TABLE t1, t2;

set default_storage_engine= @save_storage_engine;
