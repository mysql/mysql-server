-- echo
-- echo /* A. Subqueries in the SELECT clause. */
explain
select a1, a1 in (select b1 from t2 where b1 > '0') from t1;
select a1, a1 in (select b1 from t2 where b1 > '0') from t1;
-- echo
explain
select a1, a2, (a1, a2) in (select b1, b2 from t2 where b1 > '0') from t1;
select a1, a2, (a1, a2) in (select b1, b2 from t2 where b1 > '0') from t1;
-- echo
explain
select a1, a2, (a1, a2) in (select b1, b2 from t2 where b1 > '0' and b1 < '9') from t1;
select a1, a2, (a1, a2) in (select b1, b2 from t2 where b1 > '0' and b1 < '9') from t1;

-- echo
-- echo /*
-- echo   B. "Natural" examples of subqueries without grouping that
-- echo   cannot be flattened into semijoin.
-- echo */

explain
select a1 from t1 where a1 in (select b2 from t2) or a2 < '9';
select a1 from t1 where a1 in (select b2 from t2) or a2 < '9';
-- echo
explain
select a1, a2 from t1 where (a1, a2) in (select b1, b2 from t2 where b1 > '0') or a2 < '9';
select a1, a2 from t1 where (a1, a2) in (select b1, b2 from t2 where b1 > '0') or a2 < '9';
-- echo UNION subqueries are currently limited to only use IN-TO-EXISTS.
explain
select a2 from t1 where a2 in (select b2 from t2 UNION select b3 from t2 as t3);
select a2 from t1 where a2 in (select b2 from t2 UNION select b3 from t2 as t3);
-- echo
explain
select a1 from t1 where a1 = '1 - 02' and a1 in (select max(b1) from t2 where b2 = '2 - 02');
select a1 from t1 where a1 = '1 - 02' and a1 in (select max(b1) from t2 where b2 = '2 - 02');
-- echo
explain
select a1, a2 from t1 where (a1, a2) in (select b1, b2 from t2 order by b3);
select a1, a2 from t1 where (a1, a2) in (select b1, b2 from t2 order by b3);

-- echo
-- echo /* C. Subqueries in the WHERE clause with GROUP BY. */
explain
select * from t1 where a1 in (select b1 from t2 where b1 > '0' group by b1);
select * from t1 where a1 in (select b1 from t2 where b1 > '0' group by b1);
-- echo
explain
select * from t1 where (a1, a2) in (select b1, b2 from t2 where b1 > '0' group by b1, b2);
select * from t1 where (a1, a2) in (select b1, b2 from t2 where b1 > '0' group by b1, b2);
-- echo
explain
select * from t1 where (a1, a2) in (select b1, b2 from t2 where b1 > '0' group by b1, b2 having b2 <  '2 - 04');
select * from t1 where (a1, a2) in (select b1, b2 from t2 where b1 > '0' group by b1, b2 having b2 <  '2 - 04');
-- echo
explain
select * from t1 where (a1, a2, a3) in (select b1, b2, b3 from t2 group by b1, b2, b3);
select * from t1 where (a1, a2, a3) in (select b1, b2, b3 from t2 group by b1, b2, b3);
-- echo
explain
select * from t1 where (a1, a2, a3) in (select b1, b2, b3 from t2 where b3 = '3 - 02' group by b1, b2);
select * from t1 where (a1, a2, a3) in (select b1, b2, b3 from t2 where b3 = '3 - 02' group by b1, b2);
-- echo
explain
select * from t1 where (a1,a2,a3) in (select b1,b2,b3 from t2 where b1 = '1 - 01' group by b1,b2,b3);
select * from t1 where (a1,a2,a3) in (select b1,b2,b3 from t2 where b1 = '1 - 01' group by b1,b2,b3);

-- echo
-- echo /*
-- echo   D. Subqueries for which materialization is not possible, and the
-- echo   optimizer reverts to in-to-exists.
-- echo */
# The first two cases are rejected during the prepare phase by the procedure
# subquery_types_allow_materialization().
explain
select left(a1,7), left(a2,7) from t1_1024 where a1 in (select b1 from t2_1024 where b1 > '0') or a2 < '9';
select left(a1,7), left(a2,7) from t1_1024 where a1 in (select b1 from t2_1024 where b1 > '0') or a2 < '9';
explain
select left(a1,7), left(a2,7) from t1_1024 where (a1,a2) in (select b1, b2 from t2_1024 where b1 > '0') or a2 < '9';
select left(a1,7), left(a2,7) from t1_1024 where (a1,a2) in (select b1, b2 from t2_1024 where b1 > '0') or a2 < '9';
-- echo
# The following two subqueries return the result of a string function with a
# blob argument, where the return type may be != blob. These are rejected during
# cost-based optimization when attempting to create a temporary table.
explain
select left(a1,7), left(a2,7) from t1_1024 where a1 in (select substring(b1,1,1024) from t2_1024 where b1 > '0') or a2 < '9';
select left(a1,7), left(a2,7) from t1_1024 where a1 in (select substring(b1,1,1024) from t2_1024 where b1 > '0') or a2 < '9';
explain
select left(a1,7), left(a2,7) from t1_1024 where (a1,a2) in (select substring(b1,1,1024), substring(b2,1,1024) from t2_1024 where b1 > '0') or a2 < '9';
select left(a1,7), left(a2,7) from t1_1024 where (a1,a2) in (select substring(b1,1,1024), substring(b2,1,1024) from t2_1024 where b1 > '0') or a2 < '9';
-- echo


-- echo
-- echo /* E. Edge cases. */
-- echo

-- echo /* E.1 Both materialization and in_to_exists cannot be off. */
set @save_optimizer_switch=@@optimizer_switch;
set @@optimizer_switch = 'materialization=off,in_to_exists=off';
--error ER_ILLEGAL_SUBQUERY_OPTIMIZER_SWITCHES
select * from t1 where a1 in (select b1 from t2 where b1 > '0' group by b1);
set @@optimizer_switch = @save_optimizer_switch;

-- echo /* E.2 Outer query without tables, always uses IN-TO-EXISTS. */
explain
select '1 - 03' in (select b1 from t2 where b1 > '0');
select '1 - 03' in (select b1 from t2 where b1 > '0');

-- echo /* E.3 Subqueries without tables. */
explain
select a1 from t1 where a1 in (select '1 - 03') or a2 < '9';
select a1 from t1 where a1 in (select '1 - 03') or a2 < '9';
-- echo UNION subqueries are currently limited to only use IN-TO-EXISTS.
explain
select a1 from t1 where a1 in (select '1 - 03' UNION select '1 - 02');
select a1 from t1 where a1 in (select '1 - 03' UNION select '1 - 02');

-- echo /* E.4 optimize_cond detects FALSE where/having clause. */
explain
select a1 from t1 where a1 in (select b1 from t2 where b1 = b2 and b2 = '1 - 03' and b1 = '1 - 02' ) or a2 < '9';
select a1 from t1 where a1 in (select b1 from t2 where b1 = b2 and b2 = '1 - 03' and b1 = '1 - 02' ) or a2 < '9';

-- echo /* E.5 opt_sum_query detects no matching min/max row or substitutes MIN/MAX with a const. */
-- echo TODO this test produces wrong result due to missing logic to handle the case
-- echo when JOIN::optimize detects an empty subquery result.
explain
select a1 from t1 where a1 in (select max(b1) from t2);
select a1 from t1 where a1 in (select max(b1) from t2);
-- echo
explain
select a1 from t1 where a1 in (select max(b1) from t2 where b1 = '7 - 02');
select a1 from t1 where a1 in (select max(b1) from t2 where b1 = '7 - 02');

-- echo /* E.6 make_join_select detects impossible WHERE. *

-- echo TODO

-- echo /* E.7 constant optimization detects "no matching row in const table". */

-- echo TODO

-- echo /* E.8 Impossible WHERE noticed after reading const tables. */
explain
select '1 - 03' in (select b1 from t2 where b1 > '0' and b1 < '0');
select '1 - 03' in (select b1 from t2 where b1 > '0' and b1 < '0');

-- echo
-- echo /* F. UPDATE/DELETE with subqueries. */
-- echo

-- echo TODO
-- echo
