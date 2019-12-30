--echo # 
--echo # Test that on a tiny table, the computed filtering effect is not
--echo # less than one row for the basic condition filter constants
--echo #

CREATE TABLE t1(
  col1_pk INTEGER PRIMARY KEY,
  col2 INTEGER
);

INSERT INTO t1 VALUES (0,0),(1,1),(2,2),(3,3),(4,4),(5,5),(6,6),(7,7);

--disable_query_log
--disable_result_log
ANALYZE TABLE t1;
--enable_result_log
--enable_query_log

--echo # Filtered column should be 1 / (size of table) and larger than 
--echo # COND_FILTER_EQUALITY
EXPLAIN SELECT * FROM t1 WHERE col2 = 2;

--echo # Filtered column should be (size of table - 1) / (size of table) 
--echo # and less than COND_FILTER_EQUALITY
EXPLAIN SELECT * FROM t1 WHERE col2 <> 2;

--echo # Filtered column should be 1 / (size of table) and larger than
--echo # COND_FILTER_BETWEEN
EXPLAIN SELECT * FROM t1 WHERE col2 BETWEEN 2 AND 4;

--echo # Filtered column should be (size of table - 1) / (size of table)
--echo # and less than COND_FILTER_BETWEEN
EXPLAIN SELECT * FROM t1 WHERE col2 NOT BETWEEN 2 AND 4;

# Make the table even smaller to be able to test COND_FILTER_INEQUALITY
TRUNCATE TABLE t1;
INSERT INTO t1 VALUES (0,0),(1,1);

--disable_query_log
--disable_result_log
ANALYZE TABLE t1;
--enable_result_log
--enable_query_log

--echo # Filtered column should be 1 / (size of table) and larger than
--echo # COND_FILTER_INEQUALITY
EXPLAIN SELECT * FROM t1 WHERE col2 > 1;

DROP TABLE t1;

--echo #
--echo # Bug#18438671: MAKE USE OF DATATYPE CONSTRAINTS WHEN CALCULATING
--echo #               FILTERING EFFECT
--echo #

CREATE TABLE t1(
  day_of_week enum('0','1','2','3','4','5','6'),
  bit1 bit(1),
  bit3 bit(3)
);

INSERT INTO t1 VALUES (1+RAND()*7, RAND()*2, RAND()*8),
                      (1+RAND()*7, RAND()*2, RAND()*8);

let $iteration= 0;
let $i= 2;
while ($iteration < 6)
{
  eval INSERT INTO t1 SELECT 1+RAND()*7, RAND()*2, RAND()*8 FROM t1;
  let $i=$i*2;  
  inc $iteration;
}

EXPLAIN SELECT * FROM t1 WHERE bit1 =  b'1';
EXPLAIN SELECT * FROM t1 WHERE bit1 <=>b'1';
EXPLAIN SELECT * FROM t1 WHERE bit1 >  b'0';
EXPLAIN SELECT * FROM t1 WHERE bit1 >= b'0';
EXPLAIN SELECT * FROM t1 WHERE bit1 <  b'0';
EXPLAIN SELECT * FROM t1 WHERE bit1 <= b'0';
EXPLAIN SELECT * FROM t1 WHERE bit3 =  b'1';
EXPLAIN SELECT * FROM t1 WHERE day_of_week;
EXPLAIN SELECT * FROM t1 WHERE day_of_week =  1;
EXPLAIN SELECT * FROM t1 WHERE day_of_week IN (1);
EXPLAIN SELECT * FROM t1 WHERE day_of_week IN (1,2);
EXPLAIN SELECT * FROM t1 WHERE day_of_week LIKE 'foo';
EXPLAIN SELECT * FROM t1 WHERE NOT day_of_week = 1;

DROP TABLE t1;
