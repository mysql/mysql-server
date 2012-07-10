# Test for optimizer tracing of range analysis

--source include/have_optimizer_trace.inc
# InnoDB page size influences cost => makes trace vary.
--source include/have_innodb_16k.inc

SET optimizer_trace_max_mem_size=1048576; # 1MB
SET end_markers_in_json=on;
SET optimizer_trace="enabled=on,one_line=off";

CREATE TABLE t1
(
  key1 INT NOT NULL,
  INDEX i1(key1)
);

--echo Inserting 1024 records into t1

--disable_query_log
INSERT INTO t1 VALUES (1),(2),(3),(4),(5),(6),(7),(8);

let $1=7;
set @d=8;
while ($1)
{
  EVAL INSERT INTO t1 SELECT key1+@d FROM t1;
  EVAL SET @d=@d*2;
  DEC $1;
}
--enable_query_log

ALTER TABLE t1 ADD key2 INT NOT NULL, ADD INDEX i2(key2);
ALTER TABLE t1 ADD key3 INT NOT NULL, ADD INDEX i3(key3);
ALTER TABLE t1 ADD key4 INT NOT NULL, ADD INDEX i4(key4);
ALTER TABLE t1 ADD key5 INT NOT NULL, ADD INDEX i5(key5);
ALTER TABLE t1 ADD key6 INT NOT NULL, ADD INDEX i6(key6);
ALTER TABLE t1 ADD key7 INT NOT NULL, ADD INDEX i7(key7);
ALTER TABLE t1 ADD key8 INT NOT NULL, ADD INDEX i8(key8);

UPDATE t1 SET 
  key2=key1,
  key3=key1,
  key4=key1,
  key5=key1,
  key6=key1,
  key7=key1,
  key8=1024-key1;

CREATE TABLE t2 (
  key1a INT NOT NULL,
  key1b INT NOT NULL,
  key2  INT NOT NULL,
  key2_1 INT NOT NULL,
  key2_2 INT NOT NULL,
  key3  INT NOT NULL,
  primary key i1a (key1a, key1b),
  INDEX i1b (key1b, key1a),
  INDEX i2_1(key2, key2_1),
  INDEX i2_2(key2, key2_1)
);

INSERT INTO t2 SELECT key1,key1,key1 div 10, key1 % 10, key1 % 10, key1 FROM t1;

# multiple ranges on one key
--echo
EXPLAIN SELECT * FROM t1 WHERE key2 < 5 OR key2 > 1020;
--echo
SELECT * FROM information_schema.OPTIMIZER_TRACE;

# multiple ranges on one key, turn off range_optimizer tracing
set @@optimizer_trace_features="range_optimizer=off";
--echo
EXPLAIN SELECT * FROM t1 WHERE key2 < 5 OR key2 > 1020;
--echo
SELECT * FROM information_schema.OPTIMIZER_TRACE;
set @@optimizer_trace_features="range_optimizer=on";


# index merge
--echo
EXPLAIN SELECT * FROM t1 WHERE key1 < 3 OR key2 > 1020;
--echo
SELECT * FROM information_schema.OPTIMIZER_TRACE;

# group without range
--echo
EXPLAIN SELECT key2, MIN(key2_1) FROM t2 GROUP BY key2;
--echo
SELECT * FROM information_schema.OPTIMIZER_TRACE;

# distinct - group quick select without grouping attribute
EXPLAIN SELECT DISTINCT key2 FROM t2;
--echo
SELECT * FROM information_schema.OPTIMIZER_TRACE;

# group with range
--echo
EXPLAIN SELECT key2, MIN(key2_1) FROM t2 
WHERE key2 = 5 or key2 = 4 or key2 = 3 or key2 = 2 or key2 = 1 
GROUP BY key2;
--echo
SELECT * FROM information_schema.OPTIMIZER_TRACE;

#intersect
--echo
EXPLAIN SELECT * FROM t2 WHERE key2 = 1 AND (key2_1 = 1 OR key3 = 5);
--echo
SELECT * FROM information_schema.OPTIMIZER_TRACE;

# union
--echo 
EXPLAIN SELECT * FROM t1 WHERE key2=10 OR key3=3 OR key4 <=> null;
--echo
SELECT * FROM information_schema.OPTIMIZER_TRACE;

# range_scan_possible=false
--echo
EXPLAIN SELECT * FROM t2 WHERE key2_1 < 79 OR key2 = 2;    
--echo
SELECT * FROM information_schema.OPTIMIZER_TRACE;

# Multiple key parts in same index
--echo
EXPLAIN SELECT * FROM t2 WHERE key1a = 5 and key1b < 10;    
--echo
SELECT * FROM information_schema.OPTIMIZER_TRACE;

# Multiple ranges on key parts in same index
--echo
EXPLAIN SELECT * FROM t2 WHERE (key1a = 5 and key1b < 10 and key1b > 2) or
                               (key1a = 4 and key1b < 7 and key1b > 3);    
--echo
SELECT * FROM information_schema.OPTIMIZER_TRACE;

# Multiple ranges on key parts in same index
--echo
EXPLAIN SELECT * FROM t2 WHERE (key1b < 10 and key1b > 7) and 
                               (key1a = 4 or key1a = 5);
--echo
SELECT * FROM information_schema.OPTIMIZER_TRACE;

# more_expensive_than_table_scan
--echo
EXPLAIN SELECT * FROM t1 WHERE (key1 > 1 OR key2  > 2);
--echo
SELECT * FROM information_schema.OPTIMIZER_TRACE;

# Range analysis on straight join
--echo
EXPLAIN SELECT STRAIGHT_JOIN * FROM t1, t2 
  WHERE t1.key1=t2.key1a AND t1.key2 > 1020;
--echo
SELECT * FROM information_schema.OPTIMIZER_TRACE;

DROP TABLE t1,t2;

CREATE TABLE t1 (
  cola char(3) not null, 
  colb char(3) not null,  
  filler char(200),
  key(cola), 
  key(colb)
);
INSERT INTO t1 VALUES ('foo','bar', 'ZZ'),('fuz','baz', 'ZZ');

--echo Inserting records
--disable_query_log
let $1=9;
while ($1)
{
  eval INSERT INTO t1 SELECT * FROM t1 WHERE cola = 'foo';
  dec $1;
}
LET $1=13;
WHILE ($1)
{
  eval INSERT INTO t1 SELECT * FROM t1 WHERE cola <> 'foo';
  dec $1;
}

--enable_query_log

--echo
# Index roworder intersect
EXPLAIN SELECT * FROM t1 WHERE cola = 'foo' AND colb = 'bar';
--echo
SELECT * FROM information_schema.OPTIMIZER_TRACE;

--echo
# Range with escaped character should be printed escaped
EXPLAIN SELECT * FROM t1 WHERE cola = 'f\no';
--echo
SELECT * FROM information_schema.OPTIMIZER_TRACE;

DROP TABLE t1;

# Test that range optimization is not shown for every outer record 
# when there is a dynamic range.
CREATE TABLE t1(c INT);
INSERT INTO t1 VALUES (),();
CREATE TABLE t2 (b INT, KEY(b));
INSERT INTO t2 VALUES (),(),();

# First, enable dynamic range optimization tracing
SET optimizer_trace_features="greedy_search=off,dynamic_range=on";
EXPLAIN SELECT 1 FROM 
 (SELECT 1 FROM t2,t1 WHERE b < c GROUP BY 1 LIMIT 1) AS d2;
--echo
SELECT * FROM information_schema.OPTIMIZER_TRACE;
--echo

# Second, disable dynamic range optimization tracing
SET optimizer_trace_features="greedy_search=off,dynamic_range=off";
EXPLAIN SELECT 1 FROM 
 (SELECT 1 FROM t2,t1 WHERE b < c GROUP BY 1 LIMIT 1) AS d2;
--echo
SELECT * FROM information_schema.OPTIMIZER_TRACE;
--echo

DROP TABLE t1,t2;

SET optimizer_trace_features=default;

# Range analysis in test_if_skip_sort_order 
# (records_estimation_for_index_ordering)
CREATE TABLE t1 (
  i1 int,
  i2 int,
  c char(1),
  KEY k1 (i1),
  KEY k2 (i1, i2)
);
INSERT INTO t1 VALUES (0,1,'2'),(3,2,'1');

EXPLAIN SELECT * FROM t1 WHERE i1 > '2' ORDER BY i1, i2;
--echo
SELECT * FROM information_schema.OPTIMIZER_TRACE;
--echo

EXPLAIN SELECT DISTINCT i1 FROM t1 WHERE i1 >= '1' ORDER BY i1 DESC;
--echo
SELECT * FROM information_schema.OPTIMIZER_TRACE;
--echo

DROP TABLE t1;

# Analyze whether to use covering roworder intersect
CREATE TABLE t1 (
  pk INT PRIMARY KEY,
  i1 INT,
  i2 INT,
  v varchar(1),
  INDEX i1_idx (i1),
  INDEX v_idx (v,i1)
) ENGINE=InnoDB;

INSERT INTO t1 VALUES (1, 1, 9,'a'), (2, 2, 8,'b'), (3, 3, 7,'c'),
                      (4, 4, 6,'d'), (5, 5, 5,'e');

-- disable_query_log
-- disable_result_log
ANALYZE TABLE t1;
-- enable_result_log
-- enable_query_log

--echo
--echo # Covering ROR intersect not chosen: only one scan used
EXPLAIN SELECT v FROM t1 WHERE i1 = 1 AND v = 'a' AND pk < 3;
--echo
SELECT * FROM information_schema.OPTIMIZER_TRACE;

DROP INDEX i1_idx ON t1;
CREATE INDEX i1_i2_idx ON t1 (i2,i1);

ANALYZE TABLE t1;

--echo
--echo # Covering ROR intersect not chosen: cost
EXPLAIN SELECT v FROM t1 WHERE i1 = 1 AND i2 = 1  AND v = 'a' AND pk < 3;
--echo
SELECT * FROM information_schema.OPTIMIZER_TRACE;

# Todo: make a test case that chooses ROR intersect

--echo
DROP TABLE t1;

# Optimizer first decides to use ref, then changes mind to use range instead
CREATE TABLE t1 (a int, b int, PRIMARY KEY (a,b), KEY b (b)) ENGINE=InnoDB STATS_PERSISTENT=0;
INSERT INTO t1 VALUES (1,1),(1,2),(1,0),(1,3);

# -- disable_query_log
# -- disable_result_log
# ANALYZE TABLE t1;
# -- enable_result_log
# -- enable_query_log

--echo
--echo # Test trace for "access_type_changed 'ref' to 'range'"
EXPLAIN SELECT MAX(b), a FROM t1 WHERE b < 2 AND a = 1 GROUP BY a; 
--echo
SELECT * FROM information_schema.OPTIMIZER_TRACE;

--echo
drop table t1;

--echo #
--echo # Tracing of when test_if_skip_sort_order() switches to another
--echo # index and we abandon ICP,
--echo # see "disabling_pushed_condition_on_old_index" in trace.
--echo #

CREATE TABLE t1 (
  c1 VARCHAR(2) NOT NULL,
  i1 INTEGER NOT NULL,
  c2 VARCHAR(2) NOT NULL,
  KEY k1 (c1),
  KEY k2 (c1, i1)
);

INSERT INTO t1 VALUES ('0',3,'0'),('0',2,'1');

EXPLAIN SELECT * FROM t1 WHERE c1 = '1' ORDER BY i1;

SELECT * FROM information_schema.OPTIMIZER_TRACE;

DROP TABLE t1;
