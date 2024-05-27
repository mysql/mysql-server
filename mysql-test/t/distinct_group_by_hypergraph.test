--source include/have_hypergraph.inc
--source include/elide_costs.inc

--echo #
--echo # WL#16122: Temp table DISTINCT in the hypergraph optimizer
--echo # Various combinations of DISTINCT and GROUP BY to test the "Temporary
--echo # table with deduplication" plan.
--echo #

let $query_suffix=
  SELECT SQL_SMALL_RESULT;

CREATE TABLE tb1(id int , id2 int, v varchar(200));
INSERT INTO tb1(id, id2) VALUES (7, 10), (8, 11), (9, 12), (10, 10), (11, 11),
  (12, 12), (1, 10), (2, 11), (3, 12), (4, 10), (5, 11), (6, 12),
  (7, 100), (8, 101), (9, 102), (10, 150), (11, 150), (12, 102), (1, 10),
  (3, 11), (7, 12), (9, 10), (5, 101), (1, 12);
UPDATE tb1 SET v = concat('abcd', id2%21);
ANALYZE TABLE tb1;

eval $query_suffix DISTINCT id2, id FROM tb1 ORDER BY id,id2;
eval $query_suffix id2, id FROM tb1 GROUP BY id2, id ORDER BY id,id2;

# Cases where "sort with duplicate removal" was used when "temp table
# deduplication" was not available.
--source include/turn_off_only_full_group_by.inc
eval $query_suffix DISTINCT id2, (id) FROM tb1 GROUP BY id2 HAVING id < 8 ORDER BY id; # No sort with duplicate removal
eval $query_suffix DISTINCT id2, any_value(id) i FROM tb1 GROUP BY i+2 ORDER BY id2, i; # Sort with duplicate removal above temp table
eval $query_suffix DISTINCT id2, any_value(id2) i FROM tb1 GROUP BY id2+2 ORDER BY id2, i; # Sort with duplicate removal above temp table
eval $query_suffix DISTINCT id2 FROM tb1 GROUP BY id2+2 ORDER BY id2; # Sort with duplicate removal
eval $query_suffix DISTINCT id2, id i FROM tb1 GROUP BY id2+3 HAVING id2 != i ORDER BY id2+2; # Sort + Sort-with-dup + Filter + Group(no-aggs)
--source include/restore_sql_mode_after_turn_off_only_full_group_by.inc

CREATE TABLE tb2(id int , id2 int, PRIMARY KEY (id));
INSERT INTO tb2 VALUES (1, 10), (2, 11), (3, 12), (4, 10), (5, 11), (6, 12), (7, 10), (8, 11), (9, 12), (10, 10), (11, 11), (12, 12);
ANALYZE TABLE tb2;

eval $query_suffix DISTINCT any_value(id2) i , (id) FROM tb2 HAVING i  > 10 ORDER BY i, id;

let $query=
  $query_suffix DISTINCT id, id2 FROM tb1 GROUP BY id2, id;
--replace_regex $elide_costs
eval EXPLAIN FORMAT=TREE $query;
eval $query;

eval $query_suffix DISTINCT id +  id2 FROM tb1 GROUP BY id2, id ORDER BY 1;

--source include/turn_off_only_full_group_by.inc
# Distinct with aggregate function, with some join fields not HAVING a temp
# table field in temp-table-aggregation
eval $query_suffix
  DISTINCT 2 - sum(id*2) s, id2 DIV (2 - sum(id*2)) DIV 2
    FROM tb1 GROUP BY id2  HAVING s < -17 ORDER BY (-sum(id-2));

# Window and temp table aggregation under deduplication plan.
eval $query_suffix
  DISTINCT sum(id*2) OVER(),  id FROM tb1 GROUP BY id2 ORDER BY (-sum(id-2));
--source include/restore_sql_mode_after_turn_off_only_full_group_by.inc

# Two deduplication subplans:
eval $query_suffix
  DISTINCT any_value(v) , id2 FROM tb1 GROUP BY id, id2 ORDER BY (select -id2);
# .... and a Window plan in between
--source include/turn_off_only_full_group_by.inc
eval $query_suffix
  DISTINCT sum(id*2) OVER(PARTITION BY id2), id FROM tb1 GROUP BY id2 ORDER BY (-(id-2));
--source include/restore_sql_mode_after_turn_off_only_full_group_by.inc

# Subquery in ORDER BY clause
eval $query_suffix any_value(id2) , any_value(id) i FROM tb1 t1 GROUP BY id2+id
  HAVING i  > 10   ORDER BY (select avg(id) FROM tb1 t2 WHERE i < t2.id);

# Subquery in HAVING clause
eval $query_suffix  any_value(id), id2 FROM tb1 t GROUP BY id2 having
  (select t.id2 FROM tb1 t2 WHERE t.id2 = t2.id2 limit 1) > 50 ORDER BY id2;
--source include/turn_off_only_full_group_by.inc
eval $query_suffix DISTINCT v , id2 FROM tb1 t GROUP BY id, id2
  HAVING (select t.id2 FROM tb1 t2 WHERE t.id2 = t2.id2 limit 1) > 50
  ORDER BY concat(v, -id2) DESC, id2 DESC;
--source include/restore_sql_mode_after_turn_off_only_full_group_by.inc
# Table join with a HAVING clause.
eval $query_suffix
 t1.id2 i9 , (t2.id) i FROM tb1 t1 INNER JOIN tb1 t2 ON t1.id2 = t2.id2
  GROUP BY t1.id2, i HAVING (select avg(id) FROM tb1 t3 WHERE i9 < t3.id)
  ORDER BY i9, i;

# Subquery in ORDER BY clause, with DISTINCT.
eval $query_suffix
  DISTINCT t1.id2, t1.id2 + 1 i,  (select abs(50-i))
    FROM tb1 t1 INNER JOIN tb1 t2 ON t1.id = t2.id GROUP BY t2.id, t1.id2
    ORDER BY (select DISTINCT abs(50-t_inner.id2) FROM tb1 t_inner
              WHERE t_inner.id2 = t1.id2);

# Subquery in both ORDER BY and HAVING, with and without DISTINCT.
let $query=
  t1.id2, t1.id2 + 1 i,  (select abs(50-i))
    FROM tb1 t1 INNER JOIN tb1 t2 ON t1.id = t2.id GROUP BY t2.id, t1.id2
    HAVING EXISTS (SELECT * FROM tb1 t_h WHERE i =  t_h.id2 + 1
                   AND v IN ('abcd16', 'abcd17', 'abcd18', 'abcd10'))
    ORDER BY (SELECT DISTINCT abs(50-t_inner.id2) FROM tb1 t_inner
              WHERE t_inner.id2 = t1.id2);
eval $query_suffix $query;
eval $query_suffix DISTINCT $query;

# DISTINCT <agggregate>(DISTINCT <column>)
eval $query_suffix DISTINCT (count(DISTINCT id)) FROM tb1 GROUP BY id2;
eval $query_suffix DISTINCT (2 + sum(DISTINCT id)) FROM tb1 GROUP BY id2;

# Combinations of DISTINCT, agg(DISTINCT ...) and ROLLUP
eval $query_suffix
  DISTINCT COUNT(DISTINCT id) FROM tb1 GROUP BY id2;
eval $query_suffix
  DISTINCT COUNT(DISTINCT id) FROM tb1 GROUP BY id2 WITH ROLLUP;
eval $query_suffix
  DISTINCT SUM(id), COUNT(DISTINCT id) FROM tb1 GROUP BY id2 ;
eval $query_suffix
  DISTINCT SUM(id), COUNT(DISTINCT id) FROM tb1 GROUP BY id2 WITH ROLLUP;

# Materialize with deduplication for both INTERSECT and GROUP BY.
eval $query_suffix
  DISTINCT id  FROM tb1 GROUP BY id2, id INTERSECT SELECT id2 FROM tb1;
eval $query_suffix
  id2 FROM tb1 INTERSECT  $query_suffix DISTINCT id  FROM tb1 GROUP BY id2, id;
eval $query_suffix
  id2 FROM tb1 INTERSECT  $query_suffix DISTINCT id2  FROM tb1 GROUP BY id2, id;
eval $query_suffix
  id, id2 FROM tb1 INTERSECT  $query_suffix DISTINCT id, id2  FROM tb1 GROUP BY id2, id;

# Force materialization even if input is already sorted.
--replace_regex $elide_costs
eval EXPLAIN FORMAT=TREE $query_suffix id2 FROM tb2 GROUP BY id;

# Combination of group_by/DISTINCT, with temp table plan supported in one of
# them, and not supported in the other.
let $query=
  DISTINCT avg(id), JSON_ARRAYAGG(id) FROM tb1 t1
    GROUP BY id2, id WITH ROLLUP ORDER BY 1, 2;
eval $query_suffix $query;
--replace_regex $elide_costs
eval EXPLAIN FORMAT=TREE $query_suffix $query;
# SQL_BIG_RESULT should override SQL_SMALL_RESULT
eval $query_suffix SQL_BIG_RESULT $query;
--replace_regex $elide_costs
eval EXPLAIN FORMAT=TREE $query_suffix SQL_BIG_RESULT $query;

--echo #
--echo # Bug#36625150 WL#16122: BIT values displayed differently with ORDER BY
--echo #

CREATE TABLE t1(b BIT(5));
INSERT INTO t1 values (3), (1), (1), (3);
# Verify bit data type does not change.
--enable_metadata
--replace_column 1 #
eval $query_suffix DISTINCT b FROM t1 GROUP BY b ORDER BY b;
--replace_column 1 #
eval $query_suffix DISTINCT b FROM t1 ORDER BY b;
--replace_column 1 #
eval $query_suffix b FROM t1 GROUP BY b ORDER BY b;
--disable_metadata
DROP TABLE t1;


#cleanup
--source include/restore_sql_mode_after_turn_off_only_full_group_by.inc
DROP TABLE tb1, tb2;

--source include/disable_hypergraph.inc
