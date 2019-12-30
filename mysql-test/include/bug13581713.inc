--source include/turn_off_only_full_group_by.inc

insert into t1 values(100,1,2),(200,1,1),(300,2,1),(400,2,2);
analyze table t1;

eval select distinct b from $source order by c;
--sorted_result
eval select distinct min(b) from $source group by a order by min(c);
# just to see that if source is a view, it is merged
--sorted_result
--replace_column 9 #
eval explain select distinct min(b) from $source group by a order by min(c);

--echo Insert rows in different order:
delete from t1;
insert into t1 values(200,1,1),(100,1,2),(400,2,2),(300,2,1);
analyze table t1;

--echo And get a different order. Query is executed like this:
--echo - First, DISTINCT, using a tmp MEMORY table with a unique
--echo index, thus if two rows have the same 'b' but a different 'c',
--echo the second row is rejected, so the first value of 'c' wins
--echo (=> randomness of 'c')
--echo - Second, ORDER BY on the random 'c'.
eval select distinct b from $source order by c;

--echo Random order too (same reason):
--sorted_result
eval select distinct min(b) from $source group by a order by min(c);

--source include/restore_sql_mode_after_turn_off_only_full_group_by.inc

--echo This query gives random order:
--error ER_FIELD_IN_ORDER_NOT_SELECT
eval select distinct b from $source order by c;
--echo and this one too:
--error ER_FIELD_IN_ORDER_NOT_SELECT
eval select distinct b from $source order by b-1,b+1,c;

--echo and this one too:
--sorted_result
--error ER_AGGREGATE_IN_ORDER_NOT_SELECT
eval select distinct min(b) from $source group by a order by min(c);

--echo Not random (though Standard bans aggregates from ORDER BY):
eval select distinct min(b) from $source group by a order by min(b);
--error ER_AGGREGATE_IN_ORDER_NOT_SELECT
eval select distinct min(b) from $source group by a order by -min(b);
--echo All group exprs are in select list => DISTINCT is removed => no error
eval select distinct a, min(b) from $source group by a order by max(b-2)-min(c*5);

--echo This one is standard:
eval select distinct min(b) as z from $source group by a order by z;

--echo Other queries:
eval select distinct b from $source where b<0 order by rand();
eval select distinct b from $source order by 45.0+3;
eval select (select distinct b from $source_no_alias as S2 where b=7 order by S3.a) from $source_no_alias as S3;
eval select distinct b from $source order by abs(b);
eval select distinct b as z from $source order by abs(z);

--error ER_FIELD_IN_ORDER_NOT_SELECT
eval select distinct b from $source order by abs(b+a);

eval select distinct abs(b) as z from $source order by z;
eval select distinct abs(b) as z from $source order by abs(b);
eval select distinct abs(b) from $source order by abs(b);

--echo Not ok: ABS(b)+1 is neither a SELECTed expression nor an alias
--echo to one, and mentions a column of FROM tables.
--error ER_FIELD_IN_ORDER_NOT_SELECT
eval select distinct abs(b) as z from $source order by abs(b)+1;
eval select distinct abs(b) as z from $source order by z+1;
--error ER_FIELD_IN_ORDER_NOT_SELECT
eval select distinct abs(b) from $source order by abs(b)+1;
--error ER_FIELD_IN_ORDER_NOT_SELECT
eval select distinct abs(b) as z from $source order by floor(10*b);
--error ER_FIELD_IN_ORDER_NOT_SELECT
eval select distinct abs(b) from $source order by floor(10*b);
--echo Two offending columns; error message needs to report only one
--error ER_FIELD_IN_ORDER_NOT_SELECT
eval select distinct abs(b) from $source order by floor(10*b),floor(10*a);
--error ER_FIELD_IN_ORDER_NOT_SELECT
eval select distinct abs(b) from $source_no_alias as S2 order by
 (select floor(10*S2.b) from $source_no_alias as S3 limit 1);
--echo Ok as S2.b in SELECT list
eval select distinct abs(b),b from $source_no_alias as S2 order by
 (select floor(10*S2.b) from $source_no_alias as S3 limit 1);
--echo Ok as subq does not use columns of FROM clause of ordered Q.
eval select distinct abs(b) from $source_no_alias as S2 order by
 (select floor(10*S3.b) from $source_no_alias as S3 limit 1);
--echo Subq as alias => ok
eval select distinct abs(b),
 (select floor(10*S3.b) from $source_no_alias as S3 limit 1) as subq
 from $source_no_alias as S2 order by subq;
--echo Bad field in left or right argument of ALL/ANY(subq):
--error ER_FIELD_IN_ORDER_NOT_SELECT
eval select distinct abs(b) from $source_no_alias as S2 order by
 floor(10*S2.b) IN (select floor(10*S3.b) from $source_no_alias as S3);
--error ER_FIELD_IN_ORDER_NOT_SELECT
eval select distinct abs(b) from $source_no_alias as S2 order by
 floor(10*S2.b) > ALL(select floor(10*S3.b) from $source_no_alias as S3);
--error ER_FIELD_IN_ORDER_NOT_SELECT
eval select distinct abs(b) from $source_no_alias as S2 order by
 floor(10*10) IN (select floor(10*S2.b) from $source_no_alias as S3);
--error ER_FIELD_IN_ORDER_NOT_SELECT
eval select distinct abs(b) from $source_no_alias as S2 order by
 floor(10*10) > ALL(select floor(10*S2.b) from $source_no_alias as S3);

--echo Aggregates:
--error ER_AGGREGATE_IN_ORDER_NOT_SELECT
SELECT distinct 1 FROM t1 group by a order by count(*);
--error ER_AGGREGATE_IN_ORDER_NOT_SELECT
SELECT distinct 1 FROM t1 group by a order by count(*)-count(*);
--echo Test ANY_VALUE
SELECT distinct 1 FROM t1 group by a order by any_value(count(*)-count(b));
SELECT distinct 1 FROM t1 group by a order by any_value(count(*))-any_value(count(b));
--echo All group exprs are in select list => DISTINCT is removed => no error
--sorted_result
SELECT distinct a, min(b) FROM t1 group by a order by count(*)-count(*);
--error ER_AGGREGATE_IN_ORDER_NOT_SELECT
SELECT distinct 1 FROM t1 group by a order by count(*)-count(b);
--echo aggregation in outer Q => constant in inner Q
select * from t1 as t2 where t2.a in
  (SELECT distinct 1 FROM t1 group by a order by count(t2.a)-max(t2.a));
--echo ORDER BY expressions are in SELECT list => ok
SELECT distinct 1, count(*)-count(b) FROM t1 group by a order by count(*)-count(b);
--echo Without GROUP BY, aggregates yield a single row, no random order
SELECT distinct sum(a) FROM t1 order by count(*)-count(*);
SELECT distinct sum(a) FROM t1 order by count(*)-count(b);

--echo Verify that DISTINCT is optimized away even if the aggregate
--echo function is hidden in a subquery
EXPLAIN SELECT DISTINCT MAX(b) FROM t1;
EXPLAIN SELECT DISTINCT (SELECT MAX(t1.b) FROM t1 AS t2 LIMIT 1) FROM t1;
--echo but if the subquery is the aggregation query, DISTINCT must stay:
EXPLAIN SELECT DISTINCT (SELECT MAX(t1.b+0*t2.a) FROM t1 AS t2 LIMIT 1) FROM t1;
--echo QA's query is properly rejected:
--error ER_FIELD_IN_ORDER_NOT_SELECT
eval SELECT DISTINCT GP1.a AS g1 FROM $source_no_alias AS GP1
WHERE GP1.a >= 0
ORDER BY GP1.b LIMIT 8;

--echo result order does change depending on chosen plan.
--sorted_result
eval SELECT DISTINCT GP1.a AS g1 FROM $source_no_alias AS GP1
WHERE GP1.a >= 0
ORDER BY 2+ANY_VALUE(GP1.b) LIMIT 8;

DELETE FROM t1;
