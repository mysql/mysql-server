# Test for descending indexes support

# Test that desc index can be created only on engine that supports it
--error 1178
create table t1(f1 int, key f1_idx(f1 desc)) engine=heap;

eval create table
  t1(a int, b int, key a_desc_b_asc (a desc, b), key a_asc_b_desc (a, b desc))
  engine= $engine; 
flush tables;
show create table t1;

insert into t1 values(1,6),(1,5),(2,5),(2,4),(3,4),(3,3),
  (4,3),(4,2),(5,2),(5,1),(6,1),(NULL,NULL);
--disable_result_log
analyze table t1;
--enable_result_log

--echo # Should use index
--replace_column 10 # 11 #
explain select * from t1 order by a desc;
--partially_sorted_result 1   # The hypergraph optimizer doesn't use the index.
select * from t1 order by a desc;

--replace_column 10 # 11 #
explain select * from t1 order by a desc, b asc;
select * from t1 order by a desc, b asc;

--replace_column 10 # 11 #
explain select * from t1 order by a asc;
select * from t1 order by a asc, b desc;

--replace_column 10 # 11 #
explain select * from t1 order by a asc, b desc;
select * from t1 order by a asc, b desc;

--replace_column 10 # 11 #
explain select * from t1 group by a,b order by a, b desc;
select * from t1 group by a,b order by a, b desc;

--echo # For GROUP BY optimizer can pick any order for column,
--replace_column 10 # 11 #
explain select * from t1 group by a, b;
--replace_column 10 # 11 #
explain select * from t1 group by a, b order by a;
--replace_column 10 # 11 #
explain select * from t1 group by a , b order by a desc;
--replace_column 10 # 11 #
explain select * from t1 group by a, b order by a desc, b;
--replace_column 10 # 11 #
explain select * from t1 group by a, b order by a asc, b desc;

alter table t1 drop index a_asc_b_desc;
--replace_column 10 # 11 #
explain select * from t1 group by a, b order by a;

--replace_column 10 # 11 #
explain select distinct a from t1;
--sorted_result  # The hypergraph optimizer does not use indexes for DISTINCT yet.
select distinct a from t1;

--replace_column 10 # 11 #
explain select a from t1 group by a;
--sorted_result   # The hypergraph optimizer doesn't use the index.
select a from t1 group by a;

--echo # Should use index backward
--replace_column 10 # 11 #
explain select * from t1 order by a asc;
select * from t1 order by a asc, b desc;

--replace_column 10 # 11 #
explain select * from t1 order by a asc, b desc;
select * from t1 order by a asc, b desc;

--echo # Should use filesort
--replace_column 10 # 11 #
explain select * from t1 order by a desc, b desc;
select * from t1 order by a desc, b desc;

--replace_column 10 # 11 #
explain select * from t1 order by a asc, b asc;
select * from t1 order by a asc, b asc;

--error ER_DUP_FIELDNAME
create index i1 on t1 (a desc, a asc);
create index i1 on t1 (a desc, b desc);
show create table t1;

eval create table t2 (a int auto_increment, primary key (a desc)) engine= $engine;
show create table t2;
--sorted_result  # Warnings come in different order with --hypergraph.
insert ignore into t2 select a from t1;
select * from t2;

eval create table t3 (a varchar(10), key i1(a(5) desc)) engine= $engine;
flush tables;
show create table t3;
create index i2 on t3(a(6));
flush tables;
show create table t3;

--error 1221
alter table t3 add fulltext index fts_idx(a desc);
--error 1221
alter table t3 add fulltext index fts_idx(a asc);
--error 1221
alter table t3 add column b point not null, add spatial index gis_idx(b desc);
--error 1221
alter table t3 add column b point not null, add spatial index gis_idx(b asc);

--error 1221
create table t4 (a text, fulltext key fts(a desc));
--error 1221
create table t4 (a point not null, spatial key gis(a desc));
--error 1221
create table t4 (a text, fulltext key fts(a asc));
--error 1221
create table t4 (a point not null, spatial key gis(a asc));

--error 1221
create table t5 (f1 int, key h(f1 asc) using hash) engine= heap;
--error 1221
create table t5 (f1 int, key h(f1 desc) using hash) engine= heap;
create table t5 (f1 int, key h(f1) using hash) engine= heap;

drop table t1,t2,t3,t5;

CREATE TABLE t0 (i INTEGER);
INSERT INTO t0 VALUES (0),(1),(2),(3),(4),(5),(6),(7),(8),(9);
eval CREATE TABLE t1 (i1 INTEGER NOT NULL, i2 INTEGER NOT NULL,
                 i3 INTEGER NOT NULL, KEY k1 (i1 desc, i2)
) ENGINE= $engine;

INSERT INTO t1
  SELECT a.i*10 + b.i + 1, a.i*100 + b.i*10 + c.i, a.i
  FROM t0 AS a, t0 AS b, t0 AS c;
ANALYZE TABLE t1;

let query1=
  SELECT * FROM t1
  WHERE i1 BETWEEN 50 AND 52 AND MOD(i2,2)=1 ORDER BY i1 DESC;

eval EXPLAIN $query1;
--partially_sorted_result 1
eval $query1; 

let query1=
SELECT * FROM t1
  WHERE (i1 BETWEEN 50 AND 52 OR i1 BETWEEN 70 AND 72) AND MOD(i2,2)=1
  ORDER BY i1 DESC;

eval EXPLAIN $query1;
--sorted_result
eval $query1; 

let query1=
SELECT * FROM t1
  WHERE ( (i1=50 AND i2=495) OR i1 BETWEEN 70 AND 72) AND MOD(i2,2)=1
  ORDER BY i1 DESC;

eval EXPLAIN $query1;
--sorted_result
eval $query1; 

EXPLAIN SELECT * FROM t1 WHERE i1 >= 50 AND i1 < 52 AND MOD(i2,2)=1 ORDER BY i1 DESC;
--partially_sorted_result 1
SELECT * FROM t1 WHERE i1 >= 50 AND i1 < 52 AND MOD(i2,2)=1 ORDER BY i1 DESC;
EXPLAIN SELECT * FROM t1 WHERE i1 > 50 AND i1 <= 52 AND MOD(i2,2)=1 ORDER BY i1 DESC;
--partially_sorted_result 1
SELECT * FROM t1 WHERE i1 > 50 AND i1 <= 52 AND MOD(i2,2)=1 ORDER BY i1 DESC;

ALTER TABLE t1 DROP INDEX k1, ADD INDEX k1(i1, i2);
ANALYZE TABLE t1;
let query1=
  SELECT * FROM t1
  WHERE i1 BETWEEN 50 AND 52 AND MOD(i2,2)=1 ORDER BY i1 DESC;

eval EXPLAIN $query1;
--partially_sorted_result 1
eval $query1; 

ALTER TABLE t1 DROP INDEX k1, ADD INDEX k1(i1, i2 DESC);
ANALYZE TABLE t1;
let query1=
  SELECT * FROM t1
  WHERE i1 BETWEEN 50 AND 52 AND i2 BETWEEN 495 AND 515 ORDER BY i1, i2 DESC;
eval EXPLAIN $query1;
eval $query1; 

ALTER TABLE t1 DROP INDEX k1, ADD INDEX k1(i1, i2 DESC, i3);
ANALYZE TABLE t1;
let query1=
  SELECT * FROM t1
  WHERE i1 BETWEEN 48 AND 62 AND i2 BETWEEN 395 AND 615 AND
    i3 BETWEEN 4 AND 5 AND MOD(i1,2)=0 AND MOD(i2,2)=1
  ORDER BY i1, i2 DESC;

eval EXPLAIN $query1;
eval $query1; 

ALTER TABLE t1 DROP INDEX k1, ADD INDEX k1(i1, i2 DESC, i3 DESC);
ANALYZE TABLE t1;
eval EXPLAIN $query1;
eval $query1; 

ALTER TABLE t1 DROP INDEX k1, ADD INDEX k1(i1, i2 DESC, i3);
ANALYZE TABLE t1;
let $query1=
  SELECT * FROM t1   WHERE 
    ((i1 BETWEEN 48 AND 62 AND i2 BETWEEN 395 AND 615 AND
      i3 BETWEEN 4 AND 5) or i1 between 70 and 72) AND
    MOD(i1,2)=0 AND MOD(i2,2)=1
  ORDER BY i1 desc, i2 ;
eval EXPLAIN $query1;
eval $query1; 

ALTER TABLE t1 DROP INDEX k1, ADD INDEX k1(i1 DESC, i2, i3);
ANALYZE TABLE t1;
eval EXPLAIN $query1;
eval $query1; 

ALTER TABLE t1 DROP INDEX k1, ADD INDEX k1(i1 DESC, i2, i3);
ANALYZE TABLE t1;
let query1=
  SELECT * FROM t1
  WHERE ( (i1=50 AND i2=495 and i3=4) OR i1 BETWEEN 70 AND 72) AND
          MOD(i2,2)=1
  ORDER BY i1 DESC;
eval EXPLAIN $query1;
eval $query1;

#Test case for checking the "else" parts in store_min_key and store_max_key
ALTER TABLE t1 ADD COLUMN (i4 INTEGER NOT NULL);
UPDATE t1 SET i4=i3;

ALTER TABLE t1 DROP INDEX k1, ADD INDEX k1(i1 DESC, i2, i3 DESC, i4);
ANALYZE TABLE t1;
let $query1=
  SELECT * FROM t1   WHERE
    ((i1 BETWEEN 48 AND 62 AND i2 BETWEEN 395 AND 615 AND
      i3 BETWEEN 4 AND 5 AND i4 BETWEEN 5 AND 6) OR
     i1 between 70 and 72) AND MOD(i1,2)=0 AND MOD(i2,2)=1
  ORDER BY i1 desc, i2 ;
eval EXPLAIN $query1;
eval $query1;

DROP TABLE t0, t1;

CREATE TABLE t1 (a INT, b INT, KEY i1 (a DESC, b DESC));
INSERT INTO t1 (a, b) VALUES (1,1), (1,2), (1,3);
INSERT INTO t1 SELECT a + 1, b FROM t1;
INSERT INTO t1 SELECT a + 2, b FROM t1;
INSERT INTO t1 SELECT a + 4, b FROM t1;

--disable_query_log
ANALYZE TABLE t1;
--enable_query_log

EXPLAIN
SELECT a, MIN(b), MAX(b) FROM t1 GROUP BY a ORDER BY a DESC;
SELECT a, MIN(b), MAX(b) FROM t1 GROUP BY a ORDER BY a DESC;

DROP TABLE t1;

create table t1 (a int not null, b int, c varchar(10),
  key (a desc, b desc, c desc));
insert into t1 values (1, NULL, NULL), (1, NULL, 'b'), (1, 1, NULL), (1, 1, 'b'), (1, 1, 'b'), (2, 1, 'a'), (2, 1, 'b'), (2, 2, 'a'), (2, 2, 'b'), (2, 3, 'c'),(1,3,'b');
insert into t1 values (0, NULL, NULL), (0, NULL, 'b'), (0, 0, NULL), (0, 0, 'b'), (0, 0, 'b'), (0, 0, 'a'), (0, 0, 'b'), (0, 0, 'a'), (0, 0, 'b'), (0, 0, 'c'),(0,0,'b');

ANALYZE TABLE t1;
explain select * from t1 where a >= 1 and a < 3 and b >0 order by a desc,b desc;
flush status;
--partially_sorted_result 2
select * from t1 where a >= 1 and a < 3 and b >0 order by a desc,b desc;
--skip_if_hypergraph  # Depends on the query plan.
show status like 'handler_read%';
drop table t1;

create table t1 (a1 int, a2 char(3), key k1(a1 desc));
insert into t1 values(10,'aaa'), (10,null), (10,'bbb'), (20,'zzz');
ANALYZE TABLE t1;
--echo # Shouldn't optimize tables away on DESC index
--replace_column 10 # 11 # 
explain select min(a1) from t1;
select min(a1) from t1;
drop table t1;

CREATE TABLE t1
(a VARCHAR(10),
 b VARCHAR(10),
 KEY ab_asc (a ASC, b ASC),
 KEY a_asc_b_desc (a ASC, b DESC),
 key a_desc_b_asc (a DESC, b ASC))
ENGINE = InnoDB 
PARTITION BY KEY (a, b) PARTITIONS 3;
INSERT INTO t1 VALUES ("0", "0"), ("1", "1"), ("2", "2"), ("3", "3"),
("4", "4"), ("55", "55"), ("54", "54"), ("1", "2"), ("1", "4"), ("1", "3"),
("55", "54"), ("0", "1"), (NULL,NULL),(0, NULL), (1,NULL);

SELECT * FROM t1 ORDER BY a, b DESC;
DROP TABLE t1;

--echo #
--echo # Bug#23036049: WL1074:ASSERTION `CTX->CUR <= CTX->LAST' FAILED.
--echo #

eval CREATE TABLE c (
  pk INTEGER AUTO_INCREMENT,
  col_int INTEGER NOT NULL,
  col_varchar VARCHAR(5) NOT NULL,
  unique KEY (pk,col_int DESC)
) ENGINE= $engine;

INSERT IGNORE INTO c (col_int,col_varchar) VALUES
(7, 'm'),(0, 'alukq'),(8, 'lu'),(6, 'uk'), (5, 'kquk'),(9, 'qukko'),(0, 'u'),
(181, 'kkoei'),(3, 'ko'),(86, 'oei');

eval CREATE TABLE cc (
  pk INTEGER AUTO_INCREMENT,
  col_int INTEGER NOT NULL,
  col_varchar VARCHAR(5) NOT NULL,
  unique KEY (pk,col_int DESC)
) ENGINE= $engine;

INSERT IGNORE INTO cc (col_int,col_varchar) VALUES 
(9, 'gktbk'),(0, 'k'),(4, 'tbkj'),(8, 'bk'),(9, 'kjrk'),(2,'j'),(7, 'r'),
(4, 'kmqmk'),(0, 'm'),(4, 'qmkn');

SELECT DISTINCT t2.col_int
FROM ( c AS t1 INNER JOIN cc AS t2 ON (t2.col_varchar = t1.col_varchar))
WHERE ( t1.col_int IN ( 167, 9))
AND t1.pk = 122;

DROP TABLE c,cc;

--echo #

eval CREATE TABLE b (
  pk INTEGER AUTO_INCREMENT,
  col_int_key INTEGER /*! NULL */,
  col_varchar_key VARCHAR(10) /*! NULL */,
  PRIMARY KEY (pk DESC),
  KEY (col_varchar_key DESC, col_int_key DESC)
) ENGINE= $engine;

INSERT /*! IGNORE */ INTO b (col_int_key,col_varchar_key) VALUES
(3, 'ceksatef'),(3, 'eks'),(3, 'ksatefqs'),(6, 'sate'),(3, 'a');

eval CREATE TABLE cc (
  pk INTEGER ,
  col_int_key INTEGER /*! NULL */,
  col_varchar_key VARCHAR(10) /*! NULL */
) ENGINE= $engine;

INSERT /*! IGNORE */ INTO cc (col_int_key, col_varchar_key) VALUES
(NULL, 'koeiwsgpmf'),(8, 'oeiwsgpm'),(8, 'eiwsg'),(0,'iwsg'),(5, 'wsgpmfy'),
(1, 'sgpmfyvvu'),(7, 'gpmfyvvu'),(7, 'pmfyvvu'),(147, 'mfyv'),(2, NULL);

--echo # Shouldn't crash

SELECT
DISTINCT  OUTR . col_varchar_key 
FROM b AS OUTR WHERE ( OUTR . col_int_key , OUTR . pk ) IN
(
SELECT DISTINCT
INNR . pk AS x ,
INNR . pk AS y
FROM cc AS INNR WHERE OUTR . col_varchar_key = 'v'
)
AND OUTR . pk >= 3 ;

DROP TABLE b;

eval CREATE TABLE b (
  pk INTEGER AUTO_INCREMENT,
  col_int_key INTEGER /*! NULL */,

  col_varchar_key VARCHAR(10) /*! NULL */,

  PRIMARY KEY (pk DESC),
  KEY (col_varchar_key, col_int_key)
)  ENGINE= $engine;

INSERT /*! IGNORE */ INTO b (col_int_key,col_varchar_key)
VALUES (3, 'ceksatef'),(3, 'eks'),(3, 'ksatefqs'),(6, 'sate'),(3, 'a');

--echo # Shouldn't crash

SELECT
DISTINCT  OUTR . col_varchar_key 
FROM b AS OUTR WHERE ( OUTR . col_int_key , OUTR . pk ) IN
(
SELECT DISTINCT
INNR . pk AS x ,
INNR . pk AS y
FROM cc AS INNR WHERE OUTR . col_varchar_key = 'v'
)
AND OUTR . pk >= 3 ;


DROP TABLE b,cc;

--echo #
if ($engine == "myisam")
{
eval CREATE TABLE dd (
  col_varchar_key varchar(10) DEFAULT NULL,
  KEY col_varchar_key (col_varchar_key DESC)
) ENGINE=$engine;

INSERT INTO dd VALUES
('1'),
('2'),
('3'),
('4'),
('5');

ANALYZE TABLE dd;

SET SESSION DEBUG="+d,force_lis_for_group_by";

EXPLAIN SELECT DISTINCT INNR1 . col_varchar_key AS y FROM dd AS INNR1 force index for group by (col_varchar_key) WHERE INNR1 . col_varchar_key < INNR1 . col_varchar_key;

SELECT DISTINCT INNR1 . col_varchar_key AS y FROM dd AS INNR1 force index for group by (col_varchar_key) WHERE INNR1 . col_varchar_key < INNR1 . col_varchar_key;

DROP TABLE dd;
SET SESSION DEBUG="";
}


--echo #
--echo # Bug#23212656:JOIN QUERY WITH RANGE PREDICATES GIVES INCORRECT RESULTS
--echo #
eval CREATE TABLE ee (
  col_int int(11) DEFAULT NULL,
  col_int_key int(11) DEFAULT NULL,
  pk int(11) NOT NULL AUTO_INCREMENT,
  PRIMARY KEY (pk DESC),
  KEY 1col_int_key (col_int_key DESC)
) ENGINE=$engine;

INSERT INTO ee VALUES
(NULL,NULL,1), (NULL,NULL,2), (NULL,286720000,3), (NULL,1,4),
(2084831232,8,5), (NULL,0,6), (4,763953152,7), (5,NULL,8), (7,9,9);

--sorted_result
SELECT DISTINCT alias1 . col_int_key AS field1 , alias1 . col_int AS field2
FROM ee AS alias1 JOIN ee AS alias2 ON  alias1 . pk =  alias2 . col_int_key
WHERE ( alias1 . pk  BETWEEN 8 AND ( 8 + 4 ) AND alias2 . pk <> 2 );

DROP TABLE ee;

--echo #

--echo #
--echo # Bug#23217803:QUERY USING INDEX_MERGE_SORT_UNION GIVES INCORRECT
--echo #              RESULTS WITH DESC KEY
--echo #
eval CREATE TABLE t (
  pk INTEGER AUTO_INCREMENT,
  col_int INTEGER ,
  col_int_key INTEGER ,

  col_varchar_key VARCHAR(10) ,
  col_varchar VARCHAR(10) ,

  PRIMARY KEY (pk DESC),
  KEY (col_varchar_key DESC),
  UNIQUE KEY (col_int_key DESC, pk)
) ENGINE=$engine;

INSERT INTO t (col_int_key, col_int, col_varchar_key)
VALUES (1, 2, NULL),(NULL, 3, 'dks'), (7, 0, 'ksjijcsz'),(172, 84, 'sj');

SELECT col_int  FROM t  AS table1  WHERE table1 .pk  >  166  OR table1
.col_varchar_key  = 'c' OR table1 .col_int_key  >  166 LIMIT 1;

DROP TABLE t;
--echo #

--echo #
--echo # BUG#22973383:INNODB ASSERTION IN ROW_SEL_CONVERT_MYSQL_KEY_TO_INNOBASE
--echo #

eval CREATE TABLE c (
  col_varchar_10_utf8_key varchar(10) CHARACTER SET utf8mb3 DEFAULT NULL,
  unique key k5 (col_varchar_10_utf8_key(7) DESC)
) ENGINE=$engine;

eval CREATE TABLE e (
  col_varchar_10_utf8_key varchar(10) CHARACTER SET utf8mb3 DEFAULT NULL,
  col_varchar_255_utf8_key varchar(255) CHARACTER SET utf8mb3 DEFAULT NULL,
  col_varchar_10_latin1_key varchar(10) DEFAULT NULL,
  unique key k5 (col_varchar_10_utf8_key(7) DESC,
col_varchar_10_latin1_key(5) DESC, col_varchar_255_utf8_key(50) DESC)
) ENGINE=$engine;

EXPLAIN SELECT table2 . col_varchar_10_utf8_key AS field1
FROM  e AS table1 LEFT  JOIN c AS table2
  ON  table1 . col_varchar_10_utf8_key =  table2 . col_varchar_10_utf8_key
WHERE table1 . col_varchar_255_utf8_key != 'LPGIV'
AND table1 . col_varchar_10_latin1_key >= 'w'
AND table1 . col_varchar_10_utf8_key < 'zzzz';

SELECT table2 . col_varchar_10_utf8_key AS field1
FROM  e AS table1 LEFT  JOIN c AS table2
  ON  table1 . col_varchar_10_utf8_key =  table2 . col_varchar_10_utf8_key
WHERE table1 . col_varchar_255_utf8_key != 'LPGIV'
AND table1 . col_varchar_10_latin1_key >= 'w'
AND table1 . col_varchar_10_utf8_key < 'zzzz';

DROP TABLE c,e;

CREATE TABLE t1 (
i int(11) NOT NULL,
j int(11) DEFAULT NULL,
k int(11) DEFAULT NULL,
l int(11) DEFAULT NULL,
PRIMARY KEY (i),
KEY j (j,k DESC,l),
KEY i (i,j,k,l)
)ENGINE=InnoDB;

INSERT INTO t1 VALUES
(11,1,6,3),(4,1,2,3),(8,1,2,3),(10,1,2,3),(1,1,1,1),(2,1,1,1),(3,1,1,1),
(5,3,2,3),(6,4,2,3),(7,6,2,3),(12,7,6,3),(13,7,6,8),(14,7,6,9),(16,8,7,9),
(15,8,6,9);

ANALYZE TABLE t1;

EXPLAIN SELECT * FROM t1 WHERE i < 10 AND j >=1 AND k >=2 AND l <=5;
SELECT * FROM t1 WHERE i < 10 AND j >=1 AND k >=2 AND l <=5;

DROP TABLE t1;

CREATE TABLE t1 (
pk INTEGER NOT NULL,
col_int_key INTEGER ,
col_varchar_key BLOB NOT NULL,
PRIMARY KEY (pk),
KEY (col_int_key, col_varchar_key(25) DESC)
); 

CREATE TABLE t2 (
pk INTEGER NOT NULL,
col_int INTEGER ,
col_int_key INTEGER ,
col_varchar_key BLOB ,
PRIMARY KEY (pk),
KEY (col_int_key, col_varchar_key(25) DESC)
);

let $query=SELECT t1.col_varchar_key
FROM ( t2 INNER JOIN t1
ON (t1.col_int_key = t2.col_int  AND (1,5)  IN
( SELECT alias1.col_int_key, alias1. pk
FROM ( t2 AS alias2 RIGHT JOIN t1 AS alias1
 ON (alias1.pk = alias2.col_int_key )
) WHERE alias1.col_varchar_key >= 'y') ) );

eval EXPLAIN $query;
eval $query;

#Checking inversion of min_flag for desc key_part when
#minimum range is not present
let $query=SELECT t1.col_varchar_key
FROM ( t2 INNER JOIN t1
ON (t1.col_int_key = t2.col_int  AND (1,5)  IN
( SELECT alias1.col_int_key, alias1. pk
FROM ( t2 AS alias2 RIGHT JOIN t1 AS alias1
 ON (alias1.pk = alias2.col_int_key )
) WHERE alias1.col_varchar_key <= 'y') ) );

eval EXPLAIN $query;
eval $query;

DROP TABLE t1,t2;

--echo #End of test case for Bug#22973383

--echo #
--echo # Bug #23576305:WL1074:STRAIGHT_JOIN QUERY WITH RANGE
--echo #              CHECKED (MYISAM) GIVES WRONG RESULTS
--echo #

CREATE TABLE t1 (
  pk INTEGER NOT NULL ,
  col_varchar_key varchar(10) DEFAULT NULL,
  UNIQUE KEY pk_2 (pk,col_varchar_key DESC)
);

INSERT INTO t1 VALUES (3,'ksatefqs'),
(4,'sate'),(5,'a');

CREATE TABLE t2 (
  pk INTEGER NOT NULL,
  col_int_key INTEGER DEFAULT NULL,
  col_varchar_key varchar(10) DEFAULT NULL
);
INSERT INTO t2 VALUES
(10,80,'ukqukkoe'),
(11,2,'kqukkoe'),
(12,5,'qukkoeiws'),
(13,9,'ukko'),
(14,3,'kkoeiwsgp');
ANALYZE TABLE t1, t2;

let $query= SELECT t1.pk, t2.col_int_key, 
           t1.col_varchar_key, t2.col_varchar_key
           FROM t2 JOIN t1 ON ( t1.pk >= t2.col_int_key
           AND t1.col_varchar_key != t2.col_varchar_key );

--replace_column 10 # 11 #
eval EXPLAIN $query;
--sorted_result
eval $query;

let $query= SELECT STRAIGHT_JOIN t1.pk, t2.col_int_key,
            t1.col_varchar_key, t2.col_varchar_key FROM
            t2 JOIN t1 ON ( t1.pk >= t2.col_int_key AND 
            t1.col_varchar_key != t2.col_varchar_key );

--replace_column 10 # 11 #
eval EXPLAIN $query;
--sorted_result
eval $query;

DROP TABLE t1,t2;

--echo #End of test for Bug#23576305

--echo #
--echo # Bug#23730559: ASSERTION `TAB->QUICK() == SAVE_QUICK ||
--echo #               TAB->QUICK() == __NULL' FAILED.
--echo #

--echo #

CREATE TABLE b (
  col_int INTEGER NOT NULL,
  col_int_key INTEGER NOT NULL,

  col_varchar_key VARCHAR(20) NOT NULL,
  col_varchar VARCHAR(20) NOT NULL,

  KEY (col_varchar_key DESC),
  KEY (col_varchar_key(5) DESC),
  KEY (col_varchar_key, col_int_key)
) ENGINE=InnoDB;

INSERT INTO b ( col_int_key, col_int, col_varchar_key, col_varchar) VALUES
(1, 3, 'xceksatefqsdksjijc', 'xceksatefqsdksjijc'),
(7, 6, 'ce', 'ce'),
(2, 3, 'eksatefqsdksjij', 'eksatefqsdksjij'),
(5, 7, 'satefqsd', 'satefqsd');

CREATE TABLE bb (
  col_int INTEGER NOT NULL,
  col_int_key INTEGER NOT NULL,
  col_varchar_key VARCHAR(20) NOT NULL,
  col_varchar VARCHAR(20) NOT NULL,

  KEY (col_varchar_key(10) DESC, col_int_key DESC)
) ENGINE=InnoDB;

INSERT INTO bb ( col_int_key, col_int, col_varchar_key, col_varchar) VALUES
(181, 88, 'kkoeiwsgpmfyvvuqvtjn', 'kkoeiwsgpmfyvvuqvtjn'),
(3, 4, 'koeiwsgpmfyv', 'koeiwsgpmfyv'),
(86, 113, 'oeiwsgpm', 'oeiwsgpm'),
(6, 1, 'eiwsgpmfyvvuqvtjncds', 'eiwsgpmfyvvuqvtjncds'),
(8, 5, 'iwsgpmfyvvuqv', 'iwsgpmfyvvuqv');

ANALYZE TABLE b,bb;

EXPLAIN SELECT  gp1 . col_varchar AS g1 
FROM b AS gp1 LEFT JOIN bb AS gp2 USING ( col_varchar_key ) 
WHERE gp1 . col_int IN ( 
  SELECT  p1 . col_int AS p1 
  FROM bb AS p1 LEFT JOIN bb AS p2 
  ON ( p1 . col_int >= p2 . col_int_key ) 
  WHERE ( p1 . col_int , gp1 . col_int ) IN ( 
    SELECT  c1 . col_int AS C1 
    , c1 . col_int AS C2 
    FROM b AS c1 LEFT JOIN bb AS c2 USING ( col_varchar ) 
    WHERE ( gp1 . col_varchar_key >= 'n' )  
  ) 
  AND ( gp1 . col_varchar < 'e' )  
) 
AND ( gp1 . col_varchar_key <> 'y' )  
ORDER BY gp1 . col_varchar_key LIMIT 4;

SELECT  gp1 . col_varchar AS g1 
FROM b AS gp1 LEFT JOIN bb AS gp2 USING ( col_varchar_key ) 
WHERE gp1 . col_int IN ( 
  SELECT  p1 . col_int AS p1 
  FROM bb AS p1 LEFT JOIN bb AS p2 
  ON ( p1 . col_int >= p2 . col_int_key ) 
  WHERE ( p1 . col_int , gp1 . col_int ) IN ( 
    SELECT  c1 . col_int AS C1 
    , c1 . col_int AS C2 
    FROM b AS c1 LEFT JOIN bb AS c2 USING ( col_varchar ) 
    WHERE ( gp1 . col_varchar_key >= 'n' )  
  ) 
  AND ( gp1 . col_varchar < 'e' )  
) 
AND ( gp1 . col_varchar_key <> 'y' )  
ORDER BY gp1 . col_varchar_key LIMIT 4;

DROP TABLE b,bb;

--echo #
--echo #  Bug#23759797: DESC INDEX BACKWARD SCAN SHOWS WRONG RESULTS
--echo #
CREATE TABLE t1 (
  col_varchar_255_latin1_key varchar(255) DEFAULT NULL,
  pk int(11) NOT NULL AUTO_INCREMENT,
  KEY (pk DESC),
  KEY k3 (col_varchar_255_latin1_key DESC)
);
INSERT INTO t1(col_varchar_255_latin1_key, pk) VALUES
('l',4), ('something',3), ('ycyoybhug',2), ('l',5), ('my',1),
('l',4),('l',4),('l',4),('l',4),('l',4),('l',4),
('l',4),('l',4),('l',4),('l',4),('l',4),('l',4),
('l',4),('l',4),('l',4),('l',4),('l',4),('l',4),
('l',4),('l',4),('l',4),('l',4),('l',4),('l',4);
ANALYZE TABLE t1;

SELECT
t1 . pk AS field1,
t1 . pk AS field2
FROM  t1  LEFT  JOIN t1 AS t2
ON  t1 . col_varchar_255_latin1_key =  t2 . col_varchar_255_latin1_key
WHERE t1 . pk <> 4
ORDER BY field1, field2 DESC;

--replace_column 10 # 11 #
EXPLAIN SELECT
t1 . pk AS field1,
t1 . pk AS field2
FROM  t1  LEFT  JOIN t1 AS t2
ON  t1 . col_varchar_255_latin1_key =  t2 . col_varchar_255_latin1_key
WHERE t1 . pk <> 4
ORDER BY field1, field2 DESC;

DROP TABLE t1;

CREATE TABLE t1 (
  pk int(11) NOT NULL AUTO_INCREMENT,
  col_int_key int(11) DEFAULT NULL,
  PRIMARY KEY (pk DESC),
  KEY k3 (col_int_key)
);

INSERT INTO t1 VALUES (25,9), (24,1), (23,-74383360), (22,-855900160),
(21,NULL), (20,1596522496), (19,9), (18,1), (17,NULL), (16,NULL),
(15,1808465920), (14,NULL), (13,588644352), (12,3), (11,6), (10,NULL),
(9,NULL), (8,8), (7,NULL), (6,NULL), (5,-1018232832), (4,5), (3,NULL),
(2,NULL), (1,NULL);
ANALYZE TABLE t1;

SELECT DISTINCT
t1 . pk AS field1
FROM  t1  LEFT  JOIN t1 AS t2
ON  t1 . pk =  t2 . pk
WHERE ( t1 . col_int_key IS  NULL AND t1 . pk != 4 )
GROUP BY field1
HAVING field1 != 6
ORDER BY field1 ASC;

EXPLAIN SELECT DISTINCT
t1 . pk AS field1
FROM t1  LEFT  JOIN t1 AS t2
ON  t1 . pk =  t2 . pk
WHERE ( t1 . col_int_key IS  NULL AND t1 . pk != 4 )
GROUP BY field1
HAVING field1 != 6
ORDER BY field1 ASC;

EXPLAIN UPDATE t1
SET t1.pk = pk + 1000
WHERE ( t1 . col_int_key IS  NULL AND t1 . pk != 4 )
ORDER BY pk ASC LIMIT 3;

DROP TABLE t1;
--echo #

--echo #
--echo # Bug #23738137: WL1074:RESULT DIFFERENCE SEEN FOR
--echo #                QUERY WITH OR IN JOIN CONDITION
--echo #

CREATE TABLE t1 (
pk INTEGER NOT NULL,
col_int_key INTEGER DEFAULT NULL,
col_varchar_key varchar(20) DEFAULT NULL,
col_varchar varchar(20) DEFAULT NULL,
PRIMARY KEY (pk DESC),
KEY col_int_key (col_int_key DESC),
KEY col_varchar_key (col_varchar_key DESC)
) charset latin1;

INSERT INTO t1 VALUES (20,8,'eiw','eiw'),(19,8,'oeiws','oeiws'),
(18,NULL,'koeiw','koeiw'),(17,3,'kkoei','kkoei'),
(16,9,'ukkoe','ukkoe'),(15,5,'qukko','qukko'),
(14,2,'kqukk','kqukk'),(13,80,'ukquk','ukquk'),
(12,5,'lukqu','lukqu'),(10,NULL,'alukq','alukq'),
(9,3,'maluk','maluk'),(8,NULL,NULL,NULL),
(7,9,'ymalu','ymalu'),(6,3,'kymal','kymal'),
(5,6,'vkyma','vkyma'),(4,8,'vvkym','vvkym'),
(3,3,'jjvvk','jjvvk'),(1,5,'bjjvv','bjjvv');

ANALYZE TABLE t1;

let $query= SELECT STRAIGHT_JOIN count(t1.col_varchar)  FROM t1 JOIN t1 AS t2 ON
(t2.pk = t1.col_int_key) OR (t2.col_varchar_key = t1.col_varchar_key);

eval EXPLAIN $query;
eval $query;

let $query= SELECT count(t1.col_varchar) FROM t1 JOIN t1 AS t2 ON (t2.pk = t1.col_int_key)
OR (t2.col_varchar_key = t1.col_varchar_key);

eval EXPLAIN $query;
eval $query;

DROP TABLE t1;

--echo # End of test for Bug#23738137

--echo #
--echo # Bug#24294552:MULTI KEY DESC INDEX ON GCOL GIVES INCORRECT RESULTS
--echo #
CREATE TABLE t2 (
  col_int int(11) DEFAULT NULL,
  pk int(11) NOT NULL AUTO_INCREMENT,
  col_int_key int(11),
  PRIMARY KEY (pk DESC),
  KEY k2 (col_int_key, col_int DESC)
);

INSERT INTO
t2(col_int,col_int_key) VALUES (1,2), (2,4), (3,6), (4,8), (5,10), (6,12),
(7,14), (8,16), (9,18);

CREATE TABLE t1 (
  col_int int(11) DEFAULT NULL,
  pk int(11) NOT NULL,
  col_int_key int(11)
);

INSERT INTO t1(pk, col_int, col_int_key) VALUES (4,3,6), (5,4,8), (6,2,4);
ANALYZE TABLE t1, t2;

SELECT STRAIGHT_JOIN
t1 . col_int_key AS field1 ,
t2 . col_int AS field2
FROM  t1  LEFT  JOIN t2 FORCE INDEX(k2)
ON  t1 . col_int =  t2 . col_int
WHERE ( t2 . col_int_key <= t1 . col_int_key AND t1 . pk >= t2 . pk )
ORDER BY field1, field2 DESC;

--replace_column 10 # 11 #
EXPLAIN SELECT STRAIGHT_JOIN
t1 . col_int_key AS field1 ,
t2 . col_int AS field2
FROM  t1  LEFT  JOIN t2 force index(k2)
ON  t1 . col_int =  t2 . col_int
WHERE ( t2 . col_int_key <= t1 . col_int_key AND t1 . pk >= t2 . pk )
ORDER BY field1, field2 DESC;

DROP TABLE t1,t2;

--echo #

--echo #
--echo # Bug#24300848:WL1074: INNODB: ASSERTION FAILURE:
--echo #              BTR0PCUR.CC:268:CURSOR->OLD_REC
--echo #

CREATE TABLE t1(col1 int , col2 int, PRIMARY KEY (col1 DESC))
PARTITION BY RANGE (col1) (PARTITION p0 VALUES LESS THAN (5));

INSERT INTO t1 VALUES(1, 10);

SELECT * FROM  t1 WHERE col1 IN (1, 2);

DROP TABLE t1;

--echo # End of test for Bug#24300848

--echo #
--echo # Bug#24431177: WL1074:LEFT JOIN QUERY USING INDEX SHOWS
--echo #               WRONG QEP AND RESULTS ON 2ND EXECUTION
--echo #

CREATE TABLE t1(
pk INTEGER NOT NULL AUTO_INCREMENT,
col_int_key INTEGER DEFAULT NULL,
PRIMARY KEY (pk DESC),
KEY col_int_key (col_int_key DESC)
);
INSERT INTO t1 VALUES (3,15),(6,8),(20,6),(15,6),(18,5),(17,5),
(16,5),(13,5),(12,5),(9,5),(8,5),(7,5),(5,5),(11,4),(4,4),
(19,3),(10,2),(1,2),(14,1),(2,1);

ANALYZE TABLE t1;

let $query=SELECT * FROM t1 WHERE pk IN (6,2)
                               OR (col_int_key >= 7 AND col_int_key < 13);
eval EXPLAIN $query;
--sorted_result
eval $query;

ALTER TABLE t1 ADD INDEX key1 (pk);
ALTER TABLE t1 ADD INDEX key2 (col_int_key);

ANALYZE TABLE t1;

let $query=SELECT * FROM t1 FORCE INDEX (key1,col_int_key) WHERE pk IN (6,2)
                               OR (col_int_key >= 7 AND col_int_key < 13);
eval EXPLAIN $query;
--sorted_result  # The hypergraph optimizer doesn't support FORCE INDEX.
eval $query;

DROP TABLE t1;

--echo # End of test for Bug#24431777

#Test for checking clone_first/clone_last for desc indexes

CREATE TABLE t1(
a INTEGER NOT NULL,
b INTEGER NOT NULL,
KEY ab (a DESC,b DESC)
);

#More values are required to force range than index
INSERT INTO t1 VALUES (78,7),(78,6),(70,1),(47,1),(15,4),(15,1),
(10,6),(3,6),(2,56),(2,6),(1,56);

SELECT * FROM t1 WHERE (
    ( b =1 AND a BETWEEN 14 AND 21 ) OR
    ( b =2 AND a BETWEEN 16 AND 18 ) OR
    ( b =3 AND a BETWEEN 15 AND 19 ) OR
    (a BETWEEN 19 AND 47) );

DROP TABLE t1;

--echo #
--echo # Bug #25899921: INCORRECT BEHAVIOR WITH DESC INDEX AND
--echo #                IMPOSSIBLE CONDITION
--echo #

CREATE TABLE t1 (a INT, b DATE, KEY(b,a DESC));
SET @g:='1';
--error ER_TRUNCATED_WRONG_VALUE
DELETE FROM t1 WHERE b=@g ORDER BY b, a LIMIT 1;
DROP TABLE t1;

--echo # End of test for Bug#25899921
