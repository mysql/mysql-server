#
# WL#1393 - ORDER BY with LIMIT tests
#
# A big test in a separate file, so that we can run the original
# order_by test with --debug without timeout.
#
# All the sort keys fit in memory, but the addon fields do not.
#
CREATE TABLE t1(
  f0 int auto_increment PRIMARY KEY,
  f1 int,
  f2 varchar(200)
) charset latin1;

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

CREATE TEMPORARY TABLE tmp (f1 int, f2 varchar(20)) charset latin1;
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

FLUSH STATUS;
SHOW SESSION STATUS LIKE 'Sort%';

SELECT * FROM t1 ORDER BY f2,f0 LIMIT 101;

# 32-bit has smaller pointers and thus can do with fewer merge passes.
--replace_result 100 102
SHOW SESSION STATUS LIKE 'Sort%';

FLUSH STATUS;
CREATE TABLE t2 (f1 int);
INSERT INTO t2 VALUES (0), (0);
SELECT * FROM t2 where f1 =
(SELECT f2 from t1 where t1.f1 = t2.f1 ORDER BY f1 LIMIT 1);

SHOW SESSION STATUS LIKE 'Sort%';

DROP TABLE t1, t2, tmp;

--echo Bug#32038406: Filesort used for ORDER BY .. DESC even when
--echo               descending index available and used

CREATE TABLE t (
col1 INTEGER NOT NULL,
col2 BINARY(16) NOT NULL,
col3 VARCHAR(191) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NOT NULL,
col4 INTEGER NOT NULL,
col5 TIMESTAMP(6) NOT NULL DEFAULT CURRENT_TIMESTAMP(6),
col6 BLOB,
PRIMARY KEY (col1),
UNIQUE KEY uc_key (col2, col3, col4)
);

INSERT INTO t VALUES(1, x'4142434445464748494a414243444546', 'WRITEBACK',
                     0, TIMESTAMP'2020-01-01 00:00:00.000000', NULL);

let $query =
SELECT t.col1, t.col2, t.col3, t.col4, t.col5, t.col6
FROM t
WHERE t.col2 IN (x'4142434445464748494a414243444546') AND
      t.col3 IN ('WRITEBACK')
ORDER BY t.col2 DESC, t.col3 DESC, t.col4 DESC
LIMIT 1;

eval $query;
eval explain $query;

DROP TABLE t;

--echo Here, latin1 is used because it matches collation of temporal const

CREATE TABLE t1(vc VARCHAR(20) CHARACTER SET latin1);
INSERT INTO t1 VALUES('2021-02-08'), ('21-02-08');

--echo Verify that a string const value removes ORDER BY clause

let $query = SELECT * FROM t1 WHERE vc = '2021-02-08' ORDER BY vc ASC;
eval $query;
eval explain $query;

let $query = SELECT * FROM t1 WHERE vc = '2021-02-08' ORDER BY vc DESC;
eval $query;
eval explain $query;

--echo Verify the same with a variable (which is const for execution)

set @strvar = _latin1'2021-02-08';
let $query = SELECT * FROM t1 WHERE vc = @strvar ORDER BY vc ASC;
eval $query;
eval explain $query;

let $query = SELECT * FROM t1 WHERE vc = @strvar ORDER BY vc DESC;
eval $query;
eval explain $query;

--echo Verify that a date const value does not remove ORDER BY clause

let $query = SELECT * FROM t1 WHERE vc = DATE'2021-02-08' ORDER BY vc ASC;
eval $query;
eval explain $query;

let $query = SELECT * FROM t1 WHERE vc = DATE'2021-02-08' ORDER BY vc DESC;
eval $query;
eval explain $query;

DROP TABLE t1;
