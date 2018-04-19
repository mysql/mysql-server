--source include/have_optimizer_trace.inc
--source include/have_64bit.inc

# Test requires: sp-protocol/ps-protocol/view-protocol/cursor-protocol disabled
--source include/no_protocol.inc

SET optimizer_trace_max_mem_size=1048576; # 1MB
SET end_markers_in_json=off;
SET optimizer_trace="enabled=on,one_line=off";

--echo #
--echo # WL#1393 - Optimizing filesort with small limit
--echo # WL#5834 - Add optimizer traces for sorting
--echo #


CREATE TABLE t1(f0 int auto_increment primary key, f1 int, f2 varchar(200))
charset latin1;
INSERT INTO t1(f1, f2) VALUES 
(0,"0"),(1,"1"),(2,"2"),(3,"3"),(4,"4"),(5,"5"),
(6,"6"),(7,"7"),(8,"8"),(9,"9"),(10,"10"),
(11,"11"),(12,"12"),(13,"13"),(14,"14"),(15,"15"),
(16,"16"),(17,"17"),(18,"18"),(19,"19"),(20,"20"),
(21,"21"),(22,"22"),(23,"23"),(24,"24"),(25,"25"),
(26,"26"),(27,"27"),(28,"28"),(29,"29"),(30,"30"),
(31,"31"),(32,"32"),(33,"33"),(34,"34"),(35,"35"),
(36,"36"),(37,"37"),(38,"38"),(39,"39"),(40,"40"),
(41,"41"),(42,"42"),(43,"43"),(44,"44"),(45,"45"),
(46,"46"),(47,"47"),(48,"48"),(49,"49"),(50,"50"),
(51,"51"),(52,"52"),(53,"53"),(54,"54"),(55,"55"),
(56,"56"),(57,"57"),(58,"58"),(59,"59"),(60,"60"),
(61,"61"),(62,"62"),(63,"63"),(64,"64"),(65,"65"),
(66,"66"),(67,"67"),(68,"68"),(69,"69"),(70,"70"),
(71,"71"),(72,"72"),(73,"73"),(74,"74"),(75,"75"),
(76,"76"),(77,"77"),(78,"78"),(79,"79"),(80,"80"),
(81,"81"),(82,"82"),(83,"83"),(84,"84"),(85,"85"),
(86,"86"),(87,"87"),(88,"88"),(89,"89"),(90,"90"),
(91,"91"),(92,"92"),(93,"93"),(94,"94"),(95,"95"),
(96,"96"),(97,"97"),(98,"98"),(99,"99");

################
## Test sort when source data fits in memory

SELECT * FROM t1 ORDER BY f1 ASC, f0 LIMIT 100;
SELECT * FROM information_schema.OPTIMIZER_TRACE;
SELECT * FROM t1 ORDER BY f1 ASC, f0 LIMIT 30;
SELECT * FROM information_schema.OPTIMIZER_TRACE;
SELECT * FROM t1 ORDER BY f1 ASC, f0 LIMIT 0;
SELECT * FROM information_schema.OPTIMIZER_TRACE;
SELECT * FROM t1 ORDER BY f2 DESC, f0 LIMIT 30;
SELECT * FROM information_schema.OPTIMIZER_TRACE;
SELECT * FROM t1 ORDER BY f2 DESC, f0 LIMIT 0;
SELECT * FROM information_schema.OPTIMIZER_TRACE;
SELECT * FROM t1 WHERE f1>10 ORDER BY f2, f0 LIMIT 20;
SELECT * FROM information_schema.OPTIMIZER_TRACE;
SELECT * FROM t1 WHERE f1>10 ORDER BY f2, f0 LIMIT 0;
SELECT * FROM information_schema.OPTIMIZER_TRACE;
SELECT * FROM t1 WHERE f1>10 ORDER BY f2, f0 LIMIT 10 OFFSET 10;
SELECT * FROM information_schema.OPTIMIZER_TRACE;
SELECT * FROM t1 WHERE f1>10 ORDER BY f2, f0 LIMIT 0 OFFSET 10;
SELECT * FROM information_schema.OPTIMIZER_TRACE;

SELECT CONCAT("hello ", f2) AS foo FROM t1 ORDER BY foo LIMIT 2;
SELECT * FROM information_schema.OPTIMIZER_TRACE;

--disable_result_log
SELECT * from t1 ORDER BY rand(2) LIMIT 2;
--enable_result_log
SELECT * FROM information_schema.OPTIMIZER_TRACE;

################
## Test sort when source data does not fit in memory
set sort_buffer_size= 32768;

SELECT * FROM t1 ORDER BY f1 ASC, f0 LIMIT 30;
SELECT * FROM information_schema.OPTIMIZER_TRACE;
SELECT * FROM t1 ORDER BY f1 ASC, f0 LIMIT 0;
SELECT * FROM information_schema.OPTIMIZER_TRACE;
SELECT * FROM t1 ORDER BY f2 DESC, f0 LIMIT 30;
SELECT * FROM information_schema.OPTIMIZER_TRACE;
SELECT * FROM t1 ORDER BY f2 DESC, f0 LIMIT 0;
SELECT * FROM information_schema.OPTIMIZER_TRACE;
SELECT * FROM t1 WHERE f1>10 ORDER BY f2, f0 LIMIT 20;
SELECT * FROM information_schema.OPTIMIZER_TRACE;
SELECT * FROM t1 WHERE f1>10 ORDER BY f2, f0 LIMIT 0;
SELECT * FROM information_schema.OPTIMIZER_TRACE;
SELECT * FROM t1 WHERE f1>10 ORDER BY f2, f0 LIMIT 10 OFFSET 10;
SELECT * FROM information_schema.OPTIMIZER_TRACE;
SELECT * FROM t1 WHERE f1>10 ORDER BY f2, f0 LIMIT 0 OFFSET 10;
SELECT * FROM information_schema.OPTIMIZER_TRACE;

################
## Test with SQL_CALC_FOUND_ROWS
set sort_buffer_size= 32768;
set optimizer_trace_limit=1;
set optimizer_trace_offset=-2;

SELECT SQL_CALC_FOUND_ROWS * FROM t1
ORDER BY f1, f0 LIMIT 30;
SELECT FOUND_ROWS();
SELECT * FROM information_schema.OPTIMIZER_TRACE;

SELECT SQL_CALC_FOUND_ROWS * FROM t1
ORDER BY f1, f0 LIMIT 0;
SELECT FOUND_ROWS();
SELECT * FROM information_schema.OPTIMIZER_TRACE;

SELECT SQL_CALC_FOUND_ROWS * FROM t1 WHERE f1>10
ORDER BY f2, f0 LIMIT 20;
SELECT FOUND_ROWS();
SELECT * FROM information_schema.OPTIMIZER_TRACE;

SELECT SQL_CALC_FOUND_ROWS * FROM t1 WHERE f1>10
ORDER BY f2, f0 LIMIT 0;
SELECT FOUND_ROWS();
SELECT * FROM information_schema.OPTIMIZER_TRACE;

SELECT SQL_CALC_FOUND_ROWS * FROM t1 WHERE f1>10
ORDER BY f2, f0 LIMIT 10 OFFSET 10;
SELECT FOUND_ROWS();
SELECT * FROM information_schema.OPTIMIZER_TRACE;

SELECT SQL_CALC_FOUND_ROWS * FROM t1 WHERE f1>10
ORDER BY f2, f0 LIMIT 0 OFFSET 10;
SELECT FOUND_ROWS();
SELECT * FROM information_schema.OPTIMIZER_TRACE;

################
## Test sorting with join
## These are re-written to use PQ during execution.
set sort_buffer_size= 327680;
set optimizer_trace_limit=1;
set optimizer_trace_offset=-1;

CREATE TEMPORARY TABLE tmp (f1 int, f2 varchar(20)) charset latin1;
INSERT INTO tmp SELECT f1, f2 FROM t1;
INSERT INTO t1(f1, f2) SELECT * FROM tmp;
INSERT INTO tmp SELECT f1, f2 FROM t1;
INSERT INTO t1(f1, f2) SELECT * FROM tmp;

SELECT * FROM t1 JOIN tmp on t1.f2=tmp.f2
ORDER BY tmp.f1, f0 LIMIT 30;
SELECT * FROM information_schema.OPTIMIZER_TRACE;

SELECT * FROM t1 JOIN tmp on t1.f2=tmp.f2
ORDER BY tmp.f1, f0 LIMIT 30 OFFSET 30;
SELECT * FROM information_schema.OPTIMIZER_TRACE;

set optimizer_trace_limit=2;
set optimizer_trace_offset=-2;

SELECT SQL_CALC_FOUND_ROWS * FROM t1 JOIN tmp on t1.f2=tmp.f2
ORDER BY tmp.f1, f0 LIMIT 30 OFFSET 30;
SELECT FOUND_ROWS();
SELECT * FROM information_schema.OPTIMIZER_TRACE;

SELECT SQL_CALC_FOUND_ROWS * FROM t1 JOIN tmp on t1.f2=tmp.f2
WHERE t1.f2>20
ORDER BY tmp.f1, f0 LIMIT 30 OFFSET 30;
SELECT FOUND_ROWS();
SELECT * FROM information_schema.OPTIMIZER_TRACE;

DROP TABLE tmp;

################
## Test views
set optimizer_trace_limit=1;
set optimizer_trace_offset=-1;

CREATE VIEW v1 as SELECT * FROM t1 ORDER BY f1, f0 LIMIT 30;
SELECT * FROM v1;
SELECT * FROM information_schema.OPTIMIZER_TRACE;
drop view v1;

CREATE VIEW v1 as SELECT * FROM t1 ORDER BY f1, f0 LIMIT 100;
SELECT * FROM v1 ORDER BY f2, f0 LIMIT 30;
SELECT * FROM information_schema.OPTIMIZER_TRACE;

CREATE VIEW v2 as SELECT * FROM t1 ORDER BY f2, f0 LIMIT 100;
SELECT * FROM v1 JOIN v2 on v1.f1=v2.f1 ORDER BY v1.f2,v1.f0,v2.f0
LIMIT 30;
SELECT * FROM information_schema.OPTIMIZER_TRACE;

################
## Test group & having
SELECT floor(f1/10) f3, count(f2) FROM t1
GROUP BY 1 ORDER BY 2,1 LIMIT 5;
SELECT * FROM information_schema.OPTIMIZER_TRACE;

SELECT floor(f1/10) f3, count(f2) FROM t1
GROUP BY 1 ORDER BY 2,1 LIMIT 0;
SELECT * FROM information_schema.OPTIMIZER_TRACE;

################
## Test SP
delimiter |;
CREATE PROCEDURE wl1393_sp_test()
BEGIN
SELECT * FROM t1 WHERE f1>10 ORDER BY f2, f0 LIMIT 30;
SELECT * FROM information_schema.OPTIMIZER_TRACE;
SELECT * FROM t1 WHERE f1>10 ORDER BY f2, f0 LIMIT 15 OFFSET 15;
SELECT * FROM information_schema.OPTIMIZER_TRACE;
SELECT SQL_CALC_FOUND_ROWS * FROM t1 WHERE f1>10
ORDER BY f2, f0 LIMIT 15 OFFSET 15;
SELECT * FROM information_schema.OPTIMIZER_TRACE;
SELECT FOUND_ROWS();
SELECT * FROM v1 ORDER BY f2, f0 LIMIT 30;
SELECT * FROM information_schema.OPTIMIZER_TRACE;
END|
CALL wl1393_sp_test()|
DROP PROCEDURE wl1393_sp_test|
delimiter ;|

################
## Test with subqueries
SELECT d1.f1, d1.f2 FROM t1
LEFT JOIN (SELECT * FROM t1 ORDER BY f1 LIMIT 30) d1 on t1.f1=d1.f1
ORDER BY d1.f2 DESC LIMIT 30;
SELECT * FROM information_schema.OPTIMIZER_TRACE;

SELECT * FROM t1 WHERE f1 = (SELECT f1 FROM t1 ORDER BY 1 LIMIT 1);
SELECT * FROM information_schema.OPTIMIZER_TRACE;

--error ER_SUBQUERY_NO_1_ROW
SELECT * FROM t1 WHERE f1 = (SELECT f1 FROM t1 ORDER BY 1 LIMIT 2);
SELECT * FROM information_schema.OPTIMIZER_TRACE;

################
## Test stripping additional fields
CREATE TEMPORARY TABLE tmp (f1 int, f2 varchar(20) charset latin1);
INSERT INTO tmp SELECT f1,f2 FROM t1;
INSERT INTO t1(f1,f2) SELECT * FROM tmp;
INSERT INTO tmp SELECT f1,f2 FROM t1;
INSERT INTO t1(f1,f2) SELECT * FROM tmp; 
INSERT INTO t1(f1,f2) SELECT * FROM tmp;
INSERT INTO tmp SELECT f1,f2 FROM t1;
INSERT INTO t1(f1,f2) SELECT * FROM tmp; 
INSERT INTO tmp SELECT f1,f2 FROM t1;
INSERT INTO t1(f1,f2) SELECT * FROM tmp; 
INSERT INTO tmp SELECT f1,f2 FROM t1;
INSERT INTO t1(f1,f2) SELECT * FROM tmp; 
INSERT INTO tmp SELECT f1,f2 FROM t1;
INSERT INTO t1(f1,f2) SELECT * FROM tmp; 
INSERT INTO tmp SELECT f1,f2 FROM t1;
INSERT INTO t1(f1,f2) SELECT * FROM tmp; 

# Test when only sortkeys fits to memory
set sort_buffer_size= 32768; 
SELECT * FROM t1 ORDER BY f2,f0 LIMIT 100;
SELECT * FROM information_schema.OPTIMIZER_TRACE;

DROP TABLE t1, tmp;
DROP VIEW v1, v2;

--echo # end of WL#1393 - Optimizing filesort with small limit
--echo # end of WL#5834 - Add optimizer traces for sorting

--echo # WL#8741 Varlen keys for sorting JSON values

CREATE TABLE t(id INT PRIMARY KEY AUTO_INCREMENT, j JSON);

INSERT INTO t(j) VALUES
(NULL), ('true'), ('false'),
('"abc"'), ('""'), ('"abcd"'), ('"bc"'),
('"abc\\u0000\\u0000"'),
('"abc\\u0000"'),
('0.0'), ('-0.0'), ('9223372036854776000'),
('1.0e-1'), ('1.0e-2')
;

SELECT j, JSON_TYPE(j) AS tp FROM t ORDER BY j, id;
SELECT j, JSON_TYPE(j) AS tp FROM t ORDER BY j, id limit 2 offset 2;
SELECT * FROM information_schema.OPTIMIZER_TRACE;

SELECT j, JSON_TYPE(j) AS tp FROM t ORDER BY j DESC, id;
SELECT j, JSON_TYPE(j) AS tp FROM t ORDER BY j DESC, id limit 2 offset 2;
SELECT * FROM information_schema.OPTIMIZER_TRACE;

SELECT JSON_EXTRACT(j, '$') AS je, JSON_TYPE(j) AS tp FROM t ORDER BY je, id;
SELECT JSON_EXTRACT(j, '$') AS je, JSON_TYPE(j) AS tp FROM t ORDER BY je, id
limit 2 offset 2;
SELECT * FROM information_schema.OPTIMIZER_TRACE;

DROP TABLE t;

--echo # --end of WL#8741 Varlen keys for sorting JSON values
