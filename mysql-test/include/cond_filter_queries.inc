--echo
--echo ### Test that optimizer_condition_fanout_filter works ###
set optimizer_switch='condition_fanout_filter=off';
--echo
EXPLAIN SELECT * FROM t1 WHERE col3=5;
EXPLAIN SELECT * FROM t1 WHERE col1_idx>1;
EXPLAIN SELECT * FROM t1 JOIN t2 WHERE t1.col1_idx=t2.col1_idx;

set optimizer_switch='condition_fanout_filter=on';
--echo
EXPLAIN SELECT * FROM t1 WHERE col3=5;
--echo #########################################################

--echo 
--echo # Testing non-indexed Operators

--echo
--echo # Simple operands
EXPLAIN SELECT * FROM t1 WHERE t1.col3=5;
EXPLAIN SELECT * FROM t1 WHERE t1.col3<=>5;
EXPLAIN SELECT * FROM t1 WHERE t1.col3>5;
EXPLAIN SELECT * FROM t1 WHERE t1.col3>=5;
EXPLAIN SELECT * FROM t1 WHERE t1.col3<5;
EXPLAIN SELECT * FROM t1 WHERE t1.col3<=5;

--echo
--echo # NOT <simple operands>
EXPLAIN SELECT * FROM t1 WHERE t1.col3!=5;
EXPLAIN SELECT * FROM t1 WHERE NOT t1.col3<=5;

--echo
--echo # BETWEEN
EXPLAIN SELECT * FROM t1 WHERE t1.col3 BETWEEN 5 AND 10;

--echo
--echo # NOT BETWEEN
EXPLAIN SELECT * FROM t1 WHERE t1.col3 NOT BETWEEN 5 AND 10;

--echo
--echo # <column> IN
EXPLAIN SELECT * FROM t1 WHERE t1.col3 IN (5, 6);
EXPLAIN SELECT * FROM t1 WHERE t1.col3 IN (5, 6, 7, 8);
--echo # Dependent subquery in IN list. No filtering in t1 
EXPLAIN SELECT * FROM t1 
WHERE t1.col3 IN (1, (SELECT col3 FROM t2 where col3=t1.col4 LIMIT 1));

--echo
--echo # <column> NOT IN
EXPLAIN SELECT * FROM t1 WHERE t1.col3 NOT IN (5, 6);
EXPLAIN SELECT * FROM t1 WHERE t1.col3 NOT IN (5, 6, 7, 8);

--echo
--echo # <row_value> IN
EXPLAIN SELECT * FROM t1 WHERE (t1.col3, t1.col4) IN ((5,5),(6,6),(7,7));
EXPLAIN 
  SELECT * FROM t1 WHERE (t1.col3, t1.col4) IN ((5,5),(6,6),(7,7),(8,8),(9,9),(10,10));
EXPLAIN SELECT * FROM t1 WHERE (t1.col3, 1) IN ((5,5),(6,6),(7,7));

--echo
--echo # <row_value> NOT IN
EXPLAIN SELECT * FROM t1 WHERE (t1.col3, t1.col4) NOT IN ((5,5),(6,6),(7,7));
EXPLAIN SELECT * FROM t1 WHERE (t1.col3, 1) NOT IN ((5,5),(6,6),(7,7));

--echo
--echo # NULL
EXPLAIN SELECT * FROM t1 WHERE t1.col3 IS NULL;
EXPLAIN SELECT * FROM t1 WHERE t1.col3 = NULL;

EXPLAIN SELECT * FROM t1 WHERE t1.col3>NULL;
EXPLAIN SELECT * FROM t1 WHERE t1.col3>=NULL;
EXPLAIN SELECT * FROM t1 WHERE t1.col3<NULL;
EXPLAIN SELECT * FROM t1 WHERE t1.col3<=NULL;

EXPLAIN SELECT * FROM t1 WHERE NULL>t1.col3;
EXPLAIN SELECT * FROM t1 WHERE NULL>=t1.col3;
EXPLAIN SELECT * FROM t1 WHERE NULL<t1.col3;
EXPLAIN SELECT * FROM t1 WHERE NULL<=t1.col3;

--echo
--echo # NOT NULL
EXPLAIN SELECT * FROM t1 WHERE t1.col3 IS NOT NULL;
EXPLAIN SELECT * FROM t1 WHERE t1.col3 != NULL;

--echo
--echo # NULL (not nullable column)
EXPLAIN SELECT * FROM t1 WHERE t1.col4 IS NULL;
EXPLAIN SELECT * FROM t1 WHERE t1.col4 = NULL;

EXPLAIN SELECT * FROM t1 WHERE t1.col4>NULL;
EXPLAIN SELECT * FROM t1 WHERE t1.col4>=NULL;
EXPLAIN SELECT * FROM t1 WHERE t1.col4<NULL;
EXPLAIN SELECT * FROM t1 WHERE t1.col4<=NULL;

EXPLAIN SELECT * FROM t1 WHERE NULL>t1.col4;
EXPLAIN SELECT * FROM t1 WHERE NULL>=t1.col4;
EXPLAIN SELECT * FROM t1 WHERE NULL<t1.col4;
EXPLAIN SELECT * FROM t1 WHERE NULL<=t1.col4;

--echo
--echo # NOT NULL (not nullable column)
EXPLAIN SELECT * FROM t1 WHERE t1.col4 IS NOT NULL;
EXPLAIN SELECT * FROM t1 WHERE t1.col4 != NULL;

--echo
--echo # LIKE
EXPLAIN SELECT * FROM t1 WHERE t1.vc LIKE 'america';
EXPLAIN SELECT * FROM t1 WHERE t1.vc LIKE '%america';
EXPLAIN SELECT * FROM t1 WHERE t1.vc LIKE 'amer%';

EXPLAIN SELECT * FROM t1 WHERE t1.vc NOT LIKE 'america';
EXPLAIN SELECT * FROM t1 WHERE t1.vc NOT LIKE '%america';
EXPLAIN SELECT * FROM t1 WHERE t1.vc NOT LIKE 'amer%';

--echo
--echo # FULLTEXT
EXPLAIN SELECT * FROM t1 WHERE MATCH(t1.vc) AGAINST ('+norway' IN BOOLEAN MODE);
EXPLAIN SELECT * FROM t1 WHERE MATCH(t1.vc_ft) AGAINST ('+norway');
EXPLAIN SELECT * FROM t1 WHERE MATCH(t1.vc_ft) AGAINST ('norway');

EXPLAIN SELECT * FROM t1 WHERE NOT MATCH(t1.vc) AGAINST ('+norway' IN BOOLEAN MODE);
EXPLAIN SELECT * FROM t1 WHERE NOT MATCH(t1.vc_ft) AGAINST ('+norway');
EXPLAIN SELECT * FROM t1 WHERE NOT MATCH(t1.vc_ft) AGAINST ('norway');

--echo
--echo # Functions
EXPLAIN SELECT * FROM t1 WHERE length(t1.vc) > 3;
EXPLAIN SELECT * FROM t1 WHERE length(t1.vc) = 3;
EXPLAIN SELECT * FROM t1 WHERE length(t1.vc) IS NULL;
EXPLAIN SELECT * FROM t1 WHERE length(t1.vc) IS NOT NULL;

--echo 
--echo # AND/OR/XOR
EXPLAIN SELECT * FROM t1 WHERE t1.col3<5;
EXPLAIN SELECT * FROM t1 WHERE t1.col4<5;
EXPLAIN SELECT * FROM t1 WHERE t1.col3<5 OR col4<5;
EXPLAIN SELECT * FROM t1 WHERE t1.col3<5 AND col4<5;
EXPLAIN SELECT * FROM t1 WHERE t1.col3<5 XOR col4<5;
EXPLAIN SELECT * FROM t1 WHERE t1.col3<5 XOR col4<5 XOR col1_idx>3;
EXPLAIN SELECT * FROM t1 WHERE t1.col3<5 XOR (col4<5 AND col4>1);

--echo # Done OP non-indexed tests

--echo
--echo ### START SUBQUERY LOOP ###

let $iterations=2;
while ($iterations)
{
  # Should there be a filter on semijoin nests? No - #rows=1 is enough

  --echo
  --echo #  table scan in subq, no filter
  EXPLAIN SELECT * FROM t1 WHERE t1.col3 IN (SELECT col3 FROM t2);
  --echo
  --echo # table scan in subq, filter from range access
  EXPLAIN SELECT * FROM t1 WHERE t1.col3 IN (SELECT col3 FROM t2 where col1_idx>2);
  --echo
  --echo # table scan subquery, filter SEL(=)
  EXPLAIN SELECT * FROM t1 WHERE t1.col3 IN (SELECT col3 FROM t2 where col3=3);
  --echo
  --echo # range in subquery, no filter
  EXPLAIN SELECT * FROM t1 WHERE EXISTS (SELECT col3 FROM t2 where col1_idx>2);
  --echo
  --echo # table scan subquery, filter SEL(=)
  EXPLAIN SELECT * FROM t1 WHERE EXISTS (SELECT col3 FROM t2 where col3=3);
  --echo
  --echo # range in subquery, filter SEL(=)
  EXPLAIN SELECT * FROM t1 WHERE EXISTS (SELECT col3 FROM t2 where col1_idx>2 and col3=3);
  --echo
  --echo # EXISTS - outer reference
  --echo # dynamic range subq, filter SEL(>)
  EXPLAIN SELECT * FROM t1 WHERE EXISTS (SELECT 1 FROM t2 where col1_idx>t1.col3);

  --echo
  --echo #
  EXPLAIN SELECT * FROM t1 WHERE col3 =   (SELECT 1 FROM t2);
  EXPLAIN SELECT * FROM t1 WHERE col3 <=> (SELECT 1 FROM t2);
  EXPLAIN SELECT * FROM t1 WHERE col3 >   (SELECT 1 FROM t2);
  EXPLAIN SELECT * FROM t1 WHERE col3 >=  (SELECT 1 FROM t2);
  EXPLAIN SELECT * FROM t1 WHERE col3 <   (SELECT 1 FROM t2);
  EXPLAIN SELECT * FROM t1 WHERE col3 <=  (SELECT 1 FROM t2);
  EXPLAIN SELECT * FROM t1 WHERE col3 between (SELECT 1 FROM t2) and 2;

  EXPLAIN SELECT * FROM t1 WHERE col1_idx =   (SELECT 1 FROM t2 LIMIT 1);
  EXPLAIN SELECT * FROM t1 WHERE col1_idx <=> (SELECT 1 FROM t2 LIMIT 1);
  EXPLAIN SELECT * FROM t1 WHERE col1_idx >   (SELECT 1 FROM t2 LIMIT 1);
  EXPLAIN SELECT * FROM t1 WHERE col1_idx >=  (SELECT 1 FROM t2 LIMIT 1);
  EXPLAIN SELECT * FROM t1 WHERE col1_idx <   (SELECT 1 FROM t2 LIMIT 1);
  EXPLAIN SELECT * FROM t1 WHERE col1_idx <=  (SELECT 1 FROM t2 LIMIT 1);
  EXPLAIN SELECT * FROM t1 WHERE col1_idx between (SELECT 1 FROM t2 LIMIT 1) and 2;

  --echo
  set optimizer_switch='semijoin=off';
  --echo

  dec $iterations;
}

set optimizer_switch=default;

--echo ################

--echo
EXPLAIN 
SELECT * 
FROM t1 AS t1a JOIN t1 AS t1b ON t1a.col1_idx=t1b.col1_idx 
WHERE t1b.col3=5;

--echo
EXPLAIN 
SELECT * 
FROM t1 JOIN t2 ON t1.col1_idx=t2.col1_idx
WHERE t2.col3=5;

--echo
EXPLAIN 
SELECT *
FROM t1 JOIN t2 ON t1.col1_idx=t2.col1_idx
WHERE t1.col3=t2.col3;

--echo
EXPLAIN 
SELECT *
FROM t1 JOIN t2 ON t1.col1_idx=t2.col1_idx
WHERE t1.col2_idx=1 AND t2.col2_idx=1;

--echo
EXPLAIN 
SELECT * 
FROM t1 JOIN t2 ON t1.col1_idx=t2.col1_idx
WHERE t1.col2_idx>1 AND t2.col2_idx>1;

--echo
# t2(ALL)-t1(DR)
# t1 filter = Conjunction of DEFAULT (Join condition) and range estimate
EXPLAIN 
SELECT * 
FROM t1 JOIN t2 ON t1.col1_idx>t2.col1_idx
WHERE t1.col2_idx>1 AND t2.col2_idx>1;

--echo
# t2(ALL)-t1(ALL)
# t1 filter = range estimate (Join condition is ignored)
EXPLAIN 
SELECT * 
FROM t1 JOIN t2 ON t1.col1_idx>t2.col1_idx
WHERE t1.col1_idx>1 AND t2.col1_idx>1;

--echo
#
#
EXPLAIN 
SELECT * 
FROM t1 JOIN t2 ON t1.col1_idx>t2.col1_idx
WHERE t1.col1_idx!=1 AND t2.col1_idx>4;

--echo
EXPLAIN 
SELECT * FROM t1 JOIN t2 ON t1.col1_idx>t2.col1_idx;

--echo
EXPLAIN 
SELECT * FROM t1 WHERE col1_idx > 500 AND col2_idx > 500;

--echo
EXPLAIN SELECT * FROM t1 WHERE col1_idx > 120 AND col2_idx > 120;

--echo
# filter effect from indexed column IN
EXPLAIN 
SELECT * FROM t1 WHERE col1_idx IN (120,121,122) AND col2_idx IN (120,121,122);

--echo # TODO: after wl7019
--echo # EXPLAIN 
--echo # SELECT * FROM t1 WHERE col1_idx IN (120,121,122);
--echo # EXPLAIN 
--echo # SELECT * FROM t1 WHERE (col1_idx,c) IN ((120,1),(121,2),(122,3));

## working on
# filter effect from indexed column NOT IN
--echo
--echo range on col1_idx and filtering estimate from the range optimizer on col2_idx
--echo   -> no filtering effect for filter_single_col_small
--echo   -> very small filtering effect for filter_single_col_big
EXPLAIN 
SELECT * FROM t1 WHERE col1_idx IN (120,121,122) AND col2_idx NOT IN (120,121,122);

# filter effect from unindexed column IN
--echo
EXPLAIN 
SELECT * FROM t1 WHERE col1_idx IN (120,121,122) AND col3 IN (120,121,122);

# filter effect from unindexed column NOT IN
--echo
EXPLAIN 
SELECT * FROM t1 WHERE col1_idx IN (120,121,122) AND col3 NOT IN (120,121,122);


--echo
EXPLAIN 
SELECT * FROM t1 JOIN t2 ON t1.col3=t2.col1_idx WHERE t2.col1_idx>20;

--echo
EXPLAIN 
SELECT * FROM t1 JOIN t2 ON t1.col3=t2.col1_idx WHERE t2.col1_idx=20;
