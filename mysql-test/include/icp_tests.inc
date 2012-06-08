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
--echo # Bug#43360 - Server crash with a simple multi-table update
--echo #

CREATE TABLE t1 (
  a CHAR(2) NOT NULL PRIMARY KEY, 
  b VARCHAR(20) NOT NULL,
  KEY (b)
);

CREATE TABLE t2 (
  a CHAR(2) NOT NULL PRIMARY KEY,
  b VARCHAR(30) NOT NULL,
  KEY (b)
);

INSERT INTO t1 VALUES 
('AB','MySQL AB'),
('JA','Sun Microsystems'),
('MS','Microsoft'),
('IB','IBM- Inc.'),
('GO','Google Inc.');

INSERT INTO t2 VALUES
('AB','Sweden'),
('JA','USA'),
('MS','United States of America'),
('IB','North America'),
('GO','South America');
 
UPDATE t1,t2 SET t1.b=UPPER(t1.b) WHERE t1.b LIKE 'United%';

SELECT * FROM t1 ORDER BY a;

SELECT * FROM t2 ORDER BY a;

DROP TABLE t1,t2;   

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
--echo # Bug#43249 - Innodb returns zero time for the time column 
--echo #             with <=> NULL order by limit 
--echo # (Note: Fixed by patch for BUG#42580)
--echo #

CREATE TABLE t1(
  c1 TIME NOT NULL, 
  c2 TIME NULL,
  c3 DATE, 
  PRIMARY KEY(c1), 
  UNIQUE INDEX(c2)
);

INSERT INTO t1 VALUES('8:29:45',NULL,'2009-02-01');

--echo
# First time, good results:
SELECT * FROM t1 WHERE c2 <=> NULL ORDER BY c2 LIMIT 2;

--echo
# Second time, bad results:
SELECT * FROM t1 WHERE c2 <=> NULL ORDER BY c2 LIMIT 2;

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

-- disable_query_log
-- disable_result_log
ANALYZE TABLE t1;
-- enable_result_log
-- enable_query_log

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

-- disable_query_log
-- disable_result_log
ANALYZE TABLE t1;
ANALYZE TABLE t2;
ANALYZE TABLE t3;
-- enable_result_log
-- enable_query_log

INSERT INTO t3 SELECT CONCAT('c-',1000+t2.a,'=w'), 1 FROM t2;

let query=SELECT c1 FROM t3 WHERE c1 >= 'c-1004=w' and c1 <= 'c-1006=w' and i1 > 2;

eval EXPLAIN $query;
eval $query;

DROP TABLE t1, t2, t3;

--echo #
--echo # Bug#57372 "Multi-table updates and deletes fail when running with ICP 
--echo #            against InnoDB"
--echo #

CREATE TABLE t1 (
  a INT KEY, 
  b INT
) ENGINE = INNODB;

CREATE TABLE t2 (
  a INT KEY, 
  b INT
) ENGINE = INNODB;

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

-- disable_query_log
-- disable_result_log
ANALYZE TABLE t1;
-- enable_result_log
-- enable_query_log

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
--echo # Bug#42991 "invalid memory access and/or crash when using
--echo #            index condition pushdown + InnoDB"
--echo #

# Note that you need to run with --valgrind to see the warnings
# about invalid memory accesses.

CREATE TABLE t1 (
  c1 TINYTEXT NOT NULL,
  c2 INT NOT NULL,
  PRIMARY KEY (c2),
  KEY id1 (c1(4))
);

INSERT INTO t1 VALUES ('Anastasia', 5);
INSERT INTO t1 VALUES ('Karianne', 4);

-- disable_query_log
-- disable_result_log
ANALYZE TABLE t1;
-- enable_result_log
-- enable_query_log

SELECT * FROM t1 WHERE (c1 <= '6566-06-15' AND c2 <> 3);

EXPLAIN SELECT * FROM t1 WHERE (c1 <= '6566-06-15' AND c2 <> 3);

DROP TABLE t1;

--echo #
--echo # Bug#56529 - "Crash due to long semaphore wait in InnoDB 
--echo #              with ICP and subqueries"
--echo #

CREATE TABLE t1 (
  col_int_nokey INTEGER,
  col_int_key INTEGER,
  col_varchar_key VARCHAR(1),

  KEY (col_int_key),
  KEY (col_varchar_key, col_int_key)
) stats_persistent=0;

INSERT INTO t1 VALUES (NULL,2,'w');
INSERT INTO t1 VALUES (7,9,'m');
INSERT INTO t1 VALUES (9,3,'m');
INSERT INTO t1 VALUES (7,9,'k');
INSERT INTO t1 VALUES (4,NULL,'r');
INSERT INTO t1 VALUES (2,9,'t');
INSERT INTO t1 VALUES (6,3,'j');
INSERT INTO t1 VALUES (8,8,'u');
INSERT INTO t1 VALUES (NULL,8,'h');
INSERT INTO t1 VALUES (5,53,'o');
INSERT INTO t1 VALUES (NULL,0,NULL);
INSERT INTO t1 VALUES (6,5,'k');
INSERT INTO t1 VALUES (188,166,'e');
INSERT INTO t1 VALUES (2,3,'n');
INSERT INTO t1 VALUES (1,0,'t');
INSERT INTO t1 VALUES (1,1,'c');
INSERT INTO t1 VALUES (0,9,'m');
INSERT INTO t1 VALUES (9,5,'y');
INSERT INTO t1 VALUES (NULL,6,'f');

CREATE TABLE t2 (
  c1 INTEGER NOT NULL
) stats_persistent=0;

let query=
SELECT table1.col_int_nokey
FROM t1 AS table1 STRAIGHT_JOIN (
                  t1 AS table2 INNER JOIN t1 AS table3
                  ON table3.col_varchar_key = table2.col_varchar_key)
     ON table3.col_int_nokey = table1.col_int_key
WHERE table3.col_int_key != SOME ( SELECT c1 FROM t2 );

eval EXPLAIN $query;
eval $query;

DROP TABLE t1, t2;

--echo #
--echo # Bug#58243 "RQG test optimizer_subquery causes server crash
--echo #            when running with ICP"
--echo #

# Test case 1: This test case makes item->const_item() return true
#              in uses_index_fields_only() for an item tree
#              containing a subquery. This triggered the subquery
#              to be pushed down to InnoDB.

CREATE TABLE t1 (
  pk INTEGER NOT NULL,
  c1 INTEGER NOT NULL,
  c2 INTEGER NOT NULL,

  PRIMARY KEY (pk)
);

INSERT INTO t1 VALUES (1,6,7);

CREATE TABLE t2 (
  c1 INTEGER NOT NULL
);

-- disable_query_log
-- disable_result_log
ANALYZE TABLE t1;
-- enable_result_log
-- enable_query_log

let query=
SELECT t1.c1
FROM t1
WHERE t1.pk < 317 AND 2 IN (SELECT COUNT(t2.c1)
                            FROM t2)
ORDER BY t1.c2;

eval EXPLAIN $query;
eval $query;

DROP TABLE t1, t2;

# Test case 2: This test case makes item->used_tables() return 0
#              in uses_index_fields_only() for an item tree
#              containg a subquery. This triggered the subquery
#              to be pushed down to InnoDB.

CREATE TABLE t1 (
  i1 INTEGER NOT NULL,
  c1 VARCHAR(1) NOT NULL
);

INSERT INTO t1 VALUES (2,'w');

CREATE TABLE t2 (
  i1 INTEGER NOT NULL,
  c1 VARCHAR(1) NOT NULL,
  c2 VARCHAR(1) NOT NULL,
  KEY (c1, i1)
);

INSERT INTO t2 VALUES (8,'d','d');
INSERT INTO t2 VALUES (4,'v','v');

CREATE TABLE t3 (
  c1 VARCHAR(1) NOT NULL
);

INSERT INTO t3 VALUES ('v');

-- disable_query_log
-- disable_result_log
ANALYZE TABLE t1;
ANALYZE TABLE t2;
ANALYZE TABLE t3;
-- enable_result_log
-- enable_query_log

let query=
SELECT i1
FROM t1
WHERE EXISTS (SELECT t2.c1
              FROM (t2 INNER JOIN t3 ON (t3.c1 = t2.c1)) 
              WHERE t2.c2 != t1.c1 AND t2.c2 = (SELECT MIN(t3.c1)
                                                FROM t3)); 

eval EXPLAIN $query;
eval $query;

DROP TABLE t1,t2,t3;

--echo #
--echo # Bug#58015 "Assert in row_sel_field_store_in_mysql_format
--echo #            when running innodb_mrr_icp test"
--echo #

create table t1 (a char(2) charset utf8,b double, primary key (a(1)),key (b));
insert into t1 values ('',1);
select 1 from t1 where b <= 1 and a <> '';
drop table t1;

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

SELECT * FROM t1
WHERE pk IN (SELECT it.pk FROM t2 JOIN t2 AS it ON it.i=it.i WHERE t1.i);

DROP TABLE t1, t2;

--echo #
--echo # Bug #58816 "Extra temporary duplicate rows in result set when 
--echo #             switching ICP off"
--echo #

# Save optimizer switch setting
set @save_optimizer_switch_bug58816= @@optimizer_switch;

CREATE TABLE t1 (
  pk INT NOT NULL,
  c1 INT NOT NULL,
  PRIMARY KEY (pk)
);

INSERT INTO t1 VALUES (1,9),(2,7),(3,6),(4,3),(5,1);

-- disable_query_log
-- disable_result_log
ANALYZE TABLE t1;
-- enable_result_log
-- enable_query_log

EXPLAIN SELECT pk, c1 FROM t1 WHERE pk <> 3;

SET SESSION optimizer_switch='index_condition_pushdown=off';

SELECT pk, c1 FROM t1 WHERE pk <> 3;

DROP TABLE t1;

# Restore optimzer switch setting
set optimizer_switch= @save_optimizer_switch_bug58816;

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

-- disable_query_log
-- disable_result_log
ANALYZE TABLE t1;
ANALYZE TABLE t2;
-- enable_result_log
-- enable_query_log

let $query=
SELECT *
FROM t1 AS t1a 
WHERE NOT EXISTS
  (SELECT *
   FROM t1 AS t1b
   WHERE t1b.t1_int NOT IN
     (SELECT t2.t2_int 
      FROM t2
      WHERE t1b.t1_time LIKE t1b.t1_int
      OR t1b.t1_time <> t2.t2_int2
      AND 6=7 
 )
)
;

--echo
--eval $query;
--echo
--eval EXPLAIN $query;
--echo

DROP TABLE t1,t2;

--echo #
--echo # Bug#59186 Wrong results of join when ICP is enabled
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

-- disable_query_log
-- disable_result_log
ANALYZE TABLE t1;
ANALYZE TABLE t2;
-- enable_result_log
-- enable_query_log

let query=
SELECT c2 FROM t1 JOIN t2 ON t1.c1 = t2.c1
WHERE (t2.pk <= 4 AND t1.pk IN (2,1)) OR
      (t1.pk > 1 AND t2.pk BETWEEN 6 AND 6);

eval EXPLAIN $query;
eval $query;

DROP TABLE t1, t2;

--echo #
--echo # Bug#58838 "Wrong results with HAVING + LIMIT without GROUP BY when 
--echo #            ICP is enabled"
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
--echo # Bug#11873324 "WRONG RESULT WITH ICP AND STRAIGHT_JOIN"
--echo #

CREATE TABLE t1 (  
  pk INTEGER NOT NULL,
  i1 INTEGER,
  PRIMARY KEY (pk),
  KEY col_int_key (i1)
);

INSERT INTO t1 VALUES (14,NULL), (18,133);

CREATE TABLE t2 (  
   pk INTEGER NOT NULL,
   i1 INTEGER,
   c1 VARCHAR(1),
   PRIMARY KEY (pk),
   KEY col_int_key (i1)
);

INSERT INTO t2 VALUES (1,7,'f');

if (`SELECT UPPER(@@default_storage_engine) = 'INNODB'`)
{
-- disable_query_log
-- disable_result_log
ANALYZE TABLE t1;
ANALYZE TABLE t2;
-- enable_result_log
-- enable_query_log
}

# Bug was specific of IN->EXISTS:
set @old_opt_switch=@@optimizer_switch;
--disable_query_log
if (`select locate('materialization', @@optimizer_switch) > 0`) 
{
  set optimizer_switch='materialization=off';
}
--enable_query_log

let query=
SELECT t1.i1
FROM t1
WHERE t1.i1 NOT IN
( SELECT STRAIGHT_JOIN SUBQUERY_t1.pk
  FROM t1 AS SUBQUERY_t1
    JOIN t2 AS SUBQUERY_t2
    ON SUBQUERY_t2.i1 = SUBQUERY_t1.pk
  WHERE SUBQUERY_t1.i1 > 0
    OR SUBQUERY_t2.c1 = 'a'
);

eval EXPLAIN $query;
eval $query;

set @@optimizer_switch=@old_opt_switch;
DROP TABLE t1,t2;

--echo #
--echo # Bug#11876420 "MISSING ROW IN RESULT WITH SUBQUERY + IN + XOR + 
--echo #               NULL VALUES AND ICP ENABLED"
--echo # 

CREATE TABLE t1 (  
   i1 INTEGER,
   c1 VARCHAR(1),
   KEY col_varchar_key (c1)
);

INSERT INTO t1 VALUES (1,'j'), (0,'e'), (210,'f'), (8,'v'), (7,'x'),
                      (5,'m'), (NULL,'c');

CREATE TABLE t2 (  
   i1 INTEGER,
   c1 VARCHAR(1),
   KEY col_varchar_key (c1)
);

INSERT INTO t2 VALUES (8,NULL);

CREATE TABLE t3 (  
   i1 INTEGER,
   c1 VARCHAR(1),
   KEY col_varchar_key (c1)
) ENGINE=InnoDB;

INSERT INTO t3 VALUES (NULL,'w'), (1,NULL), (2,'d');

-- disable_query_log
-- disable_result_log
ANALYZE TABLE t1;
ANALYZE TABLE t2;
ANALYZE TABLE t3;
-- enable_result_log
-- enable_query_log

# Bug was specific of IN->EXISTS:
set @old_opt_switch=@@optimizer_switch;
--disable_query_log
if (`select locate('materialization', @@optimizer_switch) > 0`) 
{
  set optimizer_switch='materialization=off';
}
--enable_query_log

let query=
SELECT i1
FROM t3
WHERE c1 IN
  ( SELECT t1.c1
    FROM t2 JOIN t1
      ON t2.i1 >= t1.i1
    WHERE t1.c1 > t2.c1
  )
  XOR i1;

eval EXPLAIN $query;
eval $query;

set @@optimizer_switch=@old_opt_switch;
DROP TABLE t1, t2, t3;

--echo #
--echo # Bug#12355958 "FAILING ASSERTION: TRX->LOCK.N_ACTIVE_THRS == 1"
--echo #

CREATE TABLE t1 (
  pk INTEGER PRIMARY KEY, 
  a INTEGER NOT NULL, 
  b CHAR(1), 
  KEY(b)
);

INSERT INTO t1 VALUES (23,5,'d');

-- disable_query_log
-- disable_result_log
ANALYZE TABLE t1;
-- enable_result_log
-- enable_query_log

let query=
SELECT a1.pk 
FROM t1 AS a1 JOIN (SELECT * FROM t1 LIMIT 1) AS a2 ON a2.b = a1.b
WHERE a1.a = (SELECT pk FROM t1 LIMIT 1) 
      AND (a1.a != a2.a OR a1.b IS NULL);

eval EXPLAIN $query;
eval $query;

# Re-run the same query using a view. This will test the code with
# an Item_ref object in the condition tree (note: in order for this
# to trigger the bug the patch for Bug#59696 needs to be applied first).

CREATE VIEW v1 AS SELECT * FROM t1;

let query=
SELECT a1.pk 
FROM v1 AS a1 JOIN (SELECT * FROM v1 LIMIT 1) AS a2 ON a2.b = a1.b
WHERE a1.a = (SELECT pk FROM v1 LIMIT 1) 
      AND (a1.a != a2.a OR a1.b IS NULL);

eval EXPLAIN $query;
eval $query;

DROP VIEW v1;
DROP TABLE t1;

--echo #
--echo # BUG#12601961 "SEGFAULT IN HANDLER::COMPARE_KEY2"
--echo # BUG#12724899 "SELECT STRAIGHT_JOIN QUERY GIVES 2 DATES VERSUS
--echo #               2 WARNINGS WITH ICP ON"
--echo #

CREATE TABLE t1 (
  pk INTEGER NOT NULL, 
  i1 INTEGER NOT NULL,
  c1 VARCHAR(1) NOT NULL,
  PRIMARY KEY (pk)
);

INSERT INTO t1 VALUES (1,3,'j'), (20,8,'e');

-- disable_query_log
-- disable_result_log
ANALYZE TABLE t1;
-- enable_result_log
-- enable_query_log

let query=
SELECT alias2.i1
FROM t1 AS alias1 STRAIGHT_JOIN t1 AS alias2
ON alias2.pk AND alias2.pk <= alias1.c1
WHERE alias2.pk = 1;

eval EXPLAIN $query;
eval $query;

DROP TABLE t1;

--echo #
--echo # BUG#12822678 - 2 MORE ROWS WHEN ICP=ON W/ STRAIGHT_JOIN
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

-- disable_query_log
-- disable_result_log
ANALYZE TABLE t1;
ANALYZE TABLE t2;
-- enable_result_log
-- enable_query_log

let query=
SELECT t1.d1, t2.pk, t2.i1
FROM t1 STRAIGHT_JOIN t2 ON t2.i1
WHERE t2.pk <> t1.d1 AND t2.pk = 4;

eval EXPLAIN $query;
eval $query;

DROP TABLE t1, t2;

--echo #
--echo # BUG#12838420 "DUPLICATE VALUES FOR GROUP-BY COLUMN WHEN JOIN
--echo # BUFFERING IS OFF"
--echo #
CREATE TABLE t1 (
  col_int_key INT,
  pk INT,
  PRIMARY KEY (pk),
  KEY (col_int_key)
);
INSERT INTO t1 VALUES (2,3),(3,2),(3,5),(4,6);

CREATE TABLE t2 (
  col_int_key INT,
  pk INT,
  PRIMARY KEY (pk),
  KEY (col_int_key)
);
INSERT INTO t2 VALUES (0,9),(3,10),(4,6),(6,1),(100,3),(200,5);

-- disable_query_log
-- disable_result_log
ANALYZE TABLE t1;
ANALYZE TABLE t2;
-- enable_result_log
-- enable_query_log

let $query=
SELECT t2.col_int_key AS field1
FROM t2 USE INDEX(col_int_key) STRAIGHT_JOIN t1 ON t2.col_int_key
WHERE t2.pk < 7 AND t2.col_int_key <> 7
GROUP BY field1;

set @old_opt_switch=@@optimizer_switch;
# With BNL, a temp table is used for GROUP BY, we don't want this
SET optimizer_switch="block_nested_loop=off";
eval EXPLAIN $query;
eval $query;

SET @@optimizer_switch=@old_opt_switch;
DROP TABLE t1,t2;

--echo #
--echo # Bug#12976163 "CRASH IN INDEX CONDITION PUSHDOWN CODE AGAINST
--echo #               A MYISAM TABLE"
--echo #

CREATE TABLE t1 (
  i1 INTEGER NOT NULL,
  i2 INTEGER NOT NULL
);

INSERT INTO t1 VALUES (14,1), (15,2), (16,3);

CREATE TABLE t2 (
  i1 INTEGER NOT NULL,
  i2 INTEGER NOT NULL,
  c1 TINYTEXT
);

INSERT INTO t2
SELECT i1, 10 * i2, "MySQL" FROM t1;

DELIMITER $$;

CREATE PROCEDURE proc1(id INTEGER)
BEGIN

SELECT i2
  FROM (
       (SELECT i1, i2, NULL AS a1 FROM t1)
       UNION
       (SELECT i1, i2, c1 AS a1 FROM t2)
       ) u1
WHERE i1 = id;

END$$

DELIMITER ;$$

CALL proc1(15); 

DROP PROCEDURE proc1;

DROP TABLE t1, t2;

--echo #
--echo # Bug#13655397 "CRASH IN SYNC_THREAD_LEVELS_NONEMPTY_TRX"
--echo #

CREATE TABLE t1 (
  i1 INTEGER NOT NULL,
  i2 INTEGER NOT NULL,
  KEY (i1)
);

INSERT INTO t1 VALUES (4,4), (5,5);

CREATE TABLE t2 (
  pk INTEGER NOT NULL,
  PRIMARY KEY (pk)
);

INSERT INTO t2 VALUES (1);

-- disable_query_log
-- disable_result_log
ANALYZE TABLE t1;
ANALYZE TABLE t2;
-- enable_result_log
-- enable_query_log

CREATE FUNCTION f1() RETURNS INTEGER
RETURN (SELECT MOD(COUNT(DISTINCT pk), 10) FROM t2);

let query=
SELECT i1, i2 FROM t1 WHERE f1() = 1 AND i1 = 5;

eval EXPLAIN $query;
eval $query;

DROP FUNCTION f1;
DROP TABLE t1, t2;
