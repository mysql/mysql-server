--source include/not_hypergraph.inc  # Non-traditional formats differ in hypergraph.

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

  --source include/force_myisam_default.inc
  --source include/have_myisam.inc

  let $point=   before_reset_query_plan;
  SET @save_log_bin= @@sql_log_bin;
  SET SQL_LOG_BIN=0;
  create table t1 (a char(10) character set koi8r) ;
  insert ignore into t1 values ('ÔÅÓÔ') ;
  let $point=   before_reset_query_plan;let $query=  DELETE from t1 ;
  --source include/explain_for_connection.inc
  drop table t1 ;
  SET SQL_LOG_BIN=@save_log_bin;
  CREATE TABLE t1 (a int not null,b int not null) ;
  CREATE TABLE t2 (a int not null, b int not null, primary key (a,b)) ;
  CREATE TABLE t3 (a int not null, b int not null, primary key (a,b)) ;
  insert into t1 values (1,1),(2,1),(1,3) ;
  insert into t2 values (1,1),(2,2),(3,3) ;
  insert into t3 values (1,1),(2,1),(1,3) ;
  let $point=   before_reset_query_plan;let $query=  SELECT * from t1,t2,t3 where t1.a=t2.a AND t2.b=t3.a and t1.b=t3.b ;
  --source include/explain_for_connection.inc
#  let $point=   planned_single_delete;let $query=  DELETE t2.*,t3.* from t1,t2,t3 where t1.a=t2.a AND t2.b=t3.a and t1.b=t3.b ;
#  --source include/explain_for_connection.inc
  drop table t1,t2,t3 ;




  CREATE TABLE t1 ( f1 int(11) DEFAULT NULL ) ;
  let $point=   after_join_optimize;let $query=  SELECT 1 FROM ( SELECT 1, 2 FROM DUAL WHERE EXISTS  ( SELECT f1 FROM t1 )) AS tt ;
  --source include/explain_for_connection.inc
  DROP TABLE t1 ;
  

  CREATE TABLE t1 (a int, b int, PRIMARY KEY (a)) ;
  INSERT INTO t1 VALUES (1,1),(2,1),(3,1),(4,1),(5,1),(6,1) ;
  CREATE TABLE t2 (a int, b int, PRIMARY KEY (a)) ;
  INSERT INTO t2 VALUES (3,2),(4,2),(100,100),(101,201),(102,102) ;
  CREATE TABLE t3 (a int PRIMARY KEY) ;
  INSERT INTO t3 VALUES (1),(2),(3),(4) ;
  CREATE TABLE t4 (a int PRIMARY KEY,b int) ;
  INSERT INTO t4 VALUES (1,1),(2,2),(1000,1000),(1001,1001),(1002,1002), (1003,1003),(1004,1004) ;
  let $point=   before_reset_query_plan;let $query=  SELECT STRAIGHT_JOIN * FROM t3  JOIN t1 ON t3.a=t1.a  JOIN t2 ON t3.a=t2.a JOIN t4 WHERE t4.a IN (t1.b, t2.b) ;
  --source include/explain_for_connection.inc
  let $point=   before_reset_query_plan;let $query=  SELECT STRAIGHT_JOIN  (SELECT SUM(t4.a) FROM t4 WHERE t4.a IN (t1.b, t2.b))  FROM t3, t1, t2 WHERE t3.a=t1.a AND t3.a=t2.a ;
  --source include/explain_for_connection.inc
  DROP TABLE t1,t2,t3,t4 ;


  CREATE TABLE t1 (a INT, b INT, INDEX (a,b)) ;
  INSERT INTO t1 (a, b) VALUES (1,1), (1,2), (1,3), (1,4), (1,5),
                               (2,2), (2,3), (2,1), (3,1), (4,1),
                               (4,2), (4,3), (4,4), (4,5), (4,6),
                               (5,1), (5,2), (5,3), (5,4), (5,5);
  CREATE TABLE t2 SELECT max(b), a FROM t1 GROUP BY a ;
  let $point=   before_reset_query_plan;let $query=  SELECT * FROM (SELECT max(b), a FROM t1 GROUP BY a) b ;
  --source include/explain_for_connection.inc
  CREATE TABLE t3 LIKE t1 ;
  INSERT INTO t3 SELECT a,MAX(b) FROM t1 GROUP BY a ;
  DELETE FROM t3 ;
    INSERT INTO t3 SELECT 1, (SELECT MAX(b) FROM t1 GROUP BY a HAVING a < 2)  FROM t1 LIMIT 1 ;
  let $point=   planned_single_delete;let $query=  DELETE FROM t3 WHERE (SELECT MAX(b) FROM t1 GROUP BY a HAVING a < 2) > 10000 ;
  --source include/explain_for_connection.inc
  DROP TABLE t1,t2,t3 ;




  CREATE TABLE t1 ( ID int(10) unsigned NOT NULL AUTO_INCREMENT, col1 int(10) unsigned DEFAULT NULL, key1 int(10) unsigned NOT NULL DEFAULT '0', key2 int(10) unsigned DEFAULT NULL, text1 text, text2 text, col2 smallint(6) DEFAULT '100', col3 enum('headers','bodyandsubject') NOT NULL DEFAULT 'bodyandsubject', col4 tinyint(3) unsigned NOT NULL DEFAULT '0', PRIMARY KEY (ID), KEY (key1), KEY (key2) ) AUTO_INCREMENT=6 DEFAULT CHARSET=utf8 ;
  INSERT INTO t1 VALUES (1,NULL,1130,NULL,'Hello',NULL,100,'bodyandsubject',0), (2,NULL,1130,NULL,'bye',NULL,100,'bodyandsubject',0), (3,NULL,1130,NULL,'red',NULL,100,'bodyandsubject',0), (4,NULL,1130,NULL,'yellow',NULL,100,'bodyandsubject',0), (5,NULL,1130,NULL,'blue',NULL,100,'bodyandsubject',0) ;
  let $point=   before_reset_query_plan;let $query=  SELECT * FROM t1 WHERE key1=1130 AND col1 IS NULL ORDER BY text1 ;
  --source include/explain_for_connection.inc
  drop table t1 ;



  CREATE TABLE t1 ( c1 INT NOT NULL, c2 VARCHAR(1) DEFAULT NULL, PRIMARY KEY (c1) ) ;
  CREATE TABLE t2 ( c1 INT NOT NULL, c2 VARCHAR(1) DEFAULT NULL, PRIMARY KEY (c1) ) ;
  INSERT INTO t2 VALUES (10,'v') ;
  INSERT INTO t2 VALUES (11,'r') ;
  let $point=  after_join_optimize;let $query=  SELECT t1.c2   FROM t2 STRAIGHT_JOIN t1 ON t1.c1 < t2.c1 ;
  --source include/explain_for_connection.inc
  DROP TABLE t1, t2 ;


  CREATE TABLE ten (a INTEGER) ;
  INSERT INTO ten VALUES (0),(1),(2),(3),(4),(5),(6),(7),(8),(9) ;
  CREATE TABLE t1 ( pk INTEGER NOT NULL, i1 INTEGER NOT NULL, c1 VARCHAR(10) NOT NULL, PRIMARY KEY (pk) ) ;
  INSERT INTO t1 SELECT a, 1, 'MySQL' FROM ten ;
  CREATE TABLE t2 ( pk INTEGER NOT NULL, c1 VARCHAR(10) NOT NULL, c2 varchar(10) NOT NULL, PRIMARY KEY (pk) ) charset latin1;
  INSERT INTO t2 SELECT a, 'MySQL', 'MySQL' FROM ten ;
  CREATE TABLE t3 ( pk INTEGER NOT NULL, c1 VARCHAR(10) NOT NULL, PRIMARY KEY (pk) ) charset latin1;
  INSERT INTO t3 SELECT a, 'MySQL' FROM ten ;
  CREATE TABLE t4 ( pk int(11) NOT NULL, c1_key varchar(10) CHARACTER SET utf8 NOT NULL, c2 varchar(10) NOT NULL, c3 varchar(10) NOT NULL, PRIMARY KEY (pk), KEY k1 (c1_key) ) charset latin1;
  CREATE TABLE t5 ( pk INTEGER NOT NULL, c1 VARCHAR(10) NOT NULL, PRIMARY KEY (pk) ) charset latin1;
  INSERT INTO t5 SELECT a, 'MySQL' FROM ten ;
  let $point=   after_join_optimize;let $query=  SELECT STRAIGHT_JOIN * FROM (t1 LEFT JOIN  (t2 LEFT JOIN (t3 LEFT OUTER JOIN t4 ON t3.c1 <= t4.c1_key) ON t2.c1 = t4.c3)  ON t1.c1 = t4.c2) RIGHT OUTER JOIN t5 ON t2.c2 <= t5.c1 WHERE t1.i1 = 1 ;
  --source include/explain_for_connection.inc
  DROP TABLE ten, t1, t2, t3, t4, t5 ;



  CREATE TABLE t1 (f1 INT NOT NULL, PRIMARY KEY (f1)) ;
  CREATE TABLE t2 (f1 INT NOT NULL, f2 INT NOT NULL, PRIMARY KEY (f1, f2)) ;
  INSERT INTO t1 VALUES (4) ;
  INSERT INTO t2 VALUES (3, 3) ;
  INSERT INTO t2 VALUES (7, 7) ;
  let $point=   before_reset_query_plan;let $query=  SELECT * FROM t1 LEFT JOIN t2 ON t2.f1 = t1.f1 WHERE t1.f1 = 4 AND t2.f1 IS NOT NULL AND t2.f2 IS NOT NULL GROUP BY t2.f1, t2.f2 ;
  --source include/explain_for_connection.inc
  DROP TABLE t1,t2 ;






  CREATE TABLE t1 (id INT NOT NULL, ref INT NOT NULL, INDEX (id)) ENGINE=MyISAM ;
  CREATE TABLE t2 LIKE t1 ;
  INSERT INTO t2 (id, ref) VALUES (1,3), (2,1), (3,2), (4,5), (4,4) ;
  INSERT INTO t1 SELECT * FROM t2 ;
  INSERT INTO t1 SELECT * FROM t2 ;
  CREATE TABLE t3 (id INT NOT NULL, ref INT NOT NULL, INDEX (id)) ENGINE=MERGE UNION(t1) ;
  let $point=   before_reset_query_plan;let $query=  SELECT * FROM t3 AS a INNER JOIN t3 AS b USING (id) WHERE a.ref < b.ref ;
  --source include/explain_for_connection.inc
  let $point=   before_reset_query_plan;let $query=  DELETE FROM a USING t3 AS a INNER JOIN t3 AS b USING (id) WHERE a.ref < b.ref ;
  --source include/explain_for_connection.inc
  DROP TABLE t1, t2, t3 ;




  create table t1 (n numeric(10)) ;
  create table t2 (n numeric(10)) ;
  insert into t2 values (1),(2),(4),(8),(16),(32) ;
  let $point=   before_reset_query_plan;let $query=  SELECT * from t2 left outer join t1  using (n) ;
  --source include/explain_for_connection.inc
  let $point=   before_reset_query_plan;let $query=  DELETE  t1,t2 from t2 left outer join t1  using (n) ;
  --source include/explain_for_connection.inc
  drop table t1,t2 ;




  create table t1 (a int, b int) ;
  create table t2 (a int, b int) ;
  insert into t1 values (1,1),(2,1),(3,1) ;
  insert into t2 values (1,1), (3,1) ;
  let $point=   before_reset_query_plan;let $query=  UPDATE t1 left join t2  on t1.a=t2.a set t1.b=2, t2.b=2 where t1.b=1 and t2.b=1 or t2.a is NULL ;
  --source include/explain_for_connection.inc
  drop table t1,t2 ;




 
  CREATE TABLE t1 (id INT NOT NULL AUTO_INCREMENT PRIMARY KEY) ;
  CREATE TABLE t2 (id INT, s1 CHAR(255)) ;
  INSERT INTO t1 VALUES (0), (0), (0), (0), (0), (0), (0), (0) ;
    INSERT INTO t1 (SELECT 0 FROM t1) ;
  INSERT INTO t1 (SELECT 0 FROM t1) ;
  INSERT INTO t1 (SELECT 0 FROM t1) ;
  INSERT INTO t2 (SELECT ID, 'a' FROM t1) ;
  let $point=   before_reset_query_plan;let $query=  UPDATE t1 LEFT JOIN t2 USING(id) SET s1 = 'b' ;
  --source include/explain_for_connection.inc
  DROP TABLE t1, t2 ;
  




  create table t1 (a int not null, b int, c varchar(10), key (a, b, c)) ;
    INSERT into t1 values (1, NULL, NULL), (1, NULL, 'b'), (1, 1, NULL), (1, 1, 'b'), (1, 1, 'b'), (2, 1, 'a'), (2, 1, 'b'), (2, 2, 'a'), (2, 2, 'b'), (2, 3, 'c'),(1,3,'b') ;
  let $point=   before_reset_query_plan;let $query=  SELECT * from t1 where a = 1 and b is null order by a desc, b desc ;
  --source include/explain_for_connection.inc
  drop table t1 ;




  create table t1(a int, b int, index(b)) ;
  insert into t1 values (2, 1), (1, 1), (4, NULL), (3, NULL), (6, 2), (5, 2) ;
  let $point=   before_reset_query_plan;let $query=  SELECT * from t1 where b=1 or b is null order by a ;
  --source include/explain_for_connection.inc
  drop table t1 ;



  CREATE TABLE t1 ( a INT, c INT, UNIQUE KEY a_c (a,c), KEY (a)) ;
  INSERT INTO t1 VALUES (1, 10), (2, NULL) ;
  let $point=   before_reset_query_plan;let $query=  SELECT 1 AS col FROM t1 WHERE a=2 AND (c=10 OR c IS NULL) ORDER BY c ;
  --source include/explain_for_connection.inc
  let $point=   before_reset_query_plan;let $query=  SELECT 1 AS col FROM t1 WHERE a=2 AND (c=10 OR c IS NULL) ORDER BY c DESC ;
  --source include/explain_for_connection.inc
  DROP TABLE t1 ;







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



  CREATE TABLE t1 ( OXID varchar(32) COLLATE latin1_german2_ci NOT NULL DEFAULT '', OXPARENTID varchar(32) COLLATE latin1_german2_ci NOT NULL DEFAULT 'oxrootid', OXLEFT int NOT NULL DEFAULT '0', OXRIGHT int NOT NULL DEFAULT '0', OXROOTID varchar(32) COLLATE latin1_german2_ci NOT NULL DEFAULT '', PRIMARY KEY  (OXID), KEY OXNID (OXID), KEY OXLEFT (OXLEFT), KEY OXRIGHT (OXRIGHT), KEY OXROOTID (OXROOTID) ) ENGINE=MyISAM DEFAULT CHARSET=latin1 COLLATE=latin1_german2_ci ;
  INSERT INTO t1 VALUES ('d8c4177d09f8b11f5.52725521','oxrootid',1,40,'d8c4177d09f8b11f5.52725521'), ('d8c4177d151affab2.81582770','d8c4177d09f8b11f5.52725521',2,3, 'd8c4177d09f8b11f5.52725521'), ('d8c4177d206a333d2.74422679','d8c4177d09f8b11f5.52725521',4,5, 'd8c4177d09f8b11f5.52725521'), ('d8c4177d225791924.30714720','d8c4177d09f8b11f5.52725521',6,7, 'd8c4177d09f8b11f5.52725521'), ('d8c4177d2380fc201.39666693','d8c4177d09f8b11f5.52725521',8,9, 'd8c4177d09f8b11f5.52725521'), ('d8c4177d24ccef970.14957924','d8c4177d09f8b11f5.52725521',10,11, 'd8c4177d09f8b11f5.52725521') ;
  let $replace=/"rows_produced_per_join": 5/"rows_produced_per_join": 4/ /"data_read_per_join": "560"/"data_read_per_join": "559"/;
  let $point=   before_reset_query_plan;let $query=  SELECT s.oxid FROM t1 v, t1 s  WHERE s.oxrootid = 'd8c4177d09f8b11f5.52725521' AND v.oxrootid ='d8c4177d09f8b11f5.52725521' AND s.oxleft > v.oxleft AND s.oxleft < v.oxright ;
  --source include/explain_for_connection.inc
  let $replace=;
  DROP TABLE t1 ;




  create table t1 (a int NOT NULL, b int, primary key (a)) ;
  create table t2 (a int NOT NULL, b int, primary key (a)) ;
  insert into t1 values (0, 10),(1, 11),(2, 12) ;
  insert into t2 values (1, 21),(2, 12),(3, 23) ;
  let $point=   before_reset_query_plan;let $query=  SELECT * from t1 where b = (select b from t2 where t1.a = t2.a) ;
  --source include/explain_for_connection.inc
  let $point=   planned_single_delete;let $query=  DELETE from t1 where b = (select b from t2 where t1.a = t2.a) ;
  --source include/explain_for_connection.inc
  drop table t1, t2 ;




SET sql_mode = 'NO_ENGINE_SUBSTITUTION';

  CREATE TABLE t1 ( mot varchar(30) character set latin1 NOT NULL default '', topic mediumint(8) unsigned NOT NULL default '0', date date NOT NULL default '0000-00-00', pseudo varchar(35) character set latin1 NOT NULL default '', PRIMARY KEY  (mot,pseudo,date,topic), KEY pseudo (pseudo,date,topic), KEY topic (topic) ) ENGINE=MyISAM CHARSET=latin1 ROW_FORMAT=DYNAMIC ;
  CREATE TABLE t2 ( mot varchar(30) character set latin1 NOT NULL default '', topic mediumint(8) unsigned NOT NULL default '0', date date NOT NULL default '0000-00-00', pseudo varchar(35) character set latin1 NOT NULL default '', PRIMARY KEY  (mot,pseudo,date,topic), KEY pseudo (pseudo,date,topic), KEY topic (topic) ) ENGINE=MyISAM CHARSET=latin1 ROW_FORMAT=DYNAMIC ;
  CREATE TABLE t3 ( numeropost mediumint(8) unsigned NOT NULL auto_increment, maxnumrep int(10) unsigned NOT NULL default '0', PRIMARY KEY  (numeropost), UNIQUE KEY maxnumrep (maxnumrep) ) ENGINE=MyISAM CHARSET=latin1 ;
  INSERT INTO t1 VALUES ('joce','1','','joce'),('test','2','','test') ;
  INSERT INTO t2 VALUES ('joce','1','','joce'),('test','2','','test') ;
  INSERT INTO t3 VALUES (1,1) ;
  let $point=   before_reset_query_plan;let $query=  SELECT DISTINCT topic FROM t2 WHERE NOT EXISTS(SELECT * FROM t3 WHERE numeropost=topic) ;
  --source include/explain_for_connection.inc

# Row-based binary logging adds the columns of t1's PK to the read_set;
# this is visible in the "used_columns" information of the JSON format.
# So, depending on the binary log format used to run this query, the
# JSON output is of varying length, which influences the line numbers in
# diff hunk headers like "@@ -12,3". As this test displays the diff,
# it can fail for certain binlog formats. Force one format:

set binlog_format=mixed;

  let $point=   before_reset_query_plan;let $query=  DELETE FROM t1 WHERE topic IN (SELECT DISTINCT topic FROM t2 WHERE NOT EXISTS(SELECT * FROM t3 WHERE numeropost=topic)) ;
  --source include/explain_for_connection.inc

set binlog_format=default;

drop table t1, t2, t3 ;

SET sql_mode = default;



  CREATE TABLE t1 ( id int(11) NOT NULL auto_increment, id_cns tinyint(3) unsigned NOT NULL default '0', tipo enum('','UNO','DUE') NOT NULL default '', anno_dep smallint(4) unsigned zerofill NOT NULL default '0000', particolare mediumint(8) unsigned NOT NULL default '0', generale mediumint(8) unsigned NOT NULL default '0', bis tinyint(3) unsigned NOT NULL default '0', PRIMARY KEY  (id), UNIQUE KEY idx_cns_gen_anno (anno_dep,id_cns,generale,particolare), UNIQUE KEY idx_cns_par_anno (id_cns,anno_dep,tipo,particolare,bis) ) ;
  INSERT INTO t1 VALUES (1,16,'UNO',1987,2048,9681,0),(2,50,'UNO',1987,1536,13987,0),(3,16,'UNO',1987,2432,14594,0),(4,16,'UNO',1987,1792,13422,0),(5,16,'UNO',1987,1025,10240,0),(6,16,'UNO',1987,1026,7089,0) ;
  CREATE TABLE t2 ( id tinyint(3) unsigned NOT NULL auto_increment, max_anno_dep smallint(6) unsigned NOT NULL default '0', PRIMARY KEY  (id) ) ;
  INSERT INTO t2 VALUES (16,1987),(50,1990),(51,1990) ;
  let $point=   before_reset_query_plan;let $query=  SELECT cns.id, cns.max_anno_dep, cns.max_anno_dep = (SELECT s.anno_dep FROM t1 AS s WHERE s.id_cns = cns.id ORDER BY s.anno_dep DESC LIMIT 1) AS PIPPO FROM t2 AS cns ;
  --source include/explain_for_connection.inc
  DROP TABLE t1, t2 ;




  CREATE TABLE t1 ( a varchar(240) default NULL, b timestamp NOT NULL default CURRENT_TIMESTAMP on update CURRENT_TIMESTAMP, INDEX idx(a,b) ) ;
  CREATE TABLE t2 ( a varchar(255) default NULL ) ;
  INSERT INTO t1 VALUES ('abcdefghijk','2007-05-07 06:00:24') ;
  INSERT INTO t1 SELECT * FROM t1 ;
  INSERT INTO t1 SELECT * FROM t1 ;
  INSERT INTO t1 SELECT * FROM t1 ;
  INSERT INTO t1 SELECT * FROM t1 ;
  INSERT INTO t1 SELECT * FROM t1 ;
  INSERT INTO t1 SELECT * FROM t1 ;
  INSERT INTO t1 SELECT * FROM t1 ;
  INSERT INTO t1 SELECT * FROM t1 ;
  INSERT INTO t1 VALUES ('asdf','2007-02-08 01:11:26') ;
  INSERT INTO t2 VALUES ('abcdefghijk') ;
  INSERT INTO t2 VALUES ('asdf') ;
  let $point=   before_reset_query_plan;let $query=  SELECT (SELECT 1 FROM  t1 WHERE t1.a=t2.a ORDER BY t1.b LIMIT 1) AS d1 FROM t2 ;
  --source include/explain_for_connection.inc
  DROP TABLE t1,t2 ;





  create table t2(i int) ;
  insert into t2 values(0) ;
  drop view if exists v1;	
  CREATE VIEW v1 AS   SELECT 'f' FROM t2 UNION SELECT 'x' FROM t2 ;
  CREATE TABLE t1 ( pk int NOT NULL, col_varchar_key varchar(1) DEFAULT NULL, PRIMARY KEY (pk), KEY col_varchar_key (col_varchar_key) ) ;
  let $point=   before_reset_query_plan;let $query=  SELECT t1.pk FROM t1 WHERE t1.col_varchar_key < ALL ( SELECT * FROM v1 ) ;
  --source include/explain_for_connection.inc
  drop table t2, t1 ;
  DROP VIEW v1;





  CREATE TABLE t1 (a INT, b INT, PRIMARY KEY (a), KEY b (b)) ;
  INSERT INTO t1 VALUES (1,NULL), (9,NULL) ;
  CREATE TABLE t2 ( a INT, b INT, c INT, d INT, PRIMARY KEY (a), UNIQUE KEY b (b,c,d), KEY b_2 (b), KEY c (c), KEY d (d) ) ;
  INSERT INTO t2 VALUES (43, 2, 11 ,30), (44, 2, 12 ,30), (45, 1, 1  ,10000), (46, 1, 2  ,10000), (556,1, 32 ,10000) ;
  CREATE TABLE t3 ( a INT, b INT, c INT, PRIMARY KEY (a), UNIQUE KEY b (b,c), KEY c (c), KEY b_2 (b) ) ;
  INSERT INTO t3 VALUES (1,1,1), (2,32,1) ;
  let $point=   before_reset_query_plan;let $query=  SELECT t1.a, (SELECT 1 FROM t2 WHERE t2.b=t3.c AND t2.c=t1.a ORDER BY t2.d LIMIT 1) AS incorrect FROM t1, t3 WHERE t3.b=t1.a ;
  --source include/explain_for_connection.inc
  DROP TABLE t1,t2,t3 ;






  CREATE TABLE t1 ( pk int(11) NOT NULL AUTO_INCREMENT, col_int_key int(11) NOT NULL, col_varchar_key varchar(1) NOT NULL, col_varchar_nokey varchar(1) NOT NULL, PRIMARY KEY (pk), KEY col_int_key (col_int_key), KEY col_varchar_key (col_varchar_key,col_int_key) ) ;
  INSERT INTO t1 VALUES (1,0,'j','j'),(2,8,'v','v'), (3,1,'c','c'),(4,8,'m','m'),(5,9,'d','d') ;
  CREATE VIEW v1 AS SELECT * FROM t1 ;
  let $point=   after_join_optimize;let $query=  SELECT alias2.col_varchar_nokey FROM v1 AS alias1 RIGHT JOIN t1 AS alias2 ON 1 WHERE alias2.col_varchar_key IN ( SELECT sq2_alias1.col_varchar_nokey FROM v1 AS sq2_alias1 LEFT JOIN t1 AS sq2_alias2 ON (sq2_alias2.col_int_key = sq2_alias1.pk) WHERE sq2_alias1.pk != alias1.col_int_key AND sq2_alias1.col_varchar_key > alias1.col_varchar_key ) ;
  --source include/explain_for_connection.inc
  DROP VIEW if exists v2;
  CREATE VIEW v2 AS SELECT alias2.col_varchar_nokey FROM v1 AS alias1 RIGHT JOIN t1 AS alias2 ON 1 WHERE alias2.col_varchar_key IN ( SELECT sq2_alias1.col_varchar_nokey FROM v1 AS sq2_alias1 LEFT JOIN t1 AS sq2_alias2 ON (sq2_alias2.col_int_key = sq2_alias1.pk) WHERE sq2_alias1.pk != alias1.col_int_key AND sq2_alias1.col_varchar_key > alias1.col_varchar_key ) ;
  let $point=   after_join_optimize;let $query=  SELECT alias2.col_varchar_nokey FROM t1 AS alias2 LEFT JOIN v1 AS alias1 ON 1 WHERE alias2.col_varchar_key IN ( SELECT sq2_alias1.col_varchar_nokey FROM v1 AS sq2_alias1 LEFT JOIN t1 AS sq2_alias2 ON (sq2_alias2.col_int_key = sq2_alias1.pk) WHERE sq2_alias1.pk != alias1.col_int_key AND sq2_alias1.col_varchar_key > alias1.col_varchar_key ) ;
  --source include/explain_for_connection.inc
  DROP TABLE t1 ;
  DROP VIEW if exists v1,v2;






  CREATE TABLE t1(a INT, b CHAR(1)) ;
  INSERT INTO t1 VALUES (NULL, 'x') ;
  CREATE TABLE t2(c INT, d CHAR(1)) ;
  INSERT INTO t2 VALUES (NULL, 'y'), (9, 'z') ;
  let $point=   before_reset_query_plan;let $query=  SELECT d FROM (SELECT * FROM t2) AS a2 RIGHT JOIN (SELECT * FROM t1) AS a1 ON c = a WHERE d LIKE '_' ORDER BY d ;
  --source include/explain_for_connection.inc
  DROP TABLE t1, t2 ;





  CREATE TABLE t1 ( t3_id int NOT NULL, t1_id int NOT NULL, PRIMARY KEY  (t1_id) ) ;
  CREATE TABLE t2 ( t2_id int NOT NULL, t1_id int NOT NULL, b int NOT NULL, PRIMARY KEY  (t2_id), UNIQUE KEY idx_t2_t1_b (t1_id,b) ) ENGINE=InnoDB ;
  CREATE TABLE t3 ( t3_id int NOT NULL ) ;
  INSERT INTO t3 VALUES (3) ;
  let $point=   before_reset_query_plan;let $query=  SELECT (SELECT rs.t2_id FROM t2 rs WHERE rs.t1_id= (SELECT lt.t1_id FROM t1 lt WHERE lt.t3_id=a.t3_id) ORDER BY b DESC LIMIT 1) from t3 AS a ;
  --source include/explain_for_connection.inc
  DROP PROCEDURE IF EXISTS p1 ;
  DROP TABLE t1,t2,t3 ;




  
  create table t1 (id int, a int, key idx(a)) ;
  create table t2 (id int unsigned not null auto_increment primary key, a int) ;
  insert into t2(a) values(1),(2),(3),(4),(5),(6),(7),(8) ;
  insert into t2(a) select a from t2 ;
  insert into t2(a) select a from t2 ;
  insert into t2(a) select a from t2 ;
  update t2 set a=id ;
  insert into t1 select * from t2 ;
  let $point=   before_reset_query_plan;let $query=  UPDATE t1 join t2 on (t1.a=t2.a) set t1.id=t2.id ;
  --source include/explain_for_connection.inc
  drop table t1,t2 ;
 
let $point= before_reset_query_plan;

###############################
#MTR FOR BUG#14581870         #
###############################

CREATE TABLE a1 (
  col_int int(11) DEFAULT NULL,
  pk int(11) NOT NULL AUTO_INCREMENT,
  PRIMARY KEY (pk)
) ENGINE=MyISAM DEFAULT CHARSET=latin1;

 CREATE TABLE aa1 (
  col_int int(11) DEFAULT NULL,
  pk int(11) NOT NULL AUTO_INCREMENT,
  PRIMARY KEY (pk)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

let $query=SELECT DISTINCT   alias2.col_int AS field1 , alias2.col_int AS field2 FROM  a1 AS alias1 LEFT  JOIN aa1 AS alias2 ON  alias1.pk =  alias2.pk WHERE ( alias1.col_int = 8 OR alias1 .pk <= alias1.col_int ) GROUP BY field1, field2  ORDER BY field1, field2 DESC;
--source include/explain_for_connection.inc

DROP TABLE a1;
DROP TABLE aa1;

###############################
#MTR FOR BUG#14582154         #
###############################

CREATE TABLE f1 (
  col_varchar_1024_latin1_key varchar(1024) DEFAULT NULL,
  pk int(11) NOT NULL AUTO_INCREMENT,
   PRIMARY KEY (pk),
  KEY col_varchar_1024_latin1_key (col_varchar_1024_latin1_key(1000))
  ) ENGINE=MyISAM AUTO_INCREMENT=8 DEFAULT CHARSET=latin1;

insert ignore into f1 values
(1,"I'll"),
(2,'v'),
(3,'time'),
(4,"I'm"),
(5,'o'),
(6,'rkmqmknbtoervqlzsvasurqdhucjxdoygxqxnvgqmwcidtumxwcftedxqyciakhxptzfpjwrge
ozxnbycjzxiecurgmhbubpijrllquntppirzdphpduhwqhnsmujjjbldnkiwsrsxndolpujdnozrhh
cxsxwujulpjdzvkpaijursprnwgrpquarwkazzjeiwvdmdivjqsxmhjwagewclcfykywlcnemiuaab
rrifnhuufzasunkrcpvasdqkxbwptigbpnesqigwegcnfeuvrgnecpthmhffqbythjwpukqubzpomn
trddrwhzjtqvbjiklcekxqyoxsolbxthdcprswpjxixmvfwmsyseblwcvumvyvbitxqjxdzdytunqv
vrmpyxrencqhuyrfluezqekmqpwutxnzddrbjycyoyqbzsoxvillooqxuvoxcgohdbytybwcqxdwtq
rebgjzbycekyjgbpmqadutrqluyxhrdodxqqjwasfkvetfobocgpftrhvxugmmszwpoglvarsfiljr
zimqeplevleqdhepkhcfrahcpjjvhkkwblfenfpetwejamgeefplsksusqvazujabvkfek'),
(7,'y');

ANALYZE TABLE f1;

CREATE TABLE a2 (
  col_varchar_10_utf8_key varchar(10) CHARACTER SET utf8 DEFAULT NULL,
  KEY col_varchar_10_utf8_key (col_varchar_10_utf8_key)
) ENGINE=MyISAM DEFAULT CHARSET=latin1;

let $query=SELECT DISTINCT   alias1.pk AS field1 FROM  f1 AS alias1 LEFT  JOIN a2 AS alias2 ON  alias1.col_varchar_1024_latin1_key =  alias2 .col_varchar_10_utf8_key WHERE alias1.pk  IN (5, 5) GROUP BY field1 HAVING field1 <= 3;
--source include/explain_for_connection.inc

DROP TABLE f1;
DROP TABLE a2;

###############################
#MTR FOR BUG#14586538         #
###############################

CREATE TABLE h2 (
pk int(11) NOT NULL AUTO_INCREMENT,
PRIMARY KEY (pk)
) ENGINE=MyISAM AUTO_INCREMENT=10 DEFAULT CHARSET=latin1;

insert into h2 values (1),(2),(3),(4),(5),(6),(7),(8),(9);

ANALYZE TABLE h2;

CREATE TABLE aa3 (
  col_int_key int(11) DEFAULT NULL,
  KEY col_int_key (col_int_key)
 ) ENGINE=InnoDB DEFAULT CHARSET=latin1;

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
) ENGINE=MyISAM AUTO_INCREMENT=7 DEFAULT CHARSET=latin1;

insert into e1 values (1, NULL,NULL);
insert into e1 values (2,6,NULL);
insert into e1 values (3,8,-1131610112);
insert into e1 values (4,2,-1009057792);
insert into e1 values (5,-1220345856,1);
insert into e1 values (6,NULL,-185204736);

ANALYZE TABLE e1;

CREATE TABLE cc2 (
  col_int int(11) DEFAULT NULL
 ) ENGINE=InnoDB AUTO_INCREMENT=3 DEFAULT CHARSET=latin1;

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
) ENGINE=InnoDB;

CREATE TABLE t2 (
  pk INT NOT NULL,
  col_varchar_key VARCHAR(10),
  PRIMARY KEY (pk),
  KEY col_varchar_key (col_varchar_key)
) ENGINE=InnoDB;

let $query=SELECT DISTINCT alias2.pk AS field1, alias2.pk AS field2, alias2.pk AS field3 FROM t1 AS alias1 RIGHT JOIN t2 AS alias2  ON  alias1.col_varchar_key =  alias2.col_varchar_key WHERE alias1.col_int >alias1.col_int HAVING field3 = 7 ORDER BY field1, field2, field3 LIMIT 10 OFFSET 1;

--source include/explain_for_connection.inc

--enable_warnings
drop table t1;
drop table t2;

