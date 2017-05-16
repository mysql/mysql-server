#
# index_merge tests for statements using intersect algorithm
#

--disable_warnings
DROP TABLE IF EXISTS t1,t2;
--enable_warnings

CREATE TABLE t1 (
  pk MEDIUMINT NOT NULL AUTO_INCREMENT,
  a INT NOT NULL,
  b INT NOT NULL,
  c INT NOT NULL,
  d INT NOT NULL,
  PRIMARY KEY pk (pk),
  KEY idx_a (a),
  KEY idx_b (b),
  KEY idx_c (c),
  KEY idx_d (d)
);

--disable_query_log

# Inserting a lot of rows inorder to enable index_merge intersect

INSERT INTO t1(a,b,c,d) VALUES
               ( RAND()*5, RAND()*5, RAND()*5, RAND()*5 );
 
let $cnt=4;
while ($cnt)
{
  INSERT INTO t1(a,b,c,d) SELECT 6,6,6,6 FROM t1;
  dec $cnt;
}

INSERT INTO t1(a,b,c,d) SELECT 6, RAND()*5, RAND()*5, 
                                 RAND()*5 FROM t1 LIMIT 3;
INSERT INTO t1(a,b,c,d) SELECT RAND()*5, 6, RAND()*5, 
                                 RAND()*5 FROM t1 LIMIT 3;
INSERT INTO t1(a,b,c,d) SELECT RAND()*5, RAND()*5, 6, 
                                 RAND()*5 FROM t1 LIMIT 3;
INSERT INTO t1(a,b,c,d) SELECT RAND()*5, RAND()*5, 
                              RAND()*5, 6 FROM t1 LIMIT 3;

let $cnt=7;
while ($cnt)
{
  INSERT INTO t1(a,b,c,d) SELECT RAND()*5, RAND()*5, 
                              RAND()*5, RAND()*5 FROM t1;
  dec $cnt;
}
                      
--enable_query_log    

# The following statement analyzes and 
# stores the key distribution for a table.

ANALYZE TABLE t1;

# DELETEs are not included as index merge intersection 
# is disabled for DELETE statements.

# 1. UPDATE

--let $query = WHERE b=6 AND c=6 
--replace_result "idx_c,idx_b" "idx_b,idx_c"
--replace_column 10 #
--eval EXPLAIN UPDATE t1 SET a=2 $query
--eval SELECT COUNT(*), SUM(a) FROM t1 $query 
--eval UPDATE t1 SET a=2 $query
--eval SELECT COUNT(*), SUM(a) FROM t1 $query

--let $query = WHERE b=6 AND c=6 AND d=6
--replace_result idx_b idx_x idx_c idx_x idx_d idx_x
--replace_column 10 #
--eval EXPLAIN UPDATE t1 SET a=2 $query
--eval SELECT COUNT(*), SUM(a) FROM t1 $query
--eval UPDATE t1 SET a=2 $query
--eval SELECT COUNT(*), SUM(a) FROM t1 $query

--let $query = WHERE d=6 AND a IS NOT NULL AND b=6
--replace_result "idx_d,idx_b" "idx_b,idx_d"
--replace_column 10 #
--eval EXPLAIN UPDATE t1 SET c=6 $query
--eval SELECT COUNT(*), SUM(c) FROM t1 $query
--eval UPDATE t1 SET c=6 $query
--eval SELECT COUNT(*), SUM(c) FROM t1 $query

--let $query = WHERE d=6 AND a=6  AND c <> 6
--replace_result "idx_d,idx_a" "idx_a,idx_d"
--replace_column 10 #
--eval EXPLAIN UPDATE t1 SET b=0 $query
--eval SELECT COUNT(*), SUM(b) FROM t1 $query
--eval UPDATE t1 SET b=0 $query
--eval SELECT COUNT(*), SUM(b) FROM t1 $query

--let $query = WHERE d=6 AND a=6 AND c IN (1,2,3,4,5)
--replace_result "idx_d,idx_a" "idx_a,idx_d"
--replace_column 10 #
--eval EXPLAIN UPDATE t1 SET a=100 $query
--eval SELECT COUNT(*), SUM(a) FROM t1 $query
--eval UPDATE t1 SET a=100 $query
--eval SELECT COUNT(*), SUM(a) FROM t1 $query

--let $query = WHERE a=5 AND b=4 AND d<3
--replace_column 10 #
--eval EXPLAIN UPDATE t1 SET a=2 $query
--eval UPDATE t1 SET a=2 $query

# Any range condition over a primary key of an InnoDB table.

create table t2 (
  pk MEDIUMINT NOT NULL AUTO_INCREMENT,
  a INT NOT NULL,
  b INT NOT NULL,
  c INT NOT NULL,
  d INT NOT NULL,
  PRIMARY KEY pk (pk),
  KEY idx_a (a),
  KEY idx_b (b),
  KEY idx_c (c),
  KEY idx_d (d)
)engine=InnoDB;

INSERT INTO t2 SELECT * FROM t1;
ANALYZE TABLE t2;

--let $query = WHERE pk<2492 AND d=1
--replace_column 10 #
--eval EXPLAIN UPDATE t2 SET a=2 $query
--eval UPDATE t2 SET a=2 $query

# 2. REPLACE and INSERT  

CREATE TABLE t3(
  a INT NOT NULL,
  b INT NOT NULL,
  c INT NOT NULL,
  d INT NOT NULL,
  e INT
);

INSERT INTO t3(a,b,c,d,e) VALUES (3890,3890,3890,3890,3890);
INSERT INTO t3(a,b,c,d,e) VALUES (4000,4000,4000,4000,4000);

--let $query = INTO t3 SELECT * FROM t1 WHERE b=6 AND c=6
--replace_result "idx_c,idx_b" "idx_b,idx_c"
--replace_column 10 #
--eval EXPLAIN REPLACE $query
--eval SELECT COUNT(*) FROM t3
--eval REPLACE $query
--eval SELECT COUNT(*) FROM t3

--let $query = INTO t3 SELECT * FROM t1 WHERE d=6 AND a IS NOT NULL AND b=6
--replace_column 10 #
--replace_result "idx_d,idx_b" "idx_b,idx_d"
--eval EXPLAIN INSERT $query
--eval SELECT COUNT(*) FROM t3
--eval INSERT $query
--eval SELECT COUNT(*) FROM t3

# Test case for multi column set-up.

CREATE TABLE t4 (
  pk MEDIUMINT NOT NULL AUTO_INCREMENT,
  a INT NOT NULL,
  b INT NOT NULL,
  c INT NOT NULL,
  d INT NOT NULL,
  PRIMARY KEY pk (pk),
  KEY idx_cd (c,d),
  KEY idx_bd (b,d)
);

INSERT INTO t4 SELECT * FROM t1;

# The following statement analyzes and 
# stores the key distribution for a table.

ANALYZE TABLE t4;

--let $query = WHERE b=6 AND c=6 AND d=6
--replace_result idx_b idx_x idx_c idx_x idx_d idx_x
--replace_column 10 #
--eval EXPLAIN UPDATE t4 SET a=2 $query
--eval SELECT COUNT(*), SUM(a) FROM t4 $query
--eval UPDATE t4 SET a=2 $query
--eval SELECT COUNT(*), SUM(a)  FROM t4 $query

DROP TABLE t1,t2,t3,t4;

#
# end of test cases for intersect index_merge optimization technique
#

--echo #
--echo # Bug#24829050 INDEX_MERGE_INTERSECTION OPTIMIZATION CAUSES
--echo #              WRONG QUERY RESULTS
--echo #
CREATE TABLE t1 (ID int, PRIMARY KEY (ID));
CREATE TABLE t2 (ID int, c1 int DEFAULT NULL, c2 int, PRIMARY KEY (ID),
                 KEY c1_INDEX (c1), KEY c2_INDEX (c2));

insert into t1 values (1);
insert into t2 values (1,1,20),(2,1,20),(3,1,30),(4,2,30);

SELECT * FROM t1 WHERE t1.ID=1 AND EXISTS (
                 SELECT 1 FROM t2 WHERE t2.c2=30 AND t2.c1=t1.ID);

DROP TABLE t1;
DROP TABLE t2;

