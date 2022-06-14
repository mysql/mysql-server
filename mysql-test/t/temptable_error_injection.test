-- source include/have_debug.inc
-- source include/not_hypergraph.inc  # Depends on the query plan.

CREATE TABLE t (c INT);

INSERT INTO t VALUES (1);

ANALYZE TABLE t;

# Let's use EXPLAIN FORMAT <stmt> to make sure that our <stmt> will
# result utilizing the TempTable. Try keeping this MTR test less flaky
# by checking only part of the EXPLAIN FORMAT <stmt> output.
--let SEARCH_PATTERN = Aggregate using temporary table
--let SEARCH_EXPRESSION = `EXPLAIN FORMAT=tree SELECT * FROM (SELECT COUNT(*) FROM t GROUP BY c) as dt`
--source include/search_pattern_on_variable.inc

SET debug = '+d,temptable_create_return_full';

SELECT * FROM (SELECT COUNT(*) FROM t GROUP BY c) as dt;

SET debug = '-d,temptable_create_return_full';

DROP TABLE t;
