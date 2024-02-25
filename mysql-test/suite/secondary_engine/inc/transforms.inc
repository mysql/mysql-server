--source include/elide_costs.inc

--disable_query_log
eval INSTALL PLUGIN mock SONAME '$MOCK_PLUGIN';
--enable_query_log

CREATE TABLE t1(a DATETIME NOT NULL);
INSERT INTO t1 VALUES (TIMESTAMP'2006-06-06 15:55:55');
ALTER TABLE t1 SECONDARY_ENGINE MOCK;
ALTER TABLE t1 SECONDARY_LOAD;
EXPLAIN SELECT max(a) from t1;

--echo # Check how the two transforms {scalar,table}-subquery-to-derived
--echo # behave in PREPARE-<change conditions>-EXECUTE--<change conditions>-EXECUTE
--echo
--echo # Transform is not active for InnoDB while we the do initial PREPARE
PREPARE s FROM "EXPLAIN SELECT a FROM t1 WHERE a=(SELECT MAX(a) FROM t1) AND (a=TIMESTAMP'2006-06-06 15:55:55')";
PREPARE s2 FROM "EXPLAIN SELECT a FROM t1 WHERE a NOT IN (SELECT a FROM t1 AS t2)";
SET secondary_engine_cost_threshold = 0;
EXECUTE s;
--echo
EXECUTE s2;
SET optimizer_switch="subquery_to_derived=on";
EXECUTE s;
--echo
EXECUTE s2;
SET optimizer_switch="subquery_to_derived=off";
EXECUTE s;
--echo
EXECUTE s2;
# The previous EXECUTE used MOCK and transformed the subquery to a derived
# table; the next EXECUTE starts with InnoDB, and chooses InnoDB because
# the cost is under the threshold; but, as the transformation was specifically
# done for MOCK (by the previous EXECUTE), and isn't required for InnoDB
# (subquery_to_derived is off), we have to reprepare:
SET secondary_engine_cost_threshold = 10000;
--error 0, ER_HYPERGRAPH_NOT_SUPPORTED_YET
EXECUTE s;
--echo
--error 0, ER_HYPERGRAPH_NOT_SUPPORTED_YET
EXECUTE s2;
# The previous EXECUTE used InnoDB and no transformation. The next EXECUTE
# starts with InnoDB, and does no transformation, even though one is requested
# by changing optimizer_switch. It is questionable behaviour, but not a real
# problem.
SET optimizer_switch="subquery_to_derived=on";
--error 0, ER_HYPERGRAPH_NOT_SUPPORTED_YET
EXECUTE s;
--echo
--error 0, ER_HYPERGRAPH_NOT_SUPPORTED_YET
EXECUTE s2;
SET optimizer_switch="subquery_to_derived=off";
--error 0, ER_HYPERGRAPH_NOT_SUPPORTED_YET
EXECUTE s;
--echo
--error 0, ER_HYPERGRAPH_NOT_SUPPORTED_YET
EXECUTE s2;
SET secondary_engine_cost_threshold = 0;
EXECUTE s;
--echo
EXECUTE s2;
SET optimizer_switch="subquery_to_derived=on";
EXECUTE s;
--echo
EXECUTE s2;
SET optimizer_switch="subquery_to_derived=off";
EXECUTE s;
--echo
EXECUTE s2;
SET optimizer_switch="subquery_to_derived=on";
--echo # Let transform be active for InnoDB while we the initial PREPARE
PREPARE s FROM "EXPLAIN SELECT a FROM t1 WHERE a=(SELECT MAX(a) FROM t1) AND (a=TIMESTAMP'2006-06-06 15:55:55')";
EXECUTE s;
--echo
EXECUTE s2;
SET secondary_engine_cost_threshold = 0;
EXECUTE s;
--echo
EXECUTE s2;
SET optimizer_switch="subquery_to_derived=on";
EXECUTE s;
--echo
EXECUTE s2;
SET optimizer_switch="subquery_to_derived=off";
EXECUTE s;
--echo
EXECUTE s2;
SET secondary_engine_cost_threshold = 10000;
--error 0, ER_HYPERGRAPH_NOT_SUPPORTED_YET
EXECUTE s;
--echo
--error 0, ER_HYPERGRAPH_NOT_SUPPORTED_YET
EXECUTE s2;
SET optimizer_switch="subquery_to_derived=on";
--error 0, ER_HYPERGRAPH_NOT_SUPPORTED_YET
EXECUTE s;
--echo
--error 0, ER_HYPERGRAPH_NOT_SUPPORTED_YET
EXECUTE s2;
SET optimizer_switch="subquery_to_derived=off";
--error 0, ER_HYPERGRAPH_NOT_SUPPORTED_YET
EXECUTE s;
--echo
--error 0, ER_HYPERGRAPH_NOT_SUPPORTED_YET
EXECUTE s2;
SET secondary_engine_cost_threshold = 0;
EXECUTE s;
--echo
EXECUTE s2;
SET optimizer_switch="subquery_to_derived=on";
EXECUTE s;
--echo
EXECUTE s2;
SET optimizer_switch="subquery_to_derived=off";
EXECUTE s;
--echo
EXECUTE s2;

DROP PREPARE s;
DROP PREPARE s2;
DROP TABLE t1;
SET secondary_engine_cost_threshold = default;
SET optimizer_switch="subquery_to_derived=default";

--echo #
--echo # Bug#34445328 (Hypergraph) Optimizer degenerate join
--echo # conditions when encountering semi join + outer join
--echo #

CREATE TABLE t (a INT) SECONDARY_ENGINE MOCK;
INSERT INTO t VALUES (), (), ();
ALTER TABLE t SECONDARY_LOAD;
ANALYZE TABLE t;

# The subquery_to_derived transformation attaches a degenerate join
# condition (t3.a=t4.a) to the left outer join. Since HeatWave does
# not accept left outer join conditions that reference only the
# right/inner side of the join, we create a filter just below the join
# for this case when a secondary engine is used (pushing such a
# condition down is perfectly valid). Note that currently, due to
# bug#34444550, the condition is dropped altogether with the old
# optimizer. With the hypergraph optimizer, expect no join conditions
# in the outer join, and instead have a filter with the t3.a=t4.a
# condition.
--replace_regex $elide_costs
EXPLAIN FORMAT=TREE
SELECT 1
FROM t AS t1 LEFT JOIN t AS t2
  ON 1 IN (
    SELECT 1 FROM t AS t3 WHERE a <=>
      (SELECT a FROM t AS t4)
  );

DROP TABLE t;

--echo #
--echo # Bug#34534373: Heatwave offload issue - Sees inner tables
--echo #               of a semijoin when it should not
--echo #

CREATE TABLE t1(f1 INTEGER, f2 INTEGER) SECONDARY_ENGINE MOCK;
ALTER TABLE t1 SECONDARY_LOAD;
ANALYZE TABLE t1;
set optimizer_switch="subquery_to_derived=on";
--replace_regex $elide_costs
# For the condition that is placed on top of the left join,
# hypergraph should not find field replacements from table
# "t3" through multiple equalities. This table is not visible
# outside of the semijoin.
EXPLAIN FORMAT=TREE
SELECT 1
 FROM (t1 JOIN t1 AS t2
       ON t2.f1 = t1.f2 AND t1.f1 IN (SELECT f1 FROM t1 AS t3))
 WHERE (SELECT SUM(f2) FROM t1 AS t4) IS NULL
        AND t1.f1 = t2.f1 OR t2.f2 <= 0;
DROP TABLE t1;
set optimizer_switch="subquery_to_derived=off";

--echo #
--echo # Bug#34828328: Assertion `right != nullptr' failed in
--echo #               make_join_hypergraph.cc

CREATE TABLE t1(f1 INTEGER, f2 INTEGER) SECONDARY_ENGINE MOCK;
ALTER TABLE t1 SECONDARY_LOAD;
ANALYZE TABLE t1;

set optimizer_switch="subquery_to_derived=on";
# The following query gets offloaded with the old optimizer,
# however it does not with hypergraph. The reason is that,
# the old optimizer is able to induce LIMIT clause to eliminate
# duplicates for the semijoin. Hypergraph however rejects the
# plan because of the semijoin condition between inner side of
# the semijoin and the outer side of the outer join making
# it not possible to create a hash joinable plan.
# This could possibly be addressed as part of solution to
# Bug#34448532.
let $query=
SELECT 1
FROM t1 JOIN t1 AS t2 ON (t2.f1 = t1.f1)
LEFT JOIN t1 AS t3 ON (t2.f2 = t1.f2 AND t2.f2 IN (SELECT f1 FROM t1 AS t4));

--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
eval $query;

DROP TABLE t1;
set optimizer_switch="subquery_to_derived=off";

--disable_query_log
UNINSTALL PLUGIN mock;
--enable_query_log
