--disable_warnings
DROP TABLE IF EXISTS t_10;
DROP TABLE IF EXISTS t_100;
DROP TABLE IF EXISTS t_1000;
DROP TABLE IF EXISTS tp;
DROP TABLE IF EXISTS tsp;
DROP TABLE IF EXISTS t_empty;
DROP TABLE IF EXISTS t_null;
--enable_warnings

eval CREATE TABLE t_10 (a INT,
  b VARCHAR(55),
  PRIMARY KEY (a)) $data_directory $index_directory
ENGINE = $engine_table;

eval CREATE TABLE t_100 (a INT,
  b VARCHAR(55),
  PRIMARY KEY (a)) $data_directory $index_directory
ENGINE = $engine_table;

eval CREATE TABLE t_1000 (a INT,
  b VARCHAR(55),
  PRIMARY KEY (a)) $data_directory $index_directory
ENGINE = $engine_table;

eval CREATE TABLE t_empty (a INT,
  b VARCHAR(55),
  PRIMARY KEY (a)) $data_directory $index_directory
ENGINE = $engine_table;

eval CREATE TABLE t_null (a INT,
  b VARCHAR(55),
  PRIMARY KEY (a)) $data_directory $index_directory
ENGINE = $engine_table;

eval CREATE TABLE tp (a INT,
  b VARCHAR(55),
  PRIMARY KEY (a)) $data_directory $index_directory
ENGINE = $engine_part
PARTITION BY RANGE (a)
(PARTITION p0 VALUES LESS THAN (10) $p_data_directory $p_index_directory,
 PARTITION p1 VALUES LESS THAN (100) $p_data_directory $p_index_directory,
 PARTITION p2 VALUES LESS THAN (1000) $p_data_directory $p_index_directory);

eval CREATE TABLE tsp (a INT,
  b VARCHAR(55),
  PRIMARY KEY (a)) $data_directory $index_directory
ENGINE = $engine_subpart
PARTITION BY RANGE (a)
SUBPARTITION BY HASH(a)
(PARTITION p0 VALUES LESS THAN (10) $p_data_directory $p_index_directory
 (SUBPARTITION sp00,
  SUBPARTITION sp01,
  SUBPARTITION sp02,
  SUBPARTITION sp03,
  SUBPARTITION sp04),
 PARTITION p1 VALUES LESS THAN (100)
 (SUBPARTITION sp10 $p_data_directory $p_index_directory,
  SUBPARTITION sp11 $p_data_directory $p_index_directory,
  SUBPARTITION sp12 $p_data_directory $p_index_directory,
  SUBPARTITION sp13 $p_data_directory $p_index_directory,
  SUBPARTITION sp14 $p_data_directory $p_index_directory),
 PARTITION p2 VALUES LESS THAN (1000) $p_data_directory $p_index_directory
 (SUBPARTITION sp20,
  SUBPARTITION sp21,
  SUBPARTITION sp22,
  SUBPARTITION sp23,
  SUBPARTITION sp24));

# Values t_10 (not partitioned)
INSERT INTO t_10 VALUES (1, "One"), (3, "Three"), (5, "Five"), (9, "Nine");

# Values t_100 (not partitioned)
INSERT INTO t_100 VALUES (11, "Eleven"), (13, "Thirdteen"), (15, "Fifeteen"), (19, "Nineteen");
INSERT INTO t_100 VALUES (91, "Ninety-one"), (93, "Ninety-three"), (95, "Ninety-five"), (99, "Ninety-nine");

# Values t_1000 (not partitioned)
INSERT INTO t_1000 VALUES (111, "Hundred elven"), (113, "Hundred thirdteen"), (115, "Hundred fiveteen"), (119, "Hundred nineteen");
INSERT INTO t_1000 VALUES (131, "Hundred thirty-one"), (133, "Hundred thirty-three"), (135, "Hundred thirty-five"), (139, "Hundred thirty-nine");
INSERT INTO t_1000 VALUES (151, "Hundred fifty-one"), (153, "Hundred fifty-three"), (155, "Hundred fity-five"), (159, "Hundred fifty-nine");
INSERT INTO t_1000 VALUES (191, "Hundred ninety-one"), (193, "Hundred ninety-three"), (195, "Hundred ninety-five"), (199, "Hundred ninety-nine");

# Values t_null (not partitioned)
INSERT INTO t_null VALUES (1, "NULL");

# Values tp (partitions)
INSERT INTO tp VALUES (2, "Two"), (4, "Four"), (6, "Six"), (8, "Eight");
INSERT INTO tp VALUES (12, "twelve"), (14, "Fourteen"), (16, "Sixteen"), (18, "Eightteen");
INSERT INTO tp VALUES (112, "Hundred twelve"), (114, "Hundred fourteen"), (116, "Hundred sixteen"), (118, "Hundred eightteen");
INSERT INTO tp VALUES (122, "Hundred twenty-two"), (124, "Hundred twenty-four"), (126, "Hundred twenty-six"), (128, "Hundred twenty-eight");
INSERT INTO tp VALUES (162, "Hundred sixty-two"), (164, "Hundred sixty-four"), (166, "Hundred sixty-six"), (168, "Hundred sixty-eight");
INSERT INTO tp VALUES (182, "Hundred eighty-two"), (184, "Hundred eighty-four"), (186, "Hundred eighty-six"), (188, "Hundred eighty-eight");

# Values tps (subpartitions)
INSERT INTO tsp VALUES (2, "Two"), (4, "Four"), (6, "Six"), (8, "Eight");
INSERT INTO tsp VALUES (12, "twelve"), (14, "Fourteen"), (16, "Sixteen"), (18, "Eightteen");
INSERT INTO tsp VALUES (112, "Hundred twelve"), (114, "Hundred fourteen"), (116, "Hundred sixteen"), (118, "Hundred eightteen");
INSERT INTO tsp VALUES (122, "Hundred twenty-two"), (124, "Hundred twenty-four"), (126, "Hundred twenty-six"), (128, "Hundred twenty-eight");
INSERT INTO tsp VALUES (162, "Hundred sixty-two"), (164, "Hundred sixty-four"), (166, "Hundred sixty-six"), (168, "Hundred sixty-eight");
INSERT INTO tsp VALUES (182, "Hundred eight-two"), (184, "Hundred eighty-four"), (186, "Hundred eighty-six"), (188, "Hundred eighty-eight");

eval CREATE TABLE tsp_01(a INT,b VARCHAR(55),PRIMARY KEY (a))
             ENGINE = $engine_table $data_directory $index_directory
             AS SELECT a, b FROM t_10 WHERE MOD(a,5)= 1;
eval CREATE TABLE tsp_02(a INT,b VARCHAR(55),PRIMARY KEY (a)) 
             ENGINE = $engine_table $data_directory $index_directory
             AS SELECT a, b FROM t_10 WHERE MOD(a,5)= 2;
eval CREATE TABLE tsp_03(a INT,b VARCHAR(55),PRIMARY KEY (a)) 
             ENGINE = $engine_table $data_directory $index_directory
             AS SELECT a, b FROM t_10 WHERE MOD(a,5)= 3;
eval CREATE TABLE tsp_04(a INT,b VARCHAR(55),PRIMARY KEY (a)) 
             ENGINE = $engine_table $data_directory $index_directory
             AS SELECT a, b FROM t_10 WHERE MOD(a,5)= 4;
eval CREATE TABLE tsp_00(a INT,b VARCHAR(55),PRIMARY KEY (a)) 
             ENGINE = $engine_table $data_directory $index_directory
             AS SELECT a, b FROM t_10 WHERE MOD(a,5)= 0;
 
SHOW CREATE TABLE t_10;
SHOW CREATE TABLE t_100;
SHOW CREATE TABLE t_1000;
SHOW CREATE TABLE tp;
SHOW CREATE TABLE tsp;

--sorted_result
SELECT * FROM t_10;
--sorted_result
SELECT * FROM t_100;
--sorted_result
SELECT * FROM t_1000;
--sorted_result
SELECT * FROM tp;
--sorted_result
SELECT * FROM tp WHERE a< 10;
--sorted_result
SELECT * FROM tp WHERE a BETWEEN 11 AND 100;
--sorted_result
SELECT * FROM tp WHERE a BETWEEN 101 AND 200;
--sorted_result
SELECT * FROM tsp;
