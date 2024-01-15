#
# Tests for the BLACKHOLE storage engine
#

--source include/have_blackhole.inc

--echo #
--echo # Bug #11880012: INDEX_SUBQUERY, BLACKHOLE,
--echo #                HANG IN PREPARING WITH 100% CPU USAGE
--echo #

CREATE TABLE t1(a INT NOT NULL);
INSERT INTO t1 VALUES (1), (2), (3);
CREATE TABLE t2 (a INT UNSIGNED, b INT, UNIQUE KEY (a, b)) ENGINE=BLACKHOLE;

SELECT 1 FROM t1 WHERE a = ANY (SELECT a FROM t2);

DROP TABLE t1, t2;

--echo End of 5.5 tests 

--echo #
--echo # Bug#13948247 DIVISION BY 0 IN GET_BEST_DISJUNCT_QUICK WITH FORCE INDEX GROUP BY
--echo #

CREATE TABLE t1(a INT, b INT, c INT, KEY(c), UNIQUE(a)) ENGINE = BLACKHOLE;
SELECT 0 FROM t1 FORCE INDEX FOR GROUP BY(a) WHERE a = 0 OR b = 0 AND c = 0;
DROP TABLE t1;

--echo End of 5.6 tests

--echo #
--echo # Bug#33250020: regression: FullTextSearchIterator::Init():
--echo #               Assertion `m_ft_func->ft_handler != nullptr' failed.
--echo #

CREATE TABLE t(a VARCHAR(10), FULLTEXT (a)) ENGINE = BLACKHOLE;
INSERT INTO t VALUES ('abc'), ('xyz');
# This table scan worked even before the fix.
SELECT MATCH (a) AGAINST ('abc') AS score FROM t;
# But this index scan failed. Should succeed now.
SELECT 1 FROM t WHERE MATCH (a) AGAINST ('abc');
DROP TABLE t;

CREATE TABLE ta (pk INT, embedding VECTOR(4), PRIMARY KEY (pk)) ENGINE=BLACKHOLE;
INSERT INTO ta VALUES
(0, TO_VECTOR("[1,2,3,4]")),
(1, TO_VECTOR("[4,5,6,7]"));
--sorted_result
SELECT FROM_VECTOR(embedding) FROM ta;
DROP TABLE ta;
