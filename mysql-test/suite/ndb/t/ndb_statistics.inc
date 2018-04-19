-- source include/have_ndb.inc

CREATE TABLE t10(
  K INT NOT NULL AUTO_INCREMENT,
  I INT, J INT,
  PRIMARY KEY(K),
  KEY(I,J),
  UNIQUE KEY(J,K)
) ENGINE=ndbcluster
  partition by key (K) partitions 1;

INSERT INTO t10(I,J) VALUES (1,1),(2,2),(3,3),(4,4),(5,5),(6,6),(7,7),(8,8),(9,9),(0,0);

CREATE TABLE t100 LIKE t10;
INSERT INTO t100(I,J)
  SELECT X.J, X.J+(10*Y.J) FROM t10 AS X,t10 AS Y;

CREATE TABLE t10000 LIKE t10;

# Insert into t10000 into innodb and alter to ndb
# not to exhaust MaxNoOfConcurrentOperations
ALTER TABLE t10000 ENGINE=INNODB;
INSERT INTO t10000(I,J)
  SELECT X.J, X.J+(100*Y.J) FROM t100 AS X,t100 AS Y;
ALTER TABLE t10000 ENGINE=NDBCLUSTER;

ANALYZE TABLE t10,t100,t10000;

SELECT COUNT(*) FROM t10;
SELECT COUNT(*) FROM t100;
SELECT COUNT(*) FROM t10000;

#
# Bug #59517: Incorrect detection of single row access in
#             ha_ndbcluster::records_in_range()

# Expect a single row (or const) when PK is excact specified
EXPLAIN
SELECT * FROM t10000 WHERE k = 42;

# All queries below should *not* return a single row
EXPLAIN
SELECT * FROM t10000 WHERE k >= 42 and k < 10000;
EXPLAIN
SELECT * FROM t10000 WHERE k BETWEEN 42 AND 10000;
EXPLAIN
SELECT * FROM t10000 WHERE k < 42;
EXPLAIN
SELECT * FROM t10000 WHERE k > 42;

#
# Bug #59519 ::set_rec_per_key() assumes ORDER_INDEX to be unique
#

# 'REF' join of 'Y' should match >1 rows
EXPLAIN
SELECT * FROM t10000 AS x JOIN t10000 AS y
  ON y.i=x.i AND y.j = x.i;

#
# Bug #11804277: INCORRECT INDEX MAY BE SELECTED DUE TO INSUFFICIENT 
#                STATISTICS FROM CLUSTER
#

# Open bounded range should return 10% of #rows in table
EXPLAIN
SELECT * FROM t100 WHERE k < 42;
EXPLAIN
SELECT * FROM t100 WHERE k > 42;
EXPLAIN
SELECT * FROM t10000 WHERE k < 42;
EXPLAIN
SELECT * FROM t10000 WHERE k > 42;

#Closed bounded range should return 5% of #rows in table
EXPLAIN
SELECT * FROM t100 WHERE k BETWEEN 42 AND 10000;
EXPLAIN
SELECT * FROM t10000 WHERE k BETWEEN 42 AND 10000;

#EQ-range selectivity depends on
#  - key length specified
#  - #rows in table.
#  - unique/non-unique index
#  - min 2% selectivity
#
#  Possibly combined with open/closed ranges as
#  above which further improves selectivity
#
EXPLAIN
SELECT * FROM t10000 WHERE I = 0;
EXPLAIN
SELECT * FROM t10000 WHERE J = 0;

EXPLAIN
SELECT * FROM t10000 WHERE I = 0 AND J = 0;

EXPLAIN
SELECT * FROM t10000 WHERE I = 0;
EXPLAIN
SELECT * FROM t10000 WHERE I = 0 AND J > 1;
EXPLAIN
SELECT * FROM t10000 WHERE I = 0 AND J < 1;
EXPLAIN
SELECT * FROM t10000 WHERE I = 0 AND J BETWEEN 1 AND 10;
EXPLAIN
SELECT * FROM t10000 WHERE I = 0 AND J = 1;

EXPLAIN
SELECT * FROM t10000 WHERE J = 0;
EXPLAIN
SELECT * FROM t10000 WHERE J = 0 AND K > 1;
EXPLAIN
SELECT * FROM t10000 WHERE J = 0 AND K < 1;
EXPLAIN
SELECT * FROM t10000 WHERE J = 0 AND K BETWEEN 1 AND 10;
EXPLAIN
SELECT * FROM t10000 WHERE J = 0 AND K = 1;

## Verify selection of 'best' index
## (The one of index I/J being EQ)
EXPLAIN
SELECT * FROM t10000 WHERE I = 0 AND J <> 1;
EXPLAIN
SELECT * FROM t10000 WHERE I <> 0 AND J = 1;
EXPLAIN
SELECT * FROM t10000 WHERE I <> 0 AND J <> 1;

EXPLAIN
SELECT * FROM t10000 WHERE J <> 1 AND I = 0;
EXPLAIN
SELECT * FROM t10000 WHERE J = 1 AND I <> 0;
EXPLAIN
SELECT * FROM t10000 WHERE J <> 1 AND I <> 0;


DROP TABLE t10,t100,t10000;
