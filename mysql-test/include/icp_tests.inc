--echo #
--echo # Bug#36981 - "innodb crash when selecting for update"
--echo #

#
# Test 1: Test based on the reproduction test case for this bug.
#         This query resulted in a crash in InnoDB due to
#         InnoDB changing from using the index which the push condition
#         where for to use the clustered index due to "SELECT ... FOR UPDATE".
#

CREATE TABLE t1 (
  c1 CHAR(1),
  c2 CHAR(10),
  KEY (c1)
);

INSERT INTO t1 VALUES ('3', null);

SELECT * FROM t1 WHERE c1='3' FOR UPDATE;

DROP TABLE t1;

#
# Test 2: Extended test case to test that the correct rows are returned.
#         This test is for ensuring that if InnoDB refuses to accept
#         the pushed index condition it is still evaluated.
#         

CREATE TABLE t1 (a INT);
INSERT INTO t1 VALUES (0),(1),(2),(3),(4),(5),(6),(7),(8),(9);

CREATE TABLE t2 (a INT);
INSERT INTO t2 SELECT A.a + 10*(B.a + 10*C.a) FROM t1 A, t1 B, t1 C;

CREATE TABLE t3 (
  c1 CHAR(10) NOT NULL,
  c2 CHAR(10) NOT NULL,
  c3 CHAR(200) NOT NULL,
  KEY (c1)
);

INSERT INTO t3 
  SELECT CONCAT('c-',1000+t2.a,'=w'), CONCAT('c-',1000+ t2.a,'=w'), 'filler'
  FROM t2;

INSERT INTO t3 
  SELECT CONCAT('c-',1000+t2.a,'=w'), CONCAT('c-',2000+t2.a,'=w'), 'filler-1'
  FROM t2;
 
INSERT INTO t3
  SELECT CONCAT('c-',1000+t2.a,'=w'), CONCAT('c-',3000+t2.a,'=w'), 'filler-2'
  FROM t2;

--sorted_result
SELECT c1,c3 FROM t3 WHERE c1 >= 'c-1994=w' and c1 != 'c-1996=w' FOR UPDATE;

DROP TABLE t1,t2,t3;

--echo #
--echo # Bug#42580 - Innodb's ORDER BY ..LIMIT returns no rows for
--echo #             null-safe operator <=> NULL
--echo #

CREATE TABLE t1(
  c1 DATE NOT NULL,
  c2 DATE NULL,
  c3 DATETIME,
  c4 TIMESTAMP,
  PRIMARY KEY(c1),
  UNIQUE(c2)
);

--echo
INSERT INTO t1 VALUES('0000-00-00', '0000-00-00', '2008-01-04', '2008-01-05');
INSERT INTO t1 VALUES('2007-05-25', '2007-05-25', '2007-05-26', '2007-05-26');
INSERT INTO t1 VALUES('2008-01-01', NULL        , '2008-01-02', '2008-01-03');
INSERT INTO t1 VALUES('2008-01-17', NULL        , NULL        , '2009-01-29');
INSERT INTO t1 VALUES('2009-01-29', '2009-01-29', '2009-01-29', '2009-01-29');

--echo
SELECT * FROM t1 WHERE c2 <=> NULL ORDER BY c1,c2;
--echo
SELECT * FROM t1 WHERE c2 <=> NULL ORDER BY c1,c2 LIMIT 2;

--echo
DROP TABLE t1;

--echo #
--echo # Bug#43617 - Innodb returns wrong results with timestamp's range value 
--echo #             in IN clause
--echo # (Note: Fixed by patch for BUG#42580)
--echo #

CREATE TABLE t1(
  c1 TIMESTAMP NOT NULL, 
  c2 TIMESTAMP NULL, 
  c3 DATE, 
  c4 DATETIME, 
  PRIMARY KEY(c1), 
  UNIQUE INDEX(c2)
);

INSERT INTO t1 VALUES
  ('0000-00-00 00:00:00','0000-00-00 00:00:00','2008-01-04','2008-01-05 00:00:00'),
  ('1971-01-01 00:00:01','1980-01-01 00:00:01','2009-01-01','2009-01-02 00:00:00'),
  ('1999-01-01 00:00:00','1999-01-01 00:00:00', NULL,        NULL),
  ('2007-05-23 09:15:28','2007-05-23 09:15:28','2007-05-24','2007-05-24 09:15:28'),
  ('2007-05-27 00:00:00','2007-05-25 00:00:00','2007-05-26','2007-05-26 00:00:00'),
  ('2008-01-01 00:00:00', NULL,                '2008-01-02','2008-01-03 00:00:00'),
  ('2009-01-29 11:11:27','2009-01-29 11:11:27','2009-01-29','2009-01-29 11:11:27'),
  ('2038-01-09 03:14:07','2038-01-09 03:14:07','2009-01-05','2009-01-06 00:00:00');

--echo
SELECT * 
FROM t1 
WHERE c2 IN ('1971-01-01 00:00:01','2038-01-09 03:14:07') 
ORDER BY c2;

--echo
SELECT * 
FROM t1 
WHERE c2 IN ('1971-01-01 00:00:01','2038-01-09 03:14:07') 
ORDER BY c2 LIMIT 2;

--echo
SELECT * 
FROM t1 
WHERE c2 IN ('1971-01-01 00:00:01','2038-01-09 03:14:07') 
ORDER BY c2 DESC;

--echo
SELECT * 
FROM t1 
WHERE c2 IN ('1971-01-01 00:00:01','2038-01-09 03:14:07') 
ORDER BY c2 DESC LIMIT 2;

--echo
DROP TABLE t1;

--echo #
--echo # BUG#43618: MyISAM&Maria returns wrong results with 'between' 
--echo #            on timestamp
--echo #

CREATE TABLE t1(
   ts TIMESTAMP NOT NULL, 
   c char NULL,
   PRIMARY KEY(ts)
);

INSERT INTO t1 VALUES
   ('1971-01-01','a'),
   ('2007-05-25','b'),
   ('2008-01-01','c'),
   ('2038-01-09','d');

--disable_warnings

--echo
--echo # Execute select with invalid timestamp, desc ordering
SELECT *
FROM t1 
WHERE ts BETWEEN '0000-00-00' AND '2010-00-01 00:00:00' 
ORDER BY ts DESC
LIMIT 2; 

--echo
--echo # Should use index condition
EXPLAIN
SELECT *
FROM t1 
WHERE ts BETWEEN '0000-00-00' AND '2010-00-01 00:00:00' 
ORDER BY ts DESC
LIMIT 2; 
--echo

--enable_warnings

DROP TABLE t1;

--echo #
--echo # BUG#49906: Assertion failed - Field_varstring::val_str in field.cc
--echo # (Note: Fixed by patch for LP BUG#625841)
--echo #

CREATE TABLE t1 ( 
  f1 VARCHAR(1024),
  f2 VARCHAR(10),
  INDEX test_idx USING BTREE (f2,f1(5))
);

INSERT INTO t1 VALUES  ('a','c'), ('b','d');  

SELECT f1
FROM t1 
WHERE f2 LIKE 'd' 
ORDER BY f1;

DROP TABLE t1;

--echo #
--echo # Bug#52660 - "Perf. regr. using ICP for MyISAM on range queries on 
--echo #              an index containing TEXT"
--echo #

CREATE TABLE t1 (a INT);
INSERT INTO t1 VALUES (0),(1),(2),(3),(4),(5),(6),(7),(8),(9);

CREATE TABLE t2 (a INT);
INSERT INTO t2 SELECT A.a + 10*(B.a) FROM t1 A, t1 B;

CREATE TABLE t3 (
  c1 TINYTEXT NOT NULL,
  i1 INT NOT NULL,
  KEY (c1(6),i1)
);

INSERT INTO t3 SELECT CONCAT('c-',1000+t2.a,'=w'), 1 FROM t2;

EXPLAIN
SELECT c1 FROM t3 WHERE c1 >= 'c-1004=w' and c1 <= 'c-1006=w';
SELECT c1 FROM t3 WHERE c1 >= 'c-1004=w' and c1 <= 'c-1006=w';

EXPLAIN
SELECT c1 FROM t3 WHERE c1 >= 'c-1004=w' and c1 <= 'c-1006=w' and i1 > 2;
SELECT c1 FROM t3 WHERE c1 >= 'c-1004=w' and c1 <= 'c-1006=w' and i1 > 2;

EXPLAIN
SELECT c1 FROM t3 WHERE c1 >= 'c-1004=w' and c1 <= 'c-1006=w' or i1 > 2;
SELECT c1 FROM t3 WHERE c1 >= 'c-1004=w' and c1 <= 'c-1006=w' or i1 > 2;

DROP TABLE t1, t2, t3;

--echo #
--echo # Bug#40992 - InnoDB: Crash when engine_condition_pushdown is on
--echo #

CREATE TABLE t (
  dummy INT PRIMARY KEY, 
  a INT UNIQUE, 
  b INT
);

INSERT INTO t VALUES (1,1,1),(3,3,3),(5,5,5);

SELECT * FROM t WHERE a > 2 FOR UPDATE;

DROP TABLE t;


--echo #
--echo # Bug#35080 - Innodb crash at mem_block_get_len line 72
--echo #

CREATE TABLE t1 (
  t1_autoinc INT(11) NOT NULL AUTO_INCREMENT,
  uuid VARCHAR(36) DEFAULT NULL,
  PRIMARY KEY (t1_autoinc),
  KEY k (uuid)
);

CREATE TABLE t2 (
  t2_autoinc INT(11) NOT NULL AUTO_INCREMENT,
  uuid VARCHAR(36) DEFAULT NULL,
  date DATETIME DEFAULT NULL,
  PRIMARY KEY (t2_autoinc),
  KEY k (uuid)
);

CREATE VIEW v1 AS 
  SELECT t1_autoinc, uuid
  FROM t1
  WHERE (ISNULL(uuid) OR (uuid like '%-%'));

CREATE VIEW v2 AS 
  SELECT t2_autoinc, uuid, date 
  FROM t2
  WHERE (ISNULL(uuid) OR (LENGTH(uuid) = 36));

CREATE PROCEDURE delete_multi (IN uuid CHAR(36))
  DELETE v1, v2 FROM v1 INNER JOIN v2
  ON v1.uuid = v2.uuid
  WHERE v1.uuid = @uuid;

SET @uuid = UUID();

INSERT INTO v1 (uuid) VALUES (@uuid);
INSERT INTO v2 (uuid, date) VALUES (@uuid, '2009-09-09');

CALL delete_multi(@uuid);

DROP procedure delete_multi;
DROP table t1,t2;
DROP view v1,v2;

--echo #
--echo # Bug#41996 - multi-table delete crashes server (InnoDB table)
--echo #

CREATE TABLE t1 (
  b BIGINT,
  i INT, 
  KEY (b)
);

INSERT INTO t1 VALUES (2, 2);

DELETE t1 FROM t1 a, t1 WHERE a.i=t1.b;

DROP TABLE t1;

--echo #
--echo # Bug#43448 - Server crashes on multi table delete with Innodb
--echo #

CREATE TABLE t1 (
  id1 INT NOT NULL AUTO_INCREMENT PRIMARY KEY, 
  t CHAR(12)
);

CREATE TABLE t2 (
  id2 INT NOT NULL, 
  t CHAR(12)
);

CREATE TABLE t3(
  id3 INT NOT NULL, 
  t CHAR(12), 
  INDEX(id3)
);

delimiter |;

CREATE PROCEDURE insert_data ()
BEGIN
  DECLARE i1 INT DEFAULT 20;
  DECLARE i2 INT;
  DECLARE i3 INT;

  WHILE (i1 > 0) DO
    INSERT INTO t1(t) VALUES (i1);
    SET i2 = 2;
    WHILE (i2 > 0) DO
      INSERT INTO t2(id2, t) VALUES (i1, i2);
      SET i3 = 2;
      WHILE (i3 > 0) DO
        INSERT INTO t3(id3, t) VALUES (i1, i2);
        SET i3 = i3 -1;
      END WHILE;
      SET i2 = i2 -1;
    END WHILE;
    SET i1 = i1 - 1;
  END WHILE;
END |

delimiter ;|

CALL insert_data();

SELECT COUNT(*) FROM t1 WHERE id1 > 10;
SELECT COUNT(*) FROM t2 WHERE id2 > 10;
SELECT COUNT(*) FROM t3 WHERE id3 > 10;

DELETE t1, t2, t3 
FROM t1, t2, t3 
WHERE t1.id1 = t2.id2 AND t2.id2 = t3.id3 AND t1.id1 > 3;

SELECT COUNT(*) FROM t1;
SELECT COUNT(*) FROM t2;
SELECT COUNT(*) FROM t3;

DROP PROCEDURE insert_data;
DROP TABLE t1, t2, t3;

--echo #
--echo # Bug#57372 "Multi-table updates and deletes fail when running with ICP 
--echo #            against InnoDB"
--echo #

CREATE TABLE t1 (
  a INT KEY, 
  b INT
);

CREATE TABLE t2 (
  a INT KEY, 
  b INT
);

INSERT INTO t1 VALUES (1, 101), (2, 102), (3, 103), (4, 104), (5, 105);
INSERT INTO t2 VALUES (1, 1), (2, 2), (3, 3), (4, 4), (5, 5);

UPDATE t1, t2 
SET t1.a = t1.a + 100, t2.b = t1.a + 10 
WHERE t1.a BETWEEN 2 AND 4 AND t2.a = t1.b - 100;

--sorted_result
SELECT * FROM t1;
--sorted_result
SELECT * FROM t2;

DROP TABLE t1, t2;

--echo #
--echo # Bug#52605 - "Adding LIMIT 1 clause to query with complex range 
--echo #              predicate causes wrong results"
--echo #

CREATE TABLE t1 (
  pk INT NOT NULL,
  c1 INT,
  PRIMARY KEY (pk),
  KEY k1 (c1)
);

INSERT INTO t1 VALUES (1,NULL);
INSERT INTO t1 VALUES (2,6);
INSERT INTO t1 VALUES (3,NULL);
INSERT INTO t1 VALUES (4,6);
INSERT INTO t1 VALUES (5,NULL);
INSERT INTO t1 VALUES (6,NULL);
INSERT INTO t1 VALUES (7,9);
INSERT INTO t1 VALUES (8,0);

SELECT pk, c1
FROM t1  
WHERE (pk BETWEEN 4 AND 5 OR pk < 2) AND c1 < 240
ORDER BY c1
LIMIT 1;

EXPLAIN SELECT pk, c1
FROM t1  
WHERE (pk BETWEEN 4 AND 5 OR pk < 2) AND c1 < 240
ORDER BY c1
LIMIT 1;

DROP TABLE t1;

--echo #
--echo #
--echo # Bug#59259 "Incorrect rows returned for a correlated subquery
--echo #            when ICP is on"
--echo #

CREATE TABLE t1 (pk INTEGER PRIMARY KEY, i INTEGER NOT NULL) ENGINE=InnoDB;

INSERT INTO t1 VALUES (11,0);
INSERT INTO t1 VALUES (12,5);
INSERT INTO t1 VALUES (15,0);

CREATE TABLE t2 (pk INTEGER PRIMARY KEY, i INTEGER NOT NULL) ENGINE=InnoDB;

INSERT INTO t2 VALUES (11,1);
INSERT INTO t2 VALUES (12,2);
INSERT INTO t2 VALUES (15,4);

set @save_optimizer_switch= @@optimizer_switch;
set optimizer_switch='semijoin=off';

EXPLAIN
SELECT * FROM t1
WHERE pk IN (SELECT it.pk FROM t2 JOIN t2 AS it ON it.i=it.i WHERE it.pk-t1.i<10);
SELECT * FROM t1
WHERE pk IN (SELECT it.pk FROM t2 JOIN t2 AS it ON it.i=it.i WHERE it.pk-t1.i<10);

set optimizer_switch=@save_optimizer_switch;

DROP TABLE t1, t2;

--echo #
--echo # Bug #58816 "Extra temporary duplicate rows in result set when 
--echo #             switching ICP off"
--echo #

set @save_optimizer_switch= @@optimizer_switch;

CREATE TABLE t1 (
  pk INT NOT NULL,
  c1 INT NOT NULL,
  PRIMARY KEY (pk)
);

INSERT INTO t1 VALUES (1,9),(2,7),(3,6),(4,3),(5,1);

EXPLAIN SELECT pk, c1 FROM t1 WHERE pk <> 3;

SET SESSION optimizer_switch='index_condition_pushdown=off';

SELECT pk, c1 FROM t1 WHERE pk <> 3;

DROP TABLE t1;

set optimizer_switch= @save_optimizer_switch;

--echo #
--echo # Bug#58837: ICP crash or valgrind error due to uninitialized 
--echo #            value in innobase_index_cond
--echo #

CREATE TABLE t1 (
  t1_int INT,
  t1_time TIME
);

CREATE TABLE t2 ( 
  t2_int int PRIMARY KEY,
  t2_int2 INT
);

--disable_warnings
INSERT INTO t2 VALUES ();
INSERT INTO t1 VALUES ();
--enable_warnings

SELECT * FROM t1 AS t1a 
  WHERE NOT EXISTS (SELECT * FROM t1 AS t1b
                      WHERE t1b.t1_int NOT IN
                            (SELECT t2.t2_int FROM t2
                               WHERE t1b.t1_time LIKE t1b.t1_int
                                     OR t1b.t1_time <> t2.t2_int2
                                        AND 6=7));

DROP TABLE t1,t2;

--echo #
--echo # Bug#59186: Wrong results of join when ICP is enabled
--echo #            (fixed by the patch for LP bug #694092)
--echo #

CREATE TABLE t1 (
  pk INTEGER NOT NULL,
  c1 VARCHAR(3) NOT NULL,
  PRIMARY KEY (pk)
);
INSERT INTO t1 VALUES (1,'y'),(0,'or');

CREATE TABLE t2 (
  pk INTEGER NOT NULL,
  c1 VARCHAR(3) NOT NULL,
  c2 VARCHAR(6) NOT NULL,
  PRIMARY KEY (pk)
);
INSERT INTO t2 VALUES (6,'y','RPOYT'),(10,'m','JINQE');

EXPLAIN
SELECT c2 FROM t1 JOIN t2 ON t1.c1 = t2.c1
WHERE (t2.pk <= 4 AND t1.pk IN (2,1)) OR
      (t1.pk > 1 AND t2.pk BETWEEN 6 AND 6);
SELECT c2 FROM t1 JOIN t2 ON t1.c1 = t2.c1
WHERE (t2.pk <= 4 AND t1.pk IN (2,1)) OR
      (t1.pk > 1 AND t2.pk BETWEEN 6 AND 6);

DROP TABLE t1, t2;

--echo #
--echo # Bug#58838: "Wrong results with HAVING + LIMIT without GROUP BY when 
--echo #            ICP is enabled".
--echo #            (Fixed by the patches for LP bugs #668644, #702322)
--echo # 

CREATE TABLE t1 (
  pk INT NOT NULL,
  c1 INT,
  PRIMARY KEY (pk),
  KEY col_int_key (c1)
);

INSERT INTO t1 VALUES (1,37),(2,8),(3,-25),(4,NULL),(5,55);

SELECT pk FROM t1 WHERE c1 <> 1 HAVING pk = 3 ORDER BY pk LIMIT 0;
SELECT pk FROM t1 WHERE c1 <> 1 HAVING pk = 3 ORDER BY pk LIMIT 1;
SELECT pk FROM t1 WHERE c1 <> 1 HAVING pk = 3 ORDER BY pk LIMIT 2;
SELECT pk FROM t1 WHERE c1 <> 1 HAVING pk = 3 ORDER BY pk LIMIT 5;

DROP TABLE t1;

--echo #
--echo # Bug#59483 "Crash on INSERT/REPLACE in
--echo #            rec_convert_dtuple_to_rec_comp with ICP on"
--echo #

CREATE TABLE t1 (
 pk INTEGER AUTO_INCREMENT PRIMARY KEY,
 i1 INTEGER,
 c1 CHAR(6),
 i2 INTEGER NOT NULL,
 KEY (i2)
);

INSERT INTO t1 VALUES
 (NULL, 4, 'that', 8),
 (NULL, 1, 'she', 6),
 (NULL, 6, 'tell', 2);

SELECT * FROM t1 WHERE i2 IN (3, 6) LIMIT 2 FOR UPDATE;
INSERT INTO t1 (i2) VALUES (1);

DROP TABLE t1;

--echo #
--echo # Bug #11766678 - 59843:
--echo # USING UNINITIALISED VALUE IN USES_INDEX_FIELDS_ONLY
--echo #

CREATE TABLE t1 (
  col999 FLOAT NOT NULL,
  COL1000 VARBINARY(179) NOT NULL,
  col1003 DATE DEFAULT NULL,
  KEY idx4267 (col1000, col1003)
);

INSERT INTO t1 VALUES (),();
SELECT col999 FROM t1 WHERE col1000 = "3" AND col1003 <=> sysdate();

DROP TABLE t1;

--echo #
--echo # BUG#12822678 - ICP WITH STRAIGHT_JOIN
--echo #

CREATE TABLE t1 (
  i1 INTEGER NOT NULL,
  d1 DOUBLE,
  KEY k1 (d1)
);
INSERT INTO t1 VALUES (10,1), (17,NULL), (22,NULL);

CREATE TABLE t2 (
  pk INTEGER NOT NULL,
  i1 INTEGER NOT NULL,
  PRIMARY KEY (pk)
);
INSERT INTO t2 VALUES (4,1);

EXPLAIN
SELECT t1.d1, t2.pk, t2.i1 FROM t1 STRAIGHT_JOIN t2 ON t2.i1
  WHERE t2.pk <> t1.d1 AND t2.pk = 4;
SELECT t1.d1, t2.pk, t2.i1 FROM t1 STRAIGHT_JOIN t2 ON t2.i1
  WHERE t2.pk <> t1.d1 AND t2.pk = 4;

DROP TABLE t1, t2;

--echo #
--echo # BUG#778434 Wrong result with in_to_exists=on in maria-5.3-mwl89
--echo #
CREATE TABLE t1 ( f11 int) ;
INSERT IGNORE INTO t1 VALUES (0);

CREATE TABLE t2 ( f10 int) ;
INSERT IGNORE INTO t2 VALUES (0);

CREATE TABLE t3 ( f1 int NOT NULL , f10 int, PRIMARY KEY (f1)) ;
INSERT IGNORE INTO t3 VALUES (6,0),(10,0);

CREATE TABLE t4 ( f11 int) ;
INSERT IGNORE INTO t4 VALUES
(0),(0),(0),(0),(0),(0),(0),(0),(0),(0),(NULL),
(0),(0),(0),(0),(0),(0),(0),(0),(0),(0);

set @tmp_778434=@@optimizer_switch;
SET optimizer_switch='materialization=off,in_to_exists=on,subquery_cache=off,semijoin=off';

SELECT * FROM t1 INNER JOIN t2 ON t2.f10 = t1.f11
WHERE (6, 234) IN (
        SELECT t3.f1, t3.f1
        FROM t3 JOIN t4 ON t4.f11 = t3.f10
);

DROP TABLE t1,t2,t3,t4;
set optimizer_switch= @tmp_778434;

--echo #
--echo # Bug#885168: ICP for one index + potential ORDER BY for another
--echo #

CREATE TABLE t1 (a varchar(64), b varchar(10), INDEX(a), INDEX(b)) ;
INSERT INTO t1 VALUES 
  ('Ohio','Iowa'), ('k','d'), ('bdkpj','mbdkpjdanp'), ('d','xdmbdkpjda'),
  ('fkxdmbdkpjdanpje','o'), ('f','Pennsylvan'), ('Virginia','ei');

SET SESSION optimizer_switch='index_condition_pushdown=off';
EXPLAIN
SELECT * FROM t1 
  WHERE NOT(b = 'Texas') AND b BETWEEN 'wy' AND 'y' OR b = 'Pennsylvania'
  ORDER BY a;
SELECT * FROM t1 
  WHERE NOT(b = 'Texas') AND b BETWEEN 'wy' AND 'y' OR b = 'Pennsylvania'
  ORDER BY a;

SET SESSION optimizer_switch='index_condition_pushdown=on';
EXPLAIN
SELECT * FROM t1 
  WHERE NOT(b = 'Texas') AND b BETWEEN 'wy' AND 'y' OR b = 'Pennsylvania'
  ORDER BY a;
SELECT * FROM t1 
  WHERE NOT(b = 'Texas') AND b BETWEEN 'wy' AND 'y' OR b = 'Pennsylvania'
  ORDER BY a;

DROP TABLE t1;

--echo #
--echo # Bug#886145: join with ICP + ORDER BY  
--echo #

CREATE TABLE t1 (b int NOT NULL, c int, a varchar(1024), PRIMARY KEY (b));
INSERT INTO t1 VALUES (1,4,'Ill');

CREATE TABLE t2 (a varchar(1024), KEY (a(512)));
INSERT INTO t2 VALUES
  ('Ill'), ('eckqzsflbzaffti'), ('w'), ('she'), ('gxbwypqtjzwywwer'), ('w');

SET SESSION optimizer_switch='index_condition_pushdown=off';
EXPLAIN
SELECT t1.b, t1.c FROM t1, t2 WHERE t1.a = t2.a AND t1.b != 0 
  HAVING t1.c != 5 ORDER BY t1.c;
SELECT t1.b, t1.c FROM t1, t2 WHERE t1.a = t2.a AND t1.b != 0
  HAVING t1.c != 5 ORDER BY t1.c;

SET SESSION optimizer_switch='index_condition_pushdown=on';
EXPLAIN
SELECT t1.b, t1.c FROM t1, t2 WHERE t1.a = t2.a AND t1.b != 0
  HAVING t1.c != 5 ORDER BY t1.c;
SELECT t1.b, t1.c FROM t1, t2 WHERE t1.a = t2.a AND t1.b != 0
  HAVING t1.c != 5 ORDER BY t1.c;

DROP TABLE t1,t2;

--echo #
--echo # Bug#879871: InnoDB: possible ICP + GROUP BY primary index  
--echo #

CREATE TABLE t1 (
  a int NOT NULL, b int, c varchar(1), d varchar(1),
 PRIMARY KEY (a), KEY c (c,b)
);
INSERT INTO t1 VALUES (10,8,'g','g');

SET SESSION optimizer_switch='index_condition_pushdown=off';
SELECT a FROM t1 WHERE c IS NULL AND d IS NOT NULL GROUP BY 1;

SET SESSION optimizer_switch='index_condition_pushdown=on';
SELECT a FROM t1 WHERE c IS NULL AND d IS NOT NULL GROUP BY 1;

DROP TABLE t1;


--echo #
--echo # Bug#904832: an attempt to perform an index condition pushdown  
--echo #             of a condition containing a subquery
--echo #

CREATE TABLE t1 ( 
  a INT PRIMARY KEY NOT NULL AUTO_INCREMENT,
  b INT, c INT, d DATE NOT NULL, e VARCHAR(1),
  KEY (c), KEY (d), KEY k2(b), KEY k3(b), KEY k4(b)
);

INSERT INTO t1 (b,c,d,e) VALUES
(6,5,'2006-05-25','y'),(1,5,'2008-01-23','t'),
(6,5,'2007-06-18','d'),(4,5,'1900-01-01','r'),
(8,5,'1900-01-01','m'),(4,5,'2006-03-09','b'),
(4,5,'2001-06-05','x'),(1,5,'2008-01-23','t'),
(6,5,'2007-06-18','d'),(4,5,'1900-01-01','r'),
(8,5,'1900-01-01','m'),(4,5,'2006-03-09','b'),
(4,5,'2001-06-05','x'),(1,5,'2008-01-23','t'),
(4,5,'1900-01-01','r'),(8,5,'1900-01-01','m'),
(4,5,'2006-03-09','b'),(4,5,'2001-06-05','x'),
(1,5,'2008-01-23','t'),(6,5,'2007-06-18','d'),
(4,5,'1900-01-01','r'),(8,5,'1900-01-01','m'),
(4,5,'2006-03-09','b'),(4,5,'2001-06-05','f'),
(4,5,'2001-06-05','x'),(8,5,'1900-01-01','m'),
(4,5,'2006-03-09','b'),(4,5,'2001-06-05','x'),
(1,5,'2008-01-23','t'),(6,5,'2007-06-18','d'),
(4,5,'1900-01-01','r'),(8,5,'1900-01-01','m'),
(4,5,'2006-03-09','b'),(6,5,'2007-06-18','d'),
(6,5,'2007-06-18','d'),(4,5,'1900-01-01','r'),
(8,5,'1900-01-01','m'),(4,5,'2006-03-09','b'),
(6,5,'2007-06-18','d'),(4,5,'1900-01-01','r'),
(8,5,'1900-01-01','m'),(4,5,'2006-03-09','b'),
(4,5,'2006-03-09','b'),(4,5,'2001-06-05','x'),
(1,5,'2008-01-23','t'),(6,5,'2007-06-18','d'),
(4,5,'1900-01-01','r'),(8,5,'1900-01-01','m'),
(4,5,'2006-03-09','b'),(4,5,'2001-06-05','x'),
(4,5,'2001-06-05','x'),(1,5,'2008-01-23','t'),
(6,5,'2007-06-18','d'),(4,5,'1900-01-01','r'),
(8,5,'1900-01-01','m'),(4,5,'2006-03-09','b'),
(4,5,'2001-06-05','x'),(1,5,'2008-01-23','t'),
(6,5,'2007-06-18','d'),(4,1,'1900-01-01','r'),
(8,8,'1900-01-01','m'),(4,1,'2006-03-09','b'),
(4,1,'2001-06-05','x'),(7,1,'2006-05-28','g');

CREATE TABLE t2 (
  f INT PRIMARY KEY NOT NULL AUTO_INCREMENT,
  g INT,
  h VARCHAR(1),
  KEY (g)
);

INSERT INTO t2 (g,h) VALUES
(3,'e'),(1,'u'),(4,'v'),(9,'u'),(6,'i'),(1,'x'),
(5,'l'),(8,'y'),(0,'p'),(0,'f'),(0,'p'),(7,'d'),
(7,'f'),(5,'j'),(3,'e'),(1,'u'),(4,'v'),(9,'u'),
(6,'i'),(1,'x'),(5,'l'),(8,'y'),(0,'p'),(0,'f'),
(0,'p'),(0,'f'),(0,'p'),(7,'d'),(7,'f'),(5,'j'),
(3,'e'),(1,'u'),(4,'v'),(9,'u'),(6,'i'),(1,'x'),
(7,'d'),(7,'f'),(5,'j'),(3,'e'),(1,'u'),(4,'v'),
(9,'u'),(6,'i'),(1,'x'),(5,'l'),(8,'y'),(0,'p'),
(0,'f'),(0,'p'),(7,'d'),(7,'f'),(5,'j'),(3,'e'),
(1,'u'),(4,'v'),(9,'u'),(6,'i'),(1,'x'),(5,'l'),
(6,'q'),(2,'n'),(4,'r'),(4,'b'),(8,'y'),(0,'p'),
(0,'f'),(0,'p'),(7,'d'),(7,'f'),(5,'j'),(3,'e'),
(1,'u'),(4,'v'),(9,'u'),(6,'i'),(1,'x'),(5,'l'),
(8,'y'),(0,'p'),(0,'f'),(0,'p'),(7,'d'),(7,'f'),
(5,'j'),(3,'e'),(1,'u'),(4,'v'),(9,'u'),(6,'i'),
(1,'x'),(5,'l'),(8,'y'),(0,'p'),(0,'f'),(0,'p'),
(0,'f'),(0,'p'),(7,'d'),(7,'f'),(5,'j'),(3,'e'),
(1,'u'),(4,'v'),(9,'u'),(6,'i'),(1,'x'),(7,'d'),
(7,'f'),(5,'j'),(3,'e'),(1,'u'),(0,'f'),(0,'p'),
(7,'d'),(7,'f'),(5,'j'),(3,'e'),(1,'u'),(4,'v'),
(9,'u'),(6,'i'),(1,'x'),(5,'l'),(8,'y'),(0,'p'),
(0,'f'),(0,'p'),(0,'f'),(0,'p'),(7,'d'),(7,'f'),
(5,'j'),(3,'e'),(1,'u'),(4,'v'),(9,'u'),(6,'i'),
(1,'x'),(7,'d'),(7,'f'),(5,'j'),(3,'e'),(1,'u'),
(4,'v'),(9,'u'),(6,'i'),(1,'x'),(5,'l'),(8,'y'),
(0,'p'),(0,'f'),(0,'p'),(7,'d'),(7,'f'),(5,'j'),
(3,'e'),(1,'u'),(4,'v'),(9,'u'),(6,'i'),(1,'x'),
(5,'l'),(6,'q'),(2,'n'),(4,'r'),(4,'b'),(8,'y'),
(0,'p'),(0,'f'),(0,'p'),(7,'d'),(7,'f'),(5,'j'),
(3,'e'),(1,'u'),(4,'v'),(9,'u'),(6,'i'),(1,'x'),
(7,'f'),(5,'j'),(3,'e'),(1,'u'),(4,'v'),(9,'u');

SET @save_optimize_switch=@@optimizer_switch;
SET optimizer_switch='materialization=on';

EXPLAIN
SELECT COUNT(*) FROM t1 AS t, t2
WHERE c = g
      AND (EXISTS (SELECT * FROM t1, t2 WHERE a = f  AND h <= t.e AND a > t.b)
           OR a = 0 AND h < 'z' );

SELECT COUNT(*) FROM t1 AS t, t2
WHERE c = g
      AND (EXISTS (SELECT * FROM t1, t2 WHERE a = f  AND h <= t.e AND a > t.b)
           OR a = 0 AND h < 'z' );
SELECT COUNT(*) FROM t1 AS t, t2
WHERE c = g
      AND (EXISTS (SELECT * FROM t1, t2 WHERE a = f  AND h <= t.e AND a > t.b)
           OR a = 0 AND h < 'z' );

SET optimizer_switch=@save_optimizer_switch;

DROP TABLE t1,t2;

--echo # check "Handler_pushed" status varuiables
CREATE TABLE t1 (
  c1 CHAR(1),
  c2 CHAR(1),
  KEY (c1)
);

INSERT INTO t1 VALUES ('3', '3'),('4','4'),('5','5');

flush status;
show status like "Handler_icp%";

SELECT * FROM t1 FORCE INDEX(c1) WHERE (c1='3' or c1='4') and c1 % 2 = 0 ;

show status like "Handler_icp%";

SELECT * FROM t1 WHERE (c2='3' or c2='4') and c2 % 2 = 0 ;

show status like "Handler_icp%";

DROP TABLE t1;

#
# MDEV-308 lp:1008516 - Failing assertion: templ->mysql_col_len == len
#
create table t1 (a int,b char(5),primary key (a), key (b(1)));
insert into t1 values ('a','b');
select 1 from t1 where a and b >= 'aa';
drop table t1;

--echo #
--echo # Bug#58015 "Assert in row_sel_field_store_in_mysql_format
--echo #            when running innodb_mrr_icp test"
--echo #

create table t1 (a char(2) charset utf8,b double, primary key (a(1)),key (b));
insert into t1 values ('',1);
select 1 from t1 where b <= 1 and a <> '';
drop table t1;

--echo #
--echo # MDEV-4778: Incorrect results from Aria/MyISAM SELECT using index with prefix length on TEXT column
--echo # 
CREATE TABLE t1 (
  c1 TEXT       ,
  c2 VARCHAR(2) ,
  INDEX idx1 (c2,c1(2)),
  INDEX idx2 (c2,c1(1))
);

INSERT INTO t1 (c1,c2) VALUES ('aa','x'), ('a' ,'y');
 
SELECT * FROM t1 IGNORE INDEX(idx1,idx2) WHERE (c1='aa' AND c2='x') OR (c1='a'  AND c2='y');

EXPLAIN
SELECT * FROM t1 FORCE INDEX(idx1)       WHERE (c1='aa' AND c2='x') OR (c1='a'  AND c2='y');
SELECT * FROM t1 FORCE INDEX(idx1)       WHERE (c1='aa' AND c2='x') OR (c1='a'  AND c2='y');

SELECT * FROM t1 FORCE INDEX(idx2)       WHERE (c1='aa' AND c2='x') OR (c1='a'  AND c2='y');

DROP TABLE t1;
