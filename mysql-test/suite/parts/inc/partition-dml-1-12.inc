# NOTE: Currently run for InnoDB Storage Engine only

--echo # test handler operation with partitioned table
eval CREATE TABLE t1 (
  col_1_int int,
  col_2_int int,
  PRIMARY KEY(col_2_int),
  KEY idx1 (col_1_int, col_2_int))
  ENGINE = $engine
  PARTITION BY KEY() partitions 4;

SHOW CREATE TABLE t1;

--echo # test handler read forward

--echo # insert values in all partitions - 1st set
INSERT INTO t1 VALUES (1, 1), (2, 2), (3, 3), (4, 4);

--echo # insert values in all partitions - 2nd set
INSERT INTO t1 VALUES (8, 5), (7, 6), (6, 7), (5, 8);

HANDLER t1 OPEN;

--echo # read 2nd set of values in forward direction using handler
HANDLER t1 READ idx1 > (4, 4) LIMIT 10;

--echo # replace 1st set of values to make them bigger in idx1 order
REPLACE INTO t1 VALUES (9, 1), (10, 2), (11, 3), (12, 4);

--echo # try handler next- expect no rows
HANDLER t1 READ idx1 next;

HANDLER t1 CLOSE;

--echo # delete all rows
DELETE FROM t1;

--echo # test handler read backward

--echo # insert values in all partitions - 1st set
INSERT INTO t1 VALUES (8, 5), (7, 6), (6, 7), (5, 8);

--echo # insert values in all partitions - 2nd set
INSERT INTO t1 VALUES (9, 9), (10, 10), (11, 11), (12, 12);

HANDLER t1 OPEN;

--echo # read 1st set of values in backward direction using handler
HANDLER t1 READ idx1 < (9, 9) LIMIT 10;

--echo # replace 2nd set of values to make them smaller in idx1 order
REPLACE INTO t1 VALUES ( 1,  9), ( 2,  10), ( 3,  11), ( 4,  12);

--echo # try handler previous- expect no rows
HANDLER t1 READ idx1 prev;

HANDLER t1 CLOSE;

DROP TABLE t1;
#------------------------------------------------------------------------------#
