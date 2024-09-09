--echo # WL#8244 Hints for subquery execution
--source include/elide_costs.inc
SET EXPLAIN_FORMAT=tree;

CREATE TABLE t1 (a INTEGER NOT NULL, b INT, PRIMARY KEY (a));
CREATE TABLE t2 (a INTEGER NOT NULL, KEY (a));
CREATE TABLE t3 (a INTEGER NOT NULL, b INT, KEY (a));
INSERT INTO t1 VALUES (1,10), (2,20), (3,30),  (4,40);
INSERT INTO t2 VALUES (2), (3), (4), (5);
INSERT INTO t3 VALUES (10,3), (20,4), (30,5);

ANALYZE TABLE t1, t2, t3;

--echo This query will normally use Table Pull-out
--replace_regex $elide_costs
EXPLAIN
SELECT * FROM t2 WHERE t2.a IN (SELECT a FROM t1);
--echo Check that we can disable SEMIJOIN transformation
--replace_regex $elide_costs
EXPLAIN
SELECT * FROM t2 WHERE t2.a IN (SELECT /*+ NO_SEMIJOIN() */ a FROM t1);
--echo Same with hint in outer query
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ NO_SEMIJOIN(@subq) */ * FROM t2
WHERE t2.a IN (SELECT /*+ QB_NAME(subq) */ a FROM t1);

--echo Query with two sub-queries
--replace_regex $elide_costs
EXPLAIN
SELECT * FROM t3
WHERE t3.a IN (SELECT a FROM t1 tx)
  AND t3.b IN (SELECT a FROM t1 ty);
--echo No SEMIJOIN transformation for first subquery
--replace_regex $elide_costs
EXPLAIN
SELECT * FROM t3
WHERE t3.a IN (SELECT /*+ NO_SEMIJOIN() */ a FROM t1 tx)
  AND t3.b IN (SELECT a FROM t1 ty);
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ NO_SEMIJOIN(@subq1) */ * FROM t3
WHERE t3.a IN (SELECT /*+ QB_NAME(`subq1`) */ a FROM t1 tx)
  AND t3.b IN (SELECT /*+ QB_NAME(subq2) */ a FROM t1 ty);
--echo No SEMIJOIN transformation for latter subquery
--replace_regex $elide_costs
EXPLAIN
SELECT * FROM t3
WHERE t3.a IN (SELECT a FROM t1 tx)
  AND t3.b IN (SELECT /*+ NO_SEMIJOIN() */ a FROM t1 ty);
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ NO_SEMIJOIN(@`subq2`) */ * FROM t3
WHERE t3.a IN (SELECT /*+ QB_NAME(subq1) */ a FROM t1 tx)
  AND t3.b IN (SELECT /*+ QB_NAME(subq2) */ a FROM t1 ty);
--echo No SEMIJOIN transformation for any subquery
--replace_regex $elide_costs
EXPLAIN
SELECT * FROM t3
WHERE t3.a IN (SELECT /*+ NO_SEMIJOIN() */ a FROM t1 tx)
  AND t3.b IN (SELECT /*+ NO_SEMIJOIN() */ a FROM t1 ty);
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ NO_SEMIJOIN(@subq1) NO_SEMIJOIN(@subq2) */ * FROM t3
WHERE t3.a IN (SELECT /*+ QB_NAME(subq1) */ a FROM t1 tx)
  AND t3.b IN (SELECT /*+ QB_NAME(subq2) */ a FROM t1 ty);

--echo Query with nested sub-queries
--replace_regex $elide_costs
EXPLAIN
SELECT * FROM t3
WHERE t3.a IN (SELECT /*+ QB_NAME(subq1) */ a FROM t1 tx
               WHERE tx.b IN (SELECT /*+ QB_NAME(subq2) */ a FROM t1 ty));
--echo No SEMIJOIN transformation for outer subquery
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ NO_SEMIJOIN(@subq1) */ * FROM t3
WHERE t3.a IN (SELECT /*+ QB_NAME(subq1) */ a FROM t1 tx
               WHERE tx.b IN (SELECT /*+ QB_NAME(subq2) */ a FROM t1 ty));
--echo No SEMIJOIN transformation for inner-most subquery
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ NO_SEMIJOIN(@subq2) */ * FROM t3
WHERE t3.a IN (SELECT /*+ QB_NAME(subq1) */ a FROM t1 tx
               WHERE tx.b IN (SELECT /*+ QB_NAME(subq2) */ a FROM t1 ty));
--echo No SEMIJOIN transformation at all
--replace_regex $elide_costs
EXPLAIN
SELECT /*+  NO_SEMIJOIN(@subq1) NO_SEMIJOIN(@subq2) */ * FROM t3
WHERE t3.a IN (SELECT /*+ QB_NAME(subq1) */ a FROM t1 tx
               WHERE tx.b IN (SELECT /*+ QB_NAME(subq2) */ a FROM t1 ty));

--echo This query does not support SEMIJOIN.  SEMIJOIN hint is ignored
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ SEMIJOIN(@subq) */ * FROM t2
WHERE t2.a IN (SELECT /*+ QB_NAME(subq) */ min(a) FROM t1 group by a);

--echo This query will get LooseScan by default
--replace_regex $elide_costs
EXPLAIN
SELECT * FROM t2 WHERE t2.a IN (SELECT /*+ QB_NAME(subq1) */ a FROM t3);
--echo Let's turn off LooseScan, FirstMatch is then SELECTed
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ NO_SEMIJOIN(@subq1 LOOSESCAN) */ * FROM t2
WHERE t2.a IN (SELECT /*+ QB_NAME(subq1) */ a FROM t3);
--echo Let's also turn off FirstMatch, MatLookup is then used
--echo Hypergraph will choose FirstMatch as MatLookup is not supported
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ NO_SEMIJOIN(@subq1 FIRSTMATCH, LOOSESCAN) */ * FROM t2
WHERE t2.a IN (SELECT /*+ QB_NAME(subq1) */ a FROM t3);
--echo Let's also turn off Materialization, DuplicateWeedout should then be used
--echo Hypergraph will choose FirstMatch as DuplicateWeedout is not supported
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ NO_SEMIJOIN(@subq1 FIRSTMATCH, LOOSESCAN, MATERIALIZATION) */ *
FROM t2
WHERE t2.a IN (SELECT /*+ QB_NAME(subq1) */ a FROM t3);
--echo If we turn off all strategies, DuplicateWeedout should still be used
--echo Hypergraph will choose FirstMatch as DuplicateWeedout is not supported
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ NO_SEMIJOIN(@subq1 FIRSTMATCH, LOOSESCAN, MATERIALIZATION,
           DUPSWEEDOUT) */ *
FROM t2
WHERE t2.a IN (SELECT /*+ QB_NAME(subq1) */ a FROM t3);
--echo Turn off non-used strategies, nothing should change.  Still Loosescan
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ NO_SEMIJOIN(@subq1 FIRSTMATCH, MATERIALIZATION, DUPSWEEDOUT) */ *
FROM t2
WHERE t2.a IN (SELECT /*+ QB_NAME(subq1) */ a FROM t3);

--echo Test same query with SEMIJOIN hint
--echo Forcing LooseScan, should not change anything
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ SEMIJOIN(@subq1 LOOSESCAN) */ * FROM t2
WHERE t2.a IN (SELECT /*+ QB_NAME(subq1) */ a FROM t3);
--echo Force FirstMatch
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ SEMIJOIN(@subq1 FIRSTMATCH) */ * FROM t2
WHERE t2.a IN (SELECT /*+ QB_NAME(subq1) */ a FROM t3);
--echo Force Materialization
--echo Hypergraph will choose FirstMatch as Materialization is not supported
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ SEMIJOIN(@subq1 MATERIALIZATION) */ * FROM t2
WHERE t2.a IN (SELECT /*+ QB_NAME(subq1) */ a FROM t3);
--echo Force DuplicateWeedout
--echo Hypergraph will choose FirstMatch as DuplicateWeedout is not supported
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ SEMIJOIN(@subq1 DUPSWEEDOUT) */ * FROM t2
WHERE t2.a IN (SELECT /*+ QB_NAME(subq1) */ a FROM t3);
--echo If LooseScan is among candidates, it will be used
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ SEMIJOIN(@subq1 LOOSESCAN, DUPSWEEDOUT) */ * FROM t2
WHERE t2.a IN (SELECT /*+ QB_NAME(subq1) */ a FROM t3);
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ SEMIJOIN(@subq1 LOOSESCAN, MATERIALIZATION, DUPSWEEDOUT) */ * FROM t2
WHERE t2.a IN (SELECT /*+ QB_NAME(subq1) */ a FROM t3);
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ SEMIJOIN(@subq1 LOOSESCAN, FIRSTMATCH, MATERIALIZATION,
           DUPSWEEDOUT) */ * FROM t2
WHERE t2.a IN (SELECT /*+ QB_NAME(subq1) */ a FROM t3);
--echo Drop LooseScan from list of strategies, FirstMatch will be used
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ SEMIJOIN(@subq1 FIRSTMATCH, MATERIALIZATION, DUPSWEEDOUT) */ * FROM t2
WHERE t2.a IN (SELECT /*+ QB_NAME(subq1) */ a FROM t3);
--echo Drop FirstMatch, MatLookup is next
--echo Hypergraph will choose FirstMatch as MatLookup is not supported
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ SEMIJOIN(@subq1 MATERIALIZATION, DUPSWEEDOUT) */ * FROM t2
WHERE t2.a IN (SELECT /*+ QB_NAME(subq1) */ a FROM t3);

--echo For this query LooseScan and Materialization is not applicable
--echo Hypergraph will choose LooseScan as it's done differently.
--replace_regex $elide_costs
EXPLAIN
SELECT * FROM t1
WHERE t1.b IN (SELECT /*+ QB_NAME(subq1) */ a FROM t3 WHERE t3.b = t1.a);
--echo Turn off all applicable strategies. DuplicateWeedout should be used
--echo Hypergraph will continue to choose LooseScan
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ NO_SEMIJOIN(@subq1 FIRSTMATCH, DUPSWEEDOUT) */ * FROM t1
WHERE t1.b IN (SELECT /*+ QB_NAME(subq1) */ a FROM t3 WHERE t3.b = t1.a);
--echo Similar with SEMIJOIN hint
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ SEMIJOIN(@subq1 LOOSESCAN, MATERIALIZATION) */ * FROM t1
WHERE t1.b IN (SELECT /*+ QB_NAME(subq1) */ a FROM t3 WHERE t3.b = t1.a);

--echo Test multiple subqueries.
--echo Default for this query is Loosecan for first and FirstMatch for latter
--replace_regex $elide_costs
EXPLAIN
SELECT * FROM t1
WHERE t1.a IN (SELECT /*+ QB_NAME(subq1) */ a FROM t3)
  AND t1.b IN (SELECT /*+ QB_NAME(subq2) */ a FROM t2);
--echo Forcing the default strategy should not change anything
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ SEMIJOIN(@subq1 LOOSESCAN) SEMIJOIN(@subq2 FIRSTMATCH) */ * FROM t1
WHERE t1.a IN (SELECT /*+ QB_NAME(subq1) */ a FROM t3)
  AND t1.b IN (SELECT /*+ QB_NAME(subq2) */ a FROM t2);
--echo Forcing a strategy for one, may change the other due to cost changes
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ SEMIJOIN(@subq1 FIRSTMATCH) */ * FROM t1
WHERE t1.a IN (SELECT /*+ QB_NAME(subq1) */ a FROM t3)
  AND t1.b IN (SELECT /*+ QB_NAME(subq2) */ a FROM t2);
--echo Forcing same strategy for both
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ SEMIJOIN(@subq1 FIRSTMATCH) SEMIJOIN(@subq2 FIRSTMATCH) */ * FROM t1
WHERE t1.a IN (SELECT /*+ QB_NAME(subq1) */ a FROM t3)
  AND t1.b IN (SELECT /*+ QB_NAME(subq2) */ a FROM t2);
--echo Loosescan for both is not possible,  ends up with DuplicateWeedout
--echo Hypergraph would pick LooseScan as its done differently.
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ SEMIJOIN(@subq1 LOOSESCAN) SEMIJOIN(@subq2 LOOSESCAN) */ * FROM t1
WHERE t1.a IN (SELECT /*+ QB_NAME(subq1) */ a FROM t3)
  AND t1.b IN (SELECT /*+ QB_NAME(subq2) */ a FROM t2);
--echo Swap strategies compared to default
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ SEMIJOIN(@subq1 FIRSTMATCH) SEMIJOIN(@subq2 LOOSESCAN) */ * FROM t1
WHERE t1.a IN (SELECT /*+ QB_NAME(subq1) */ a FROM t3)
  AND t1.b IN (SELECT /*+ QB_NAME(subq2) */ a FROM t2);
--echo Different subsets of strategies for different subqueries
--echo Hypergraph would pick FirstMatch for the subq2
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ SEMIJOIN(@subq1 FIRSTMATCH, LOOSESCAN)
           SEMIJOIN(@subq2 MATERIALIZATION, DUPSWEEDOUT) */ * FROM t1
WHERE t1.a IN (SELECT /*+ QB_NAME(subq1) */ a FROM t3)
  AND t1.b IN (SELECT /*+ QB_NAME(subq2) */ a FROM t2);
--echo Vice versa
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ SEMIJOIN(@subq1 MATERIALIZATION, DUPSWEEDOUT)
           SEMIJOIN(@subq2 FIRSTMATCH, LOOSESCAN) */ * FROM t1
WHERE t1.a IN (SELECT /*+ QB_NAME(subq1) */ a FROM t3)
  AND t1.b IN (SELECT /*+ QB_NAME(subq2) */ a FROM t2);
--echo Another combination
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ SEMIJOIN(@subq1 MATERIALIZATION, FIRSTMATCH)
           SEMIJOIN(@subq2 LOOSESCAN, DUPSWEEDOUT) */ * FROM t1
WHERE t1.a IN (SELECT /*+ QB_NAME(subq1) */ a FROM t3)
  AND t1.b IN (SELECT /*+ QB_NAME(subq2) */ a FROM t2);
--echo Turn off default
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ NO_SEMIJOIN(@subq1 LOOSESCAN)
           NO_SEMIJOIN(@subq2 FIRSTMATCH) */ * FROM t1
WHERE t1.a IN (SELECT /*+ QB_NAME(subq1) */ a FROM t3)
  AND t1.b IN (SELECT /*+ QB_NAME(subq2) */ a FROM t2);
--echo Also turn off 2nd choice. Gives DuplicateWeedout over both
--echo Hypergraph will choose FirstMatch as DuplicateWeedout is not supported.
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ NO_SEMIJOIN(@subq1 LOOSESCAN, FIRSTMATCH)
           NO_SEMIJOIN(@subq2 FIRSTMATCH, LOOSESCAN) */ * FROM t1
WHERE t1.a IN (SELECT /*+ QB_NAME(subq1) */ a FROM t3)
  AND t1.b IN (SELECT /*+ QB_NAME(subq2) */ a FROM t2);
--echo Also turn off DuplicateWeedout.  Materialization is only one left.
--echo Hypergraph will choose FirstMatch as Materialization is not supported.
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ NO_SEMIJOIN(@subq1 LOOSESCAN, FIRSTMATCH, DUPSWEEDOUT)
           NO_SEMIJOIN(@subq2 FIRSTMATCH, LOOSESCAN, DUPSWEEDOUT) */ * FROM t1
WHERE t1.a IN (SELECT /*+ QB_NAME(subq1) */ a FROM t3)
  AND t1.b IN (SELECT /*+ QB_NAME(subq2) */ a FROM t2);
--echo Force materialization with SEMIJOIN hints instead
--echo Hypergraph will choose FirstMatch as Materialization is not supported.
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ SEMIJOIN(@subq1 MATERIALIZATION)
           SEMIJOIN(@subq2 MATERIALIZATION) */ * FROM t1
WHERE t1.a IN (SELECT /*+ QB_NAME(subq1) */ a FROM t3)
  AND t1.b IN (SELECT /*+ QB_NAME(subq2) */ a FROM t2);
--echo This query gives DuplicateWeedout over both since combining
--echo DuplicateWeedout with another strategy does not seem possible.
--echo Hypergraph will choose FirstMatch as Materialization is not supported.
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ SEMIJOIN(@subq1 MATERIALIZATION)
           SEMIJOIN(@subq2 DUPSWEEDOUT) */ * FROM t1
WHERE t1.a IN (SELECT /*+ QB_NAME(subq1) */ a FROM t3)
  AND t1.b IN (SELECT /*+ QB_NAME(subq2) */ a FROM t2);
--echo More alternatives for 2nd subquery gives Materialization for first
--echo Hypergraph will choose FirstMatch as Materialization is not supported.
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ SEMIJOIN(@subq1 MATERIALIZATION)
           SEMIJOIN(@subq2 LOOSESCAN, FIRSTMATCH, DUPSWEEDOUT) */ * FROM t1
WHERE t1.a IN (SELECT /*+ QB_NAME(subq1) */ a FROM t3)
  AND t1.b IN (SELECT /*+ QB_NAME(subq2) */ a FROM t2);

--echo A query with nested subqueries which by default will use FirstMatch
--echo Hypergraph will choose LooseScan
--replace_regex $elide_costs
EXPLAIN
SELECT * FROM t1
WHERE t1.a IN (SELECT /*+ QB_NAME(subq1) */ a FROM t3
               WHERE t3.b IN (SELECT /*+ QB_NAME(subq2) */ a FROM t2));
--echo Let's turn off FirstMatch, Materialization is then selected
--echo Hypergraph will choose LooseScan
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ NO_SEMIJOIN(@subq1 FIRSTMATCH) */ * FROM t1
WHERE t1.a IN (SELECT /*+ QB_NAME(subq1) */ a FROM t3
               WHERE t3.b IN (SELECT /*+ QB_NAME(subq2) */ a FROM t2));
--echo Let's also turn off Materialization,  DuplicateWeedout is then used
--echo Hypergraph will choose LooseScan
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ NO_SEMIJOIN(@subq1 FIRSTMATCH, MATERIALIZATION) */ * FROM t1
WHERE t1.a IN (SELECT /*+ QB_NAME(subq1) */ a FROM t3
               WHERE t3.b IN (SELECT /*+ QB_NAME(subq2) */ a FROM t2));
--echo Also turn off DuplicateWeedout. LooseScan not usable; so still DuplicateWeedout
--echo Hypergraph will choose LooseScan
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ NO_SEMIJOIN(@subq1 FIRSTMATCH, MATERIALIZATION, DUPSWEEDOUT) */ *
FROM t1
WHERE t1.a IN (SELECT /*+ QB_NAME(subq1) */ a FROM t3
               WHERE t3.b IN (SELECT /*+ QB_NAME(subq2) */ a FROM t2));
--echo If we turn off all strategies, DuplicateWeedout should still be used
--echo Hypergraph will choose FirstMatch as DuplicateWeedout is not supported
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ NO_SEMIJOIN(@subq1 FIRSTMATCH, LOOSESCAN, MATERIALIZATION,
           DUPSWEEDOUT) */ *
FROM t1
WHERE t1.a IN (SELECT /*+ QB_NAME(subq1) */ a FROM t3
               WHERE t3.b IN (SELECT /*+ QB_NAME(subq2) */ a FROM t2));

--echo Test same query with SEMIJOIN hint
--echo Force FirstMatch, should not change anything
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ SEMIJOIN(@subq1 FIRSTMATCH) */ * FROM t1
WHERE t1.a IN (SELECT /*+ QB_NAME(subq1) */ a FROM t3
               WHERE t3.b IN (SELECT /*+ QB_NAME(subq2) */ a FROM t2));
--echo Force LooseScan, will use DuplicateWeedout
--echo Hypergraph will choose LooseScan
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ SEMIJOIN(@subq1 LOOSESCAN) */ * FROM t1
WHERE t1.a IN (SELECT /*+ QB_NAME(subq1) */ a FROM t3
               WHERE t3.b IN (SELECT /*+ QB_NAME(subq2) */ a FROM t2));
--echo Force Materialization
--echo Hypergraph will choose FirstMatch as Materialization is not supported
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ SEMIJOIN(@subq1 MATERIALIZATION) */ * FROM t1
WHERE t1.a IN (SELECT /*+ QB_NAME(subq1) */ a FROM t3
               WHERE t3.b IN (SELECT /*+ QB_NAME(subq2) */ a FROM t2));
--echo Force DuplicateWeedout
--echo Hypergraph will choose FirstMatch as DuplicateWeedout is not supported
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ SEMIJOIN(@subq1 DUPSWEEDOUT) */ * FROM t1
WHERE t1.a IN (SELECT /*+ QB_NAME(subq1) */ a FROM t3
               WHERE t3.b IN (SELECT /*+ QB_NAME(subq2) */ a FROM t2));
--echo If FirstMatch is among candidates, it will be used
--echo Default for Hyeprgraph here is LooseScan
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ SEMIJOIN(@subq1 FIRSTMATCH, DUPSWEEDOUT) */ * FROM t1
WHERE t1.a IN (SELECT /*+ QB_NAME(subq1) */ a FROM t3
               WHERE t3.b IN (SELECT /*+ QB_NAME(subq2) */ a FROM t2));
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ SEMIJOIN(@subq1 FIRSTMATCH, LOOSESCAN, DUPSWEEDOUT) */ * FROM t1
WHERE t1.a IN (SELECT /*+ QB_NAME(subq1) */ a FROM t3
               WHERE t3.b IN (SELECT /*+ QB_NAME(subq2) */ a FROM t2));
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ SEMIJOIN(@subq1 FIRSTMATCH, MATERIALIZATION, LOOSESCAN,
           DUPSWEEDOUT) */ * FROM t1
WHERE t1.a IN (SELECT /*+ QB_NAME(subq1) */ a FROM t3
               WHERE t3.b IN (SELECT /*+ QB_NAME(subq2) */ a FROM t2));
--echo Drop FirstMatch. Materialization will be used
--echo Hypergraph will use LooseScan
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ SEMIJOIN(@subq1 MATERIALIZATION, LOOSESCAN, DUPSWEEDOUT) */ * FROM t1
WHERE t1.a IN (SELECT /*+ QB_NAME(subq1) */ a FROM t3
               WHERE t3.b IN (SELECT /*+ QB_NAME(subq2) */ a FROM t2));
--echo Drop Materialization, DuplicateWeedout next
--echo Hypergraph will use LooseScan
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ SEMIJOIN(@subq1 LOOSESCAN, DUPSWEEDOUT) */ * FROM t1
WHERE t1.a IN (SELECT /*+ QB_NAME(subq1) */ a FROM t3
               WHERE t3.b IN (SELECT /*+ QB_NAME(subq2) */ a FROM t2));
--echo Strategy hints on inner-most query is ignored since sj-nests are merged
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ NO_SEMIJOIN(@subq2 FIRSTMATCH) */ * FROM t1
WHERE t1.a IN (SELECT /*+ QB_NAME(subq1) */ a FROM t3
               WHERE t3.b IN (SELECT /*+ QB_NAME(subq2) */ a FROM t2));
--echo Ditto
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ SEMIJOIN(@subq2 MATERIALIZATION) */ * FROM t1
WHERE t1.a IN (SELECT /*+ QB_NAME(subq1) */ a FROM t3
               WHERE t3.b IN (SELECT /*+ QB_NAME(subq2) */ a FROM t2));

--echo Turn off semijoin for outer subquery. FirstMatch is used for inner
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ NO_SEMIJOIN(@subq1) */ * FROM t1
WHERE t1.a IN (SELECT /*+ QB_NAME(subq1) */ a FROM t3
               WHERE t3.b IN (SELECT /*+ QB_NAME(subq2) */ a FROM t2));
--echo Do not use FirstMatch for inner
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ NO_SEMIJOIN(@subq1) NO_SEMIJOIN(@subq2 FIRSTMATCH) */ * FROM t1
WHERE t1.a IN (SELECT /*+ QB_NAME(subq1) */ a FROM t3
               WHERE t3.b IN (SELECT /*+ QB_NAME(subq2) */ a FROM t2));
--echo Do not use FirstMatch nor Materialization for inner
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ NO_SEMIJOIN(@subq1)
           NO_SEMIJOIN(@subq2 FIRSTMATCH, MATERIALIZATION) */ * FROM t1
WHERE t1.a IN (SELECT /*+ QB_NAME(subq1) */ a FROM t3
               WHERE t3.b IN (SELECT /*+ QB_NAME(subq2) */ a FROM t2));
--echo LooseScan is last resort
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ NO_SEMIJOIN(@subq1)
           NO_SEMIJOIN(@subq2 FIRSTMATCH, MATERIALIZATION, DUPSWEEDOUT) */ *
FROM t1
WHERE t1.a IN (SELECT /*+ QB_NAME(subq1) */ a FROM t3
               WHERE t3.b IN (SELECT /*+ QB_NAME(subq2) */ a FROM t2));
--echo Allow all stragies except default
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ NO_SEMIJOIN(@subq1)
           SEMIJOIN(@subq2 MATERIALIZATION, DUPSWEEDOUT, LOOSESCAN) */ *
FROM t1
WHERE t1.a IN (SELECT /*+ QB_NAME(subq1) */ a FROM t3
               WHERE t3.b IN (SELECT /*+ QB_NAME(subq2) */ a FROM t2));
--echo Force a particular strategy
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ NO_SEMIJOIN(@subq1) SEMIJOIN(@subq2 LOOSESCAN) */ * FROM t1
WHERE t1.a IN (SELECT /*+ QB_NAME(subq1) */ a FROM t3
               WHERE t3.b IN (SELECT /*+ QB_NAME(subq2) */ a FROM t2));

--echo Turn off semijoin for inner-most subquery.  FirstMatch is used for outer
--echo Hypergraph would prefer LooseScan
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ NO_SEMIJOIN(@subq2) */ * FROM t1
WHERE t1.a IN (SELECT /*+ QB_NAME(subq1) */ a FROM t3
               WHERE t3.b IN (SELECT /*+ QB_NAME(subq2) */ a FROM t2));
--echo Do not use FirstMatch for outer
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ NO_SEMIJOIN(@subq1 FIRSTMATCH) NO_SEMIJOIN(@subq2) */ * FROM t1
WHERE t1.a IN (SELECT /*+ QB_NAME(subq1) */ a FROM t3
               WHERE t3.b IN (SELECT /*+ QB_NAME(subq2) */ a FROM t2));
--echo Do not use FirstMatch nor Materialization for outer
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ NO_SEMIJOIN(@subq1 FIRSTMATCH, MATERIALIZATION)
       	   NO_SEMIJOIN(@subq2) */ * FROM t1
WHERE t1.a IN (SELECT /*+ QB_NAME(subq1) */ a FROM t3
               WHERE t3.b IN (SELECT /*+ QB_NAME(subq2) */ a FROM t2));
--echo LooseScan can not be used since index scan would not be "covering"
--echo Hypergraph can do LooseScan here
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ NO_SEMIJOIN(@subq1 FIRSTMATCH, MATERIALIZATION, DUPSWEEDOUT)
       	   NO_SEMIJOIN(@subq2) */ * FROM t1
WHERE t1.a IN (SELECT /*+ QB_NAME(subq1) */ a FROM t3
               WHERE t3.b IN (SELECT /*+ QB_NAME(subq2) */ a FROM t2));
--echo Allow all stragies except default
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ SEMIJOIN(@subq1 MATERIALIZATION, DUPSWEEDOUT, LOOSESCAN)
       	   NO_SEMIJOIN(@subq2) */ * FROM t1
WHERE t1.a IN (SELECT /*+ QB_NAME(subq1) */ a FROM t3
               WHERE t3.b IN (SELECT /*+ QB_NAME(subq2) */ a FROM t2));
--echo Force a particular strategy
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ SEMIJOIN(@subq1 DUPSWEEDOUT) NO_SEMIJOIN(@subq2) */ * FROM t1
WHERE t1.a IN (SELECT /*+ QB_NAME(subq1) */ a FROM t3
               WHERE t3.b IN (SELECT /*+ QB_NAME(subq2) */ a FROM t2));
--echo Turn off semijoin for both subqueries
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ NO_SEMIJOIN(@subq1) NO_SEMIJOIN(@subq2) */ * FROM t1
WHERE t1.a IN (SELECT /*+ QB_NAME(subq1) */ a FROM t3
               WHERE t3.b IN (SELECT /*+ QB_NAME(subq2) */ a FROM t2));

--echo Test hints with prepared statements
PREPARE stmt1 FROM "EXPLAIN
SELECT /*+ NO_SEMIJOIN(@subq1 FIRSTMATCH, LOOSESCAN)
           NO_SEMIJOIN(@subq2 FIRSTMATCH, LOOSESCAN) */ * FROM t1
WHERE t1.a IN (SELECT /*+ QB_NAME(subq1) */ a FROM t3)
  AND t1.b IN (SELECT /*+ QB_NAME(subq2) */ a FROM t2)";
--replace_regex $elide_costs
EXECUTE stmt1;
--replace_regex $elide_costs
EXECUTE stmt1;
DEALLOCATE PREPARE stmt1;
--echo Another Prepared Statement test
PREPARE stmt1 FROM "EXPLAIN
SELECT /*+ NO_SEMIJOIN(@subq1) SEMIJOIN(@subq2 LOOSESCAN) */ * FROM t1
WHERE t1.a IN (SELECT /*+ QB_NAME(subq1) */ a FROM t3
               WHERE t3.b IN (SELECT /*+ QB_NAME(subq2) */ a FROM t2))";
--replace_regex $elide_costs
EXECUTE stmt1;
--replace_regex $elide_costs
EXECUTE stmt1;
DEALLOCATE PREPARE stmt1;

SET optimizer_switch = default;

--echo Tests with non-default optimizer_switch settings

SET optimizer_switch = 'semijoin=off';
--echo No table pull-out for this query
--replace_regex $elide_costs
EXPLAIN
SELECT * FROM t2 WHERE t2.a IN (SELECT a FROM t1);
--echo This should not change anything
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ NO_SEMIJOIN(@subq) */ * FROM t2
WHERE t2.a IN (SELECT /*+ QB_NAME(subq) */ a FROM t1);
--echo Force semijoin
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ SEMIJOIN(@subq) */ * FROM t2
WHERE t2.a IN (SELECT /*+ QB_NAME(subq) */ a FROM t1);
--echo Setting strategy should still force semijoin
--echo Strategy is ignored since table pull-out is done
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ SEMIJOIN(@subq FIRSTMATCH) */ * FROM t2
WHERE t2.a IN (SELECT /*+ QB_NAME(subq) */ a FROM t1);
--echo Query with two sub-queries
--replace_regex $elide_costs
EXPLAIN
SELECT * FROM t3
WHERE t3.a IN (SELECT a FROM t1 tx)
  AND t3.b IN (SELECT a FROM t1 ty);
--echo SEMIJOIN transformation for first subquery
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ SEMIJOIN(@subq1) */ * FROM t3
WHERE t3.a IN (SELECT /*+ QB_NAME(subq1) */ a FROM t1 tx)
  AND t3.b IN (SELECT /*+ QB_NAME(subq2) */ a FROM t1 ty);
--echo SEMIJOIN transformation for latter subquery
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ SEMIJOIN(@subq2) */ * FROM t3
WHERE t3.a IN (SELECT /*+ QB_NAME(subq1) */ a FROM t1 tx)
  AND t3.b IN (SELECT /*+ QB_NAME(subq2) */ a FROM t1 ty);
--echo SEMIJOIN transformation for both subqueries
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ SEMIJOIN(@subq1) SEMIJOIN(@subq2) */ * FROM t3
WHERE t3.a IN (SELECT /*+ QB_NAME(subq1) */ a FROM t1 tx)
  AND t3.b IN (SELECT /*+ QB_NAME(subq2) */ a FROM t1 ty);
--echo Query with nested sub-queries
--replace_regex $elide_costs
EXPLAIN
SELECT * FROM t3
WHERE t3.a IN (SELECT /*+ QB_NAME(subq1) */ a FROM t1 tx
               WHERE tx.b IN (SELECT /*+ QB_NAME(subq2) */ a FROM t1 ty));
--echo SEMIJOIN transformation for outer subquery
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ SEMIJOIN(@subq1) */ * FROM t3
WHERE t3.a IN (SELECT /*+ QB_NAME(subq1) */ a FROM t1 tx
               WHERE tx.b IN (SELECT /*+ QB_NAME(subq2) */ a FROM t1 ty));
--echo SEMIJOIN transformation for inner-most subquery
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ SEMIJOIN(@subq2) */ * FROM t3
WHERE t3.a IN (SELECT /*+ QB_NAME(subq1) */ a FROM t1 tx
               WHERE tx.b IN (SELECT /*+ QB_NAME(subq2) */ a FROM t1 ty));
--echo SEMIJOIN transformation for both
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ SEMIJOIN(@subq1) SEMIJOIN(@subq2) */ * FROM t3
WHERE t3.a IN (SELECT /*+ QB_NAME(subq1) */ a FROM t1 tx
               WHERE tx.b IN (SELECT /*+ QB_NAME(subq2) */ a FROM t1 ty));

--echo Test strategies when some are disabled by optimizer_switch
SET optimizer_switch='semijoin=on';

SET optimizer_switch='loosescan=off';
--echo This query will get LooseScan by default. FirstMatch now.
--replace_regex $elide_costs
EXPLAIN
SELECT * FROM t2 WHERE t2.a IN (SELECT /*+ QB_NAME(subq1) */ a FROM t3);
--echo Let's turn off LooseScan also by hint, FirstMatch should still be SELECTed
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ NO_SEMIJOIN(@subq1 LOOSESCAN) */ * FROM t2
WHERE t2.a IN (SELECT /*+ QB_NAME(subq1) */ a FROM t3);
--echo Let's also turn off FirstMatch, MatLookup should then be used
--echo Hypergraph will use FirstMatch as MatLookup is not supported
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ NO_SEMIJOIN(@subq1 FIRSTMATCH) */ * FROM t2
WHERE t2.a IN (SELECT /*+ QB_NAME(subq1) */ a FROM t3);
--echo Let's also turn off Materialization, DuplicateWeedout should then be used
--echo Hypergraph will use FirstMatch as DupliecateWeedout MatLookup is not supported
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ NO_SEMIJOIN(@subq1 FIRSTMATCH, MATERIALIZATION) */ *
FROM t2
WHERE t2.a IN (SELECT /*+ QB_NAME(subq1) */ a FROM t3);

--echo Let's force LooseScan back on
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ SEMIJOIN(@subq1 LOOSESCAN) */ * FROM t2
WHERE t2.a IN (SELECT /*+ QB_NAME(subq1) */ a FROM t3);
--echo Forcing another strategy
--echo Hypergraph will use the default - FirstMatch
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ SEMIJOIN(@subq1 MATERIALIZATION) */ * FROM t2
WHERE t2.a IN (SELECT /*+ QB_NAME(subq1) */ a FROM t3);
--echo If LooseScan is among candidates, it is used even if originally disabled
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ SEMIJOIN(@subq1 LOOSESCAN, DUPSWEEDOUT) */ * FROM t2
WHERE t2.a IN (SELECT /*+ QB_NAME(subq1) */ a FROM t3);
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ SEMIJOIN(@subq1 LOOSESCAN, MATERIALIZATION, DUPSWEEDOUT) */ * FROM t2
WHERE t2.a IN (SELECT /*+ QB_NAME(subq1) */ a FROM t3);
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ SEMIJOIN(@subq1 LOOSESCAN, FIRSTMATCH, MATERIALIZATION,
           DUPSWEEDOUT) */ * FROM t2
WHERE t2.a IN (SELECT /*+ QB_NAME(subq1) */ a FROM t3);

--echo Disable another strategy
SET optimizer_switch='firstmatch=off';

--echo Turn on FirstMatch, but not LooseScan on with hint
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ SEMIJOIN(@subq1 FIRSTMATCH, MATERIALIZATION, DUPSWEEDOUT) */ * FROM t2
WHERE t2.a IN (SELECT /*+ QB_NAME(subq1) */ a FROM t3);
--echo Drop all remaining strategies with hint, should use DuplicateWeedout
--echo Hypergraph will use FirstMatch
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ NO_SEMIJOIN(@subq1 MATERIALIZATION, DUPSWEEDOUT) */ * FROM t2
WHERE t2.a IN (SELECT /*+ QB_NAME(subq1) */ a FROM t3);

--echo For this query LooseScan and Materialization is not applicable
--echo Should use DuplicateWeedout since FirstMatch is disabled
--echo Hypergraph will use FirstMatch as DuplicateWeedout is not supported
--replace_regex $elide_costs
EXPLAIN
SELECT * FROM t1
WHERE t1.b IN (SELECT /*+ QB_NAME(subq1) */ a FROM t3 WHERE t3.b = t1.a);
--echo Turn off all applicable strategies. DuplicateWeedout should still be used
--echo Hypergraph will use FirstMatch as DuplicateWeedout is not supported
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ NO_SEMIJOIN(@subq1 FIRSTMATCH, DUPSWEEDOUT) */ * FROM t1
WHERE t1.b IN (SELECT /*+ QB_NAME(subq1) */ a FROM t3 WHERE t3.b = t1.a);
--echo Reverse which strategies are allowed with hint
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ SEMIJOIN(@subq1 LOOSESCAN, FIRSTMATCH) */ * FROM t1
WHERE t1.b IN (SELECT /*+ QB_NAME(subq1) */ a FROM t3 WHERE t3.b = t1.a);

--echo Default for this query is Loosecan for first and FirstMatch for latter
--echo Since both strategies are disabled, will now use DuplicateWeedout
--echo Hypergraph will use FirstMatch as DuplicateWeedout is not supported
--replace_regex $elide_costs
EXPLAIN
SELECT * FROM t1
WHERE t1.a IN (SELECT /*+ QB_NAME(subq1) */ a FROM t3)
  AND t1.b IN (SELECT /*+ QB_NAME(subq2) */ a FROM t2);
--echo Allowing LooseScan and FirstMatch and optimizer_switch is ignored
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ SEMIJOIN(@subq1 LOOSESCAN, FIRSTMATCH)
       	   SEMIJOIN(@subq2 LOOSESCAN, FIRSTMATCH) */ * FROM t1
WHERE t1.a IN (SELECT /*+ QB_NAME(subq1) */ a FROM t3)
  AND t1.b IN (SELECT /*+ QB_NAME(subq2) */ a FROM t2);
--echo Forcing a disabled strategy for one
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ SEMIJOIN(@subq1 FIRSTMATCH) */ * FROM t1
WHERE t1.a IN (SELECT /*+ QB_NAME(subq1) */ a FROM t3)
  AND t1.b IN (SELECT /*+ QB_NAME(subq2) */ a FROM t2);
--echo Forcing same strategy for both
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ SEMIJOIN(@subq1 FIRSTMATCH) SEMIJOIN(@subq2 FIRSTMATCH) */ * FROM t1
WHERE t1.a IN (SELECT /*+ QB_NAME(subq1) */ a FROM t3)
  AND t1.b IN (SELECT /*+ QB_NAME(subq2) */ a FROM t2);
--echo Swap strategies compared to default
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ SEMIJOIN(@subq1 FIRSTMATCH) SEMIJOIN(@subq2 LOOSESCAN) */ * FROM t1
WHERE t1.a IN (SELECT /*+ QB_NAME(subq1) */ a FROM t3)
  AND t1.b IN (SELECT /*+ QB_NAME(subq2) */ a FROM t2);
--echo Different subsets of strategies for different subqueries
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ SEMIJOIN(@subq1 FIRSTMATCH, LOOSESCAN)
           SEMIJOIN(@subq2 MATERIALIZATION, DUPSWEEDOUT) */ * FROM t1
WHERE t1.a IN (SELECT /*+ QB_NAME(subq1) */ a FROM t3)
  AND t1.b IN (SELECT /*+ QB_NAME(subq2) */ a FROM t2);
--echo Turn off DuplicateWeedout for both.  Materialization is left
--echo Hypergraph will use FirstMatch as DuplicateWeedout is not supported
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ NO_SEMIJOIN(@subq1 DUPSWEEDOUT)
           NO_SEMIJOIN(@subq2 DUPSWEEDOUT) */ * FROM t1
WHERE t1.a IN (SELECT /*+ QB_NAME(subq1) */ a FROM t3)
  AND t1.b IN (SELECT /*+ QB_NAME(subq2) */ a FROM t2);
--echo Forcing materialization should have same effect
--echo Hypergraph will use FirstMatch as Materialization is not supported
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ SEMIJOIN(@subq1 MATERIALIZATION)
           SEMIJOIN(@subq2 MATERIALIZATION) */ * FROM t1
WHERE t1.a IN (SELECT /*+ QB_NAME(subq1) */ a FROM t3)
  AND t1.b IN (SELECT /*+ QB_NAME(subq2) */ a FROM t2);
--echo Turn off DuplicateWeedout for first.  Materialization is used for both
--echo Hypergraph will use FirstMatch as DuplicateWeedout is not supported
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ NO_SEMIJOIN(@subq1 DUPSWEEDOUT) */ * FROM t1
WHERE t1.a IN (SELECT /*+ QB_NAME(subq1) */ a FROM t3)
  AND t1.b IN (SELECT /*+ QB_NAME(subq2) */ a FROM t2);
--echo Turn off DuplicateWeedout for second.  Same effect.
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ NO_SEMIJOIN(@subq2 DUPSWEEDOUT) */ * FROM t1
WHERE t1.a IN (SELECT /*+ QB_NAME(subq1) */ a FROM t3)
  AND t1.b IN (SELECT /*+ QB_NAME(subq2) */ a FROM t2);

--echo Enable all strategies except DuplicateWeedout
SET optimizer_switch='firstmatch=on,loosescan=on,materialization=on,duplicateweedout=off';

--echo If we turn off all other strategies, DuplicateWeedout will be used
--echo Hypergraph will choose FirstMatch
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ NO_SEMIJOIN(@subq1 LOOSESCAN, FIRSTMATCH, MATERIALIZATION) */ *
FROM t2
WHERE t2.a IN (SELECT /*+ QB_NAME(subq1) */ a FROM t3);
--echo LooseScan and Materialization is not applicable, FirstMatch is used
--echo Hypergraph can do LooseScan here. So it will pick it.
--replace_regex $elide_costs
EXPLAIN
SELECT * FROM t1
WHERE t1.b IN (SELECT /*+ QB_NAME(subq1) */ a FROM t3 WHERE t3.b = t1.a);
--echo Turn off all applicable strategies. DuplicateWeedout should be used
--echo Hypergraph can do LooseScan here. So it will pick it.
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ NO_SEMIJOIN(@subq1 FIRSTMATCH) */ * FROM t1
WHERE t1.b IN (SELECT /*+ QB_NAME(subq1) */ a FROM t3 WHERE t3.b = t1.a);
--echo Similar with SEMIJOIN hint
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ SEMIJOIN(@subq1 LOOSESCAN, MATERIALIZATION) */ * FROM t1
WHERE t1.b IN (SELECT /*+ QB_NAME(subq1) */ a FROM t3 WHERE t3.b = t1.a);

--echo Disable all strategies
SET optimizer_switch='firstmatch=off,loosescan=off,materialization=off,duplicateweedout=off';
--echo DuplicateWeedout is then used
--echo For Hypergraph it will be FirstMatch
--replace_regex $elide_costs
EXPLAIN
SELECT * FROM t2 WHERE t2.a IN (SELECT /*+ QB_NAME(subq1) */ a FROM t3);
--echo Turning off extra strategies should not change anything
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ NO_SEMIJOIN(@subq1 LOOSESCAN, DUPSWEEDOUT) */ *
FROM t2
WHERE t2.a IN (SELECT /*+ QB_NAME(subq1) */ a FROM t3);
--echo Turning on some strategies should give one of those
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ SEMIJOIN(@subq1 FIRSTMATCH, MATERIALIZATION) */ *
FROM t2
WHERE t2.a IN (SELECT /*+ QB_NAME(subq1) */ a FROM t3);
--echo For this query that cannot use LooseScan or Materialization,
--echo turning those on will still give DupliateWeedout
--echo Hypergraph will pick LooseScan.
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ SEMIJOIN(@subq1 LOOSESCAN, MATERIALIZATION) */ * FROM t1
WHERE t1.b IN (SELECT /*+ QB_NAME(subq1) */ a FROM t3 WHERE t3.b = t1.a);
--echo Turning on FirstMatch should give FirstMatch
--echo Hypergraph will continue to pick LooseScan.
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ SEMIJOIN(@subq1 LOOSESCAN, FIRSTMATCH) */ * FROM t1
WHERE t1.b IN (SELECT /*+ QB_NAME(subq1) */ a FROM t3 WHERE t3.b = t1.a);

SET optimizer_switch = default;

--echo Test that setting optimizer_switch after prepare will change strategy
PREPARE stmt1 FROM "EXPLAIN
SELECT /*+ NO_SEMIJOIN(@subq1 FIRSTMATCH, LOOSESCAN)
           NO_SEMIJOIN(@subq2 FIRSTMATCH, LOOSESCAN) */ * FROM t1
WHERE t1.a IN (SELECT /*+ QB_NAME(subq1) */ a FROM t3)
  AND t1.b IN (SELECT /*+ QB_NAME(subq2) */ a FROM t2)";
--replace_regex $elide_costs
EXECUTE stmt1;
SET optimizer_switch = 'duplicateweedout=off';
--echo Will now use materialization
--echo No effect for Hypergraph
EXECUTE stmt1;
SET optimizer_switch = 'duplicateweedout=on';
--echo Turn DuplicateWeedout back on
EXECUTE stmt1;
DEALLOCATE PREPARE stmt1;

SET optimizer_switch = default;

--echo Specifying two SEMIJOIN/NO_SEMIJOIN for same query block gives warning
--echo First has effect, second is ignored
--replace_regex $elide_costs
EXPLAIN
SELECT * FROM t2
WHERE t2.a IN (SELECT /*+ NO_SEMIJOIN() SEMIJOIN() */ a FROM t1);
--echo Try opposite order
--replace_regex $elide_costs
EXPLAIN
SELECT * FROM t2
WHERE t2.a IN (SELECT /*+ SEMIJOIN() NO_SEMIJOIN() */ a FROM t1);
--echo Specify at different levels, hint inside block has effect
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ NO_SEMIJOIN(@subq) */ * FROM t2
WHERE t2.a IN (SELECT /*+ QB_NAME(subq) SEMIJOIN() */ a FROM t1);
--echo Specify at different levels, opposite order
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ SEMIJOIN(@subq) */ * FROM t2
WHERE t2.a IN (SELECT /*+ QB_NAME(subq) NO_SEMIJOIN() */ a FROM t1);
--echo Duplicate hints also gives warning, but hint has effect
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ SEMIJOIN(@subq) */ * FROM t2
WHERE t2.a IN (SELECT /*+ QB_NAME(subq) SEMIJOIN() */ a FROM t1);
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ NO_SEMIJOIN(@subq) */ * FROM t2
WHERE t2.a IN (SELECT /*+ QB_NAME(subq) NO_SEMIJOIN() */ a FROM t1);
--echo Multiple subqueries with conflicting hints
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ SEMIJOIN(@subq1 LOOSESCAN) SEMIJOIN(@subq2 FIRSTMATCH) */ * FROM t1
WHERE t1.a IN (SELECT /*+ QB_NAME(subq1) NO_SEMIJOIN() */ a FROM t3)
  AND t1.b IN (SELECT /*+ QB_NAME(subq2) SEMIJOIN(LOOSESCAN) */ a FROM t2);
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ SEMIJOIN(@subq1 LOOSESCAN) SEMIJOIN(@subq2 FIRSTMATCH) */ * FROM t1
WHERE t1.a IN (SELECT /*+ QB_NAME(subq1) NO_SEMIJOIN(LOOSESCAN) */ a FROM t3)
  AND t1.b IN (SELECT /*+ QB_NAME(subq2) SEMIJOIN(LOOSESCAN) */ a FROM t2);
--echo Conflicting hints in same hint comment
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ SEMIJOIN(@subq1 LOOSESCAN) SEMIJOIN(@subq1 FIRSTMATCH) */ * FROM t1
WHERE t1.a IN (SELECT /*+ QB_NAME(subq1) */ a FROM t3)
  AND t1.b IN (SELECT /*+ QB_NAME(subq2) */ a FROM t2);
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ SEMIJOIN(@subq1 LOOSESCAN) NO_SEMIJOIN(@subq1 LOOSESCAN) */ *
FROM t1
WHERE t1.a IN (SELECT /*+ QB_NAME(subq1) */ a FROM t3)
  AND t1.b IN (SELECT /*+ QB_NAME(subq2) */ a FROM t2);
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ NO_SEMIJOIN(@subq1 LOOSESCAN) NO_SEMIJOIN(@subq1 FIRSTMATCH) */ *
FROM t1
WHERE t1.a IN (SELECT /*+ QB_NAME(subq1) */ a FROM t3)
  AND t1.b IN (SELECT /*+ QB_NAME(subq2) */ a FROM t2);

--echo Non-supported strategies should give warnings
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ SEMIJOIN(@subq1 INTOEXISTS) NO_SEMIJOIN(@subq2 INTOEXISTS) */ *
FROM t1
WHERE t1.a IN (SELECT /*+ QB_NAME(subq1) */ a FROM t3)
  AND t1.b IN (SELECT /*+ QB_NAME(subq2) */ a FROM t2);

--echo SUBQUERY tests
--echo SUBQUERY should disable SEMIJOIN and use specified subquery strategy
--replace_regex $elide_costs
EXPLAIN
SELECT * FROM t2 WHERE t2.a IN (SELECT /*+ SUBQUERY(INTOEXISTS) */ a FROM t1);
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ SUBQUERY(@subq MATERIALIZATION) */ * FROM t2
WHERE t2.a IN (SELECT /*+ QB_NAME(subq) */ a FROM t1);
--echo Query with two subqueries
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ SUBQUERY(@subq1 INTOEXISTS) SUBQUERY(@subq2 MATERIALIZATION) */ *
FROM t3
WHERE t3.a IN (SELECT /*+ QB_NAME(subq1) */ a FROM t1 tx)
  AND t3.b IN (SELECT /*+ QB_NAME(subq2) */ a FROM t1 ty);
--echo Query with nested sub-queries
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ SUBQUERY(@subq1 INTOEXISTS) SUBQUERY(@subq2 MATERIALIZATION) */ *
FROM t3
WHERE t3.a IN (SELECT /*+ QB_NAME(subq1) */ a FROM t1 tx
               WHERE tx.b IN (SELECT /*+ QB_NAME(subq2) */ a FROM t1 ty));
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ SUBQUERY(@subq1 MATERIALIZATION) SUBQUERY(@subq2 INTOEXISTS) */ *
FROM t3
WHERE t3.a IN (SELECT /*+ QB_NAME(subq1) */ a FROM t1 tx
               WHERE tx.b IN (SELECT /*+ QB_NAME(subq2) */ a FROM t1 ty));

--echo This query does not support SEMIJOIN.  Materialization is default
--replace_regex $elide_costs
EXPLAIN
SELECT * FROM t2
WHERE t2.a IN (SELECT /*+ QB_NAME(subq) */ min(a) FROM t1 group by a);
--echo Use In-to-exists instead
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ SUBQUERY(@subq INTOEXISTS) */ * FROM t2
WHERE t2.a IN (SELECT /*+ QB_NAME(subq) */ min(a) FROM t1 group by a);

--echo For this query In-to-exists is default
--replace_regex $elide_costs
EXPLAIN
SELECT a, a IN (SELECT a FROM t1) FROM t2;
--echo Force Subquery Materialization
--echo Hypergraph does not support materializing subqueries in projection
--replace_regex $elide_costs
EXPLAIN
SELECT a, a IN (SELECT /*+ SUBQUERY(MATERIALIZATION) */ a FROM t1) FROM t2;
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ SUBQUERY(@subq MATERIALIZATION) */ a,
       a IN (SELECT /*+ QB_NAME(subq) */ a FROM t1) FROM t2;

--echo This query does not support Subquery Materialization due to type mismatch
--replace_regex $elide_costs
EXPLAIN
SELECT * FROM t2
WHERE t2.a IN (SELECT /*+ QB_NAME(subq) */ concat(sum(b),"") FROM t1 group by a);
--echo Trying to force Subquery Materialization will not change anything
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ SUBQUERY(@subq MATERIALIZATION) */ * FROM t2
WHERE t2.a IN (SELECT /*+ QB_NAME(subq) */ concat(sum(b),"") FROM t1 group by a);

--echo Test hints with prepared statements
PREPARE stmt1 FROM "EXPLAIN
SELECT /*+ SUBQUERY(@subq1 MATERIALIZATION)
           SUBQUERY(@subq2 INTOEXISTS) */ * FROM t1
WHERE t1.a IN (SELECT /*+ QB_NAME(subq1) */ a FROM t3)
  AND t1.b IN (SELECT /*+ QB_NAME(subq2) */ a FROM t2)";
--replace_regex $elide_costs
EXECUTE stmt1;
--replace_regex $elide_costs
EXECUTE stmt1;
DEALLOCATE PREPARE stmt1;

--echo Test optimizer_switch settings with SUBQUERY hint
SET optimizer_switch='materialization=off';
--echo This query will now use In-to-exist
--replace_regex $elide_costs
EXPLAIN
SELECT * FROM t2
WHERE t2.a IN (SELECT /*+ QB_NAME(subq) */ min(a) FROM t1 group by a);
--echo Force it to use Materialization
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ SUBQUERY(@subq MATERIALIZATION) */ * FROM t2
WHERE t2.a IN (SELECT /*+ QB_NAME(subq) */ min(a) FROM t1 group by a);

SET optimizer_switch='materialization=on,subquery_materialization_cost_based=off';
--echo This query will now use materialization
--replace_regex $elide_costs
EXPLAIN
SELECT a, a IN (SELECT a FROM t1) FROM t2;
--echo Force In-to-exists
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ SUBQUERY(@subq INTOEXISTS) */ a,
       a IN (SELECT /*+ QB_NAME(subq) */ a FROM t1) FROM t2;

--echo Specifying both strategies should give a warning
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ SUBQUERY(@subq1 MATERIALIZATION, INTOEXISTS)
       SUBQUERY(@subq2 MATERIALIZATION, INTOEXISTS) */ *
FROM t3
WHERE t3.a IN (SELECT /*+ QB_NAME(subq1) */ a FROM t1 tx)
  AND t3.b IN (SELECT /*+ QB_NAME(subq2) */ a FROM t1 ty);
--echo Non-supported strategies should give warnings
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ SUBQUERY(@subq1 FIRSTMATCH) SUBQUERY(@subq2 LOOSESCAN) */ *
FROM t3
WHERE t3.a IN (SELECT /*+ QB_NAME(subq1) */ a FROM t1 tx)
  AND t3.b IN (SELECT /*+ QB_NAME(subq2) */ a FROM t1 ty);

SET optimizer_switch= default;

--echo Specifying two SUBQUERY for same query block gives warning
--echo First has effect, second is ignored
--replace_regex $elide_costs
EXPLAIN
SELECT * FROM t2
WHERE t2.a IN (SELECT /*+ SUBQUERY(MATERIALIZATION) SUBQUERY(INTOEXISTS) */ a
FROM t1);
--echo Try opposite order
--replace_regex $elide_costs
EXPLAIN
SELECT * FROM t2
WHERE t2.a IN (SELECT /*+ SUBQUERY(INTOEXISTS) SUBQUERY(MATERIALIZATION) */ a
FROM t1);
--echo Specify at different levels, hint inside block has effect
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ SUBQUERY(@subq MATERIALIZATION) */ * FROM t2
WHERE t2.a IN (SELECT /*+ QB_NAME(subq) SUBQUERY(INTOEXISTS) */ a FROM t1);
--echo Specify at different levels, opposite order
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ SUBQUERY(@subq INTOEXISTS) */ * FROM t2
WHERE t2.a IN (SELECT /*+ QB_NAME(subq) SUBQUERY(MATERIALIZATION) */ a FROM t1);

--echo Specifying combinations of SUBQUERY and SEMIJOIN/NO_SEMIJOIN
--echo for same query block gives warning
--echo First has effect, second is ignored
--replace_regex $elide_costs
EXPLAIN
SELECT * FROM t2
WHERE t2.a IN (SELECT /*+ SUBQUERY(INTOEXISTS) SEMIJOIN() */ a FROM t1);
--echo Try opposite order
--replace_regex $elide_costs
EXPLAIN
SELECT * FROM t2
WHERE t2.a IN (SELECT /*+ NO_SEMIJOIN() SUBQUERY(MATERIALIZATION) */ a FROM t1);
--echo Specify at different levels, hint inside block has effect
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ SUBQUERY(@subq MATERIALIZATION) */ * FROM t2
WHERE t2.a IN (SELECT /*+ QB_NAME(subq) SEMIJOIN() */ a FROM t1);
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ SUBQUERY(@subq INTOEXISTS) */ * FROM t2
WHERE t2.a IN (SELECT /*+ QB_NAME(subq) NO_SEMIJOIN() */ a FROM t1);
--replace_regex $elide_costs
EXPLAIN
SELECT /*+ SEMIJOIN(@subq FIRSTMATCH) */ * FROM t2
WHERE t2.a IN (SELECT /*+ QB_NAME(subq) SUBQUERY(@subq INTOEXISTS) */ a FROM t1);

drop table t1, t2, t3;

--echo #
--echo # Bug#37008930: Assertion `strategy ==
--echo #               Subquery_strategy::CANDIDATE_FOR_IN2EXISTS_OR_MAT' failed.
--echo #

CREATE TABLE t1 (f1 INTEGER, f2 INTEGER);
let $query = SELECT 1 FROM t1
WHERE f1 IN (SELECT MIN(f2) FROM (t1 AS t2)
             WHERE NOT EXISTS (SELECT ROW_NUMBER() OVER () FROM t1 AS t3
                                WHERE 1 = t2.f2));
--replace_regex $elide_costs
eval EXPLAIN $query;
eval $query;
DROP TABLE t1;
