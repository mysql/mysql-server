# Checks that the DISTANCE() values stored in t1 (see
# t/distance_tier_portability.test) match the values computed now.
#
# R1: stored generated columns equal the recomputed values.
SELECT COUNT(*) AS stored_mismatches FROM t1
 WHERE NOT (d_euclidean <=> DISTANCE(v, w, 'EUCLIDEAN'))
    OR NOT (d_squared <=> DISTANCE(v, w, 'EUCLIDEAN_SQUARED'));

# R2: looking each row up in an index with the value computed now finds it.
# The scalar subqueries keep the outer value a constant for the lookup.
--let $i = 4
while ($i)
{
  if ($i == 4) {
    --let $index = i_euclidean
    --let $expr = d_euclidean
    --let $value = DISTANCE(a.v, a.w, 'EUCLIDEAN')
  }
  if ($i == 3) {
    --let $index = i_cosine
    --let $expr = d_cosine
    --let $value = DISTANCE(a.v, a.w, 'COSINE')
  }
  if ($i == 2) {
    --let $index = i_dot
    --let $expr = DISTANCE(b.v, b.w, 'DOT')
    --let $value = DISTANCE(a.v, a.w, 'DOT')
  }
  if ($i == 1) {
    --let $index = i_manhattan
    --let $expr = DISTANCE(b.v, b.w, 'MANHATTAN')
    --let $value = DISTANCE(a.v, a.w, 'MANHATTAN')
  }
  --let $query = SELECT COUNT(*) FROM t1 AS a WHERE (SELECT COUNT(*) FROM t1 AS b FORCE INDEX ($index) WHERE $expr = $value AND b.id = a.id) <> 1
  # The subquery must look the value up in the index.
  --disable_query_log
  --disable_warnings
  --eval EXPLAIN FORMAT=JSON INTO @plan $query
  --enable_warnings
  --enable_query_log
  --let $lookup = `SELECT JSON_SEARCH(@plan, 'one', '%index lookup on b using $index (%') IS NOT NULL`
  if (!$lookup) {
    --eval EXPLAIN FORMAT=TREE $query
    --die The subquery does not look the value up in the index
  }
  --echo # Rows not found through $index:
  --eval $query
  --dec $i
}
