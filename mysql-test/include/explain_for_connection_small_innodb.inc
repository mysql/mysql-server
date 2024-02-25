--source include/have_debug.inc
--source include/have_debug_sync.inc
--source include/not_windows.inc
--source include/have_grep.inc

--disable_query_log
--disable_result_log
--disable_warnings

connect (ce, localhost, root,,test);
connect (cq, localhost, root,,test);
connection cq;
SET GLOBAL innodb_stats_persistent=on;
  

CREATE TABLE t1 (a int PRIMARY KEY, b varchar(128), KEY (b)) ENGINE = InnoDB PARTITION BY HASH (a) PARTITIONS 13 ;
CREATE TABLE t2 (a int PRIMARY KEY AUTO_INCREMENT, b varchar(128)) ENGINE = InnoDB PARTITION BY HASH (a) PARTITIONS 13 ;
INSERT INTO t1 VALUES (1, 'First row, p1') ;
INSERT INTO t1 VALUES (0, 'First row, p0'), (2, 'First row, p2'), (3, 'First row, p3'), (4, 'First row, p4') ;
INSERT INTO t1 VALUES (1 * 13, 'Second row, p0'), (2 * 13, 'Third row, p0'), (3 * 13, 'Fourth row, p0'), (4 * 13, 'Fifth row, p0') ;
INSERT INTO t2 VALUES (NULL, 'First auto-inc row') ;
INSERT INTO t2 (b) VALUES ('Second auto-inc row') ;
INSERT INTO t2 VALUES (10, 'First row, p10') ;
UPDATE t2 SET b = CONCAT(b, ', UPDATED') WHERE a = 10 ;
INSERT INTO t1 VALUES (65, 'No duplicate') ON DUPLICATE KEY UPDATE b = CONCAT(b, ', INSERT_DUP_KEY_UPDATE') ;
INSERT INTO t1 VALUES (65, 'No duplicate') ON DUPLICATE KEY UPDATE b = CONCAT(b, ', INSERT_DUP_KEY_UPDATE') ;
INSERT INTO t1 VALUES (78, 'No duplicate') ON DUPLICATE KEY UPDATE a = a + 13, b = CONCAT(b, ', INSERT_DUP_KEY_UPDATE') ;
INSERT INTO t1 VALUES (78, 'No duplicate') ON DUPLICATE KEY UPDATE a = a + 13, b = CONCAT(b, ', INSERT_DUP_KEY_UPDATE') ;
INSERT INTO t1 VALUES (78, 'No duplicate') ON DUPLICATE KEY UPDATE a = a + 13, b = CONCAT(b, ', INSERT_DUP_KEY_UPDATE third') ;
INSERT INTO t1 VALUES (104, 'No duplicate') ON DUPLICATE KEY UPDATE a = a + 1 ;
INSERT INTO t1 VALUES (104, 'No duplicate') ON DUPLICATE KEY UPDATE a = a + 1 ;
INSERT INTO t1 VALUES (104, 'No duplicate 104') ON DUPLICATE KEY UPDATE a = a + 1 ;
REPLACE INTO t1 VALUES (5, 'REPLACE first') ;
REPLACE INTO t1 VALUES (5, 'REPLACE second') ;
UPDATE t1 SET b = CONCAT(b, ', updated 1') WHERE a IN (13, 26, 39, 52) ;
UPDATE t1 SET a = 99, b = CONCAT(b, ', updated 2 -> p8') WHERE a = 13 ;
UPDATE t1 SET a = 13 + 99, b = CONCAT(b, ', updated 3') WHERE a = 99 ;
UPDATE t1 SET a = a + 1, b = CONCAT(b, ', updated 4 -> p9') WHERE a = 112 ;
UPDATE t1 SET b = CONCAT(b, ', same as min(a) + 2 in t2') WHERE a = (SELECT MIN(a) + 2 FROM t2) ;

let $point=   planned_single_update;let $query=  UPDATE t1 SET b = CONCAT(b, ', max(a) in t2: ', (SELECT MAX(a) FROM t2)) WHERE a = 5 ;
--source include/explain_for_connection.inc
let $point=   before_reset_query_plan;let $query=  UPDATE t1, t2 SET t1.b = CONCAT(t1.b, ', t2.b:', t2.b), t2.b = CONCAT(t2.b, ', t1.b:', t1.b) WHERE t2.b = t1.b and t2.a = 4 ;
--source include/explain_for_connection.inc

DROP TABLE t1,t2;

CREATE TABLE t1 ( t3_id int NOT NULL, t1_id int NOT NULL, PRIMARY KEY  (t1_id) ) ;
CREATE TABLE t2 ( t2_id int NOT NULL, t1_id int NOT NULL, b int NOT NULL, PRIMARY KEY  (t2_id), UNIQUE KEY idx_t2_t1_b (t1_id,b) ) ENGINE=InnoDB ;
CREATE TABLE t3 ( t3_id int NOT NULL ) ;
INSERT INTO t3 VALUES (3) ;
let $point=   before_reset_query_plan;let $query=  SELECT (SELECT rs.t2_id FROM t2 rs WHERE rs.t1_id= (SELECT lt.t1_id FROM t1 lt WHERE lt.t3_id=a.t3_id) ORDER BY b DESC LIMIT 1) from t3 AS a ;
--source include/explain_for_connection.inc
DROP PROCEDURE IF EXISTS p1 ;
DROP TABLE t1,t2,t3 ;

###############################
#MTR FOR BUG#14581870         #
###############################

CREATE TABLE a1 (
  col_int int(11) DEFAULT NULL,
  pk int(11) NOT NULL AUTO_INCREMENT,
  PRIMARY KEY (pk)
) DEFAULT CHARSET=latin1;

 CREATE TABLE aa1 (
  col_int int(11) DEFAULT NULL,
  pk int(11) NOT NULL AUTO_INCREMENT,
  PRIMARY KEY (pk)
) DEFAULT CHARSET=latin1;

let $query=SELECT DISTINCT   alias2.col_int AS field1 , alias2.col_int AS field2 FROM  a1 AS alias1 LEFT  JOIN aa1 AS alias2 ON  alias1.pk =  alias2.pk WHERE ( alias1.col_int = 8 OR alias1 .pk <= alias1.col_int ) GROUP BY field1, field2  ORDER BY field1, field2 DESC;
--source include/explain_for_connection.inc

DROP TABLE a1;
DROP TABLE aa1;

###############################
#MTR FOR BUG#14586538         #
###############################

CREATE TABLE h2 (
pk int(11) NOT NULL AUTO_INCREMENT,
PRIMARY KEY (pk)
) AUTO_INCREMENT=10 DEFAULT CHARSET=latin1;

insert into h2 values (1),(2),(3),(4),(5),(6),(7),(8),(9);

ANALYZE TABLE h2;

CREATE TABLE aa3 (
  col_int_key int(11) DEFAULT NULL,
  KEY col_int_key (col_int_key)
 ) DEFAULT CHARSET=latin1;

let $query=SELECT alias1.pk AS field1 FROM  h2 AS alias1 LEFT  JOIN aa3 AS alias2 ON  alias1.pk =  alias2.col_int_key WHERE alias1.pk <> 9 GROUP BY field1 ORDER BY field1 LIMIT 1 OFFSET 3;
--source include/explain_for_connection.inc

DROP TABLE h2;
DROP TABLE aa3;

###############################
#MTR FOR BUG#14591376         #
###############################

CREATE TABLE e1 (
  col_int int(11) DEFAULT NULL,
  pk int(11) NOT NULL AUTO_INCREMENT,
  col_int_key int(11) DEFAULT NULL,
  PRIMARY KEY (pk),
  KEY col_int_key (col_int_key)
) AUTO_INCREMENT=7 DEFAULT CHARSET=latin1;

insert into e1 values (1, NULL,NULL);
insert into e1 values (2,6,NULL);
insert into e1 values (3,8,-1131610112);
insert into e1 values (4,2,-1009057792);
insert into e1 values (5,-1220345856,1);
insert into e1 values (6,NULL,-185204736);

ANALYZE TABLE e1;

CREATE TABLE cc2 (
  col_int int(11) DEFAULT NULL
 ) AUTO_INCREMENT=3 DEFAULT CHARSET=latin1;

insert into cc2 values ( 685899776),(NULL);

ANALYZE TABLE CC2;

let $query=SELECT    alias1.pk AS field1 FROM  e1 AS alias1 RIGHT  JOIN cc2 AS alias2 ON  alias1.col_int =  alias2.col_int WHERE alias1.col_int_key  IN (8) ;
--source include/explain_for_connection.inc

DROP TABLE e1;
DROP TABLE cc2;

###############################
#MTR FOR Bug#15915676         #
###############################

CREATE TABLE t1 (
  pk INT NOT NULL,
  col_int INT,
  col_varchar_key VARCHAR(10),
  PRIMARY KEY (pk),
  KEY col_varchar_key (col_varchar_key)
) ;

CREATE TABLE t2 (
  pk INT NOT NULL,
  col_varchar_key VARCHAR(10),
  PRIMARY KEY (pk),
  KEY col_varchar_key (col_varchar_key)
) ;

let $query=SELECT DISTINCT alias2.pk AS field1, alias2.pk AS field2, alias2.pk AS field3 FROM t1 AS alias1 RIGHT JOIN t2 AS alias2  ON  alias1.col_varchar_key =  alias2.col_varchar_key WHERE alias1.col_int >alias1.col_int HAVING field3 = 7 ORDER BY field1, field2, field3 LIMIT 10 OFFSET 1;

--source include/explain_for_connection.inc
--enable_warnings
drop table t1;
drop table t2;
