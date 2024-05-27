--source include/elide_costs.inc
set cte_max_recursion_depth=50000;
select @@cte_max_recursion_depth;

flush status;

--echo # Mutual recursion unsupported; cycles must have one node only
--error ER_NO_SUCH_TABLE
with recursive qn as (select * from qn2),
               qn2 as (select * from qn)
select * from qn;

--echo # At least one anchor member, all anchors before all recursive

--error ER_CTE_RECURSIVE_REQUIRES_UNION
with recursive qn as
  (select 1 from qn)
select * from qn;

--error ER_CTE_RECURSIVE_REQUIRES_NONRECURSIVE_FIRST
with recursive qn as
  (select 1 from qn union all
   select 1 from dual)
select * from qn;

--error ER_CTE_RECURSIVE_REQUIRES_NONRECURSIVE_FIRST
with recursive qn as
  (select 1 union all select 1 from qn union all select 1)
select * from qn;

--error ER_CTE_RECURSIVE_REQUIRES_NONRECURSIVE_FIRST
with recursive qn as
  (select 1 from qn union all select 1 from qn)
select * from qn;

--echo # It's ok to have the RECURSIVE word without any recursive member

with recursive qn as
  (select 1 from dual union all
   select 1 from dual)
select * from qn;

--echo # UNION DISTINCT allowed
--echo # Also demonstrates EXPLAIN FORMAT=TREE of recursive CTEs.

let $query =
with recursive qn as
  (select 1 from dual union
   select 1 from qn)
select * from qn;

--replace_regex $elide_metrics
eval EXPLAIN FORMAT=tree $query;
eval $query;

--echo # No aggregation on the QN
create table t1(b int);
insert into t1 values(10),(20),(10);

with recursive qn as
  (select max(b) as a from t1 union
   select a from qn)
select * from qn;
--error ER_CTE_RECURSIVE_FORBIDS_AGGREGATION
with recursive qn as
  (select b as a from t1 union
   select max(a) from qn)
select * from qn;

--echo # No window functions
with recursive qn as
  (select rank() over (order by b) as a from t1 union
   select a from qn)
select * from qn;
--error ER_CTE_RECURSIVE_FORBIDS_AGGREGATION
with recursive qn as
  (select b as a from t1 union
   select rank() over (order by a) from qn)
select * from qn;
drop table t1;

--error ER_CTE_RECURSIVE_FORBIDS_AGGREGATION
with recursive qn as
  (select 1 as a from dual union all
   select max(a) from qn)
select * from qn;

--echo # No GROUP BY
with recursive qn as
  (select 1 as a from dual group by a union all
   select a+1 from qn where a<3)
select * from qn;
--error ER_CTE_RECURSIVE_FORBIDS_AGGREGATION
with recursive qn as
  (select 1 as a from dual union all
   select a from qn group by a)
select * from qn;

--echo # No subquery referencing a QN
--error ER_CTE_RECURSIVE_REQUIRES_SINGLE_REFERENCE
with recursive qn as (
select 1 from dual union all
select 1 from dual where 1 not in(select * from qn))
select * from qn;

--error ER_CTE_RECURSIVE_REQUIRES_SINGLE_REFERENCE
with recursive qn as (
select 1 from dual union all
select 1 from qn
order by (select * from qn))
select * from qn;

--echo # Reject also if this subquery is a derived table.
--error ER_CTE_RECURSIVE_REQUIRES_SINGLE_REFERENCE
with recursive qn as (
select 1 from dual union all
select * from (select * from qn) as dt)
select * from qn;

--echo # no ORDER BY as it causes one more tmp table => doesn't work.
--error ER_NOT_SUPPORTED_YET
with recursive qn as (
select 1 as a from dual union all
select 1 from qn
order by a)
select * from qn;
--echo # No matter if global, or attached to one recursive member.
--error ER_NOT_SUPPORTED_YET
with recursive qn as (
select 1 as a from dual union all
(select 1 from qn order by a))
select * from qn;
--echo # Allowed on non-recursive query block (though pointless)
with recursive qn as (
(select 1 as a from dual order by a) union all
select a+1 from qn where a<3)
select * from qn;

--echo # No LIMIT inside recursive query block
--error ER_NOT_SUPPORTED_YET
with recursive qn as (
select 1 as a from dual union all
(select 1 from qn limit 10))
select * from qn;

-- echo # No SELECT DISTINCT
--error ER_NOT_SUPPORTED_YET
WITH RECURSIVE qn AS
(select 1 union all select distinct 3 from qn)
select * from qn;

--error ER_CTE_RECURSIVE_REQUIRES_SINGLE_REFERENCE
with recursive qn as (select 1 from dual union all
                      select 1 from dual
                        where 1 not in(select * from qn))
select * from qn;

--echo # Numbers from 123 to 130:

with recursive qn as (select 123 as a union all select 1+a from qn where a<130) select * from qn;

--echo # One-level recursive sequence of numbers
with recursive qn as (select 1 as n, 2 as un union all select 1+n, un*5-6 from qn where n<10) select * from qn;

--echo # Fibonacci

with recursive qn as (select 1 as n, 1 as un, 1 as unp1 union all select 1+n, unp1, un+unp1 from qn where n<10) select * from qn;

--echo # Validate that cast(a_varchar as char) produces a varchar, not a
--echo # char.
create table t(c char(3), vc varchar(3), b binary(3), vb varbinary(3));
create table u
select cast(c as char(4)), cast(vc as char(4)),
       cast(b as binary(4)), cast(vb as binary(4)),
       "abc" as literal_c, cast("abc" as char(4)),
       _binary "abc" as literal_b, cast(_binary "abc" as binary(4))
from t;
show create table u;
drop table t,u;

--echo # if it used char the 'x' would fall off due to spaces.
with recursive qn as (select 1 as n, cast('x' as char(100)) as un union all select 1+n, concat(un,'x') from qn where n<10) select * from qn;

--echo # String now growing at the left

with recursive qn as (select cast("x" as char(10)) as a from dual
union all select concat("x",a) from qn where length(a)<10) select *
from qn;

--echo # Forgot cast-as-char(10) in anchor => qn.a column has length 1
--echo # => concat() is cast as char(1) => truncation
--echo # => length is always 1 => infinite loop; to prevent this and be
--echo # helpful, raise error when truncating, if in strict mode. Like if:
--echo # CREATE tmp table SELECT non-recursive part;
--echo # INSERT INTO tmp table SELECT recursive part;

let $query=
with recursive qn as (select "x" as a from dual
union all select concat("x",a) from qn where length(a)<10) select *
from qn;

if (`select @@sql_mode like '%strict%'`)
{

# We want the CTE generation to behave like:
create temporary table tt select "x" as a from dual;
# Use a second table, due to ER_CANT_REOPEN
create temporary table tt1 select "x" as a from dual;
--error ER_DATA_TOO_LONG
insert into tt1 select concat("x",a) from tt where length(a)<10;
drop temporary table tt,tt1;

# Test the CTE:
--error ER_DATA_TOO_LONG
eval $query;
}

--echo # Overflow integer type INT (max 4G)
--error ER_DATA_OUT_OF_RANGE
with recursive qn as (select 1 as a from dual
union all select a*2000 from qn where a<10000000000000000000) select * from qn;

--echo # Use Decimal
with recursive qn as (select cast(1 as decimal(30,0)) as a from dual
union all select a*2000 from qn where a<10000000000000000000) select * from qn;

--echo # Columns of a recursive QN are always NULLable, as in the Standard.
--echo # Without it, we would get conversion
--echo # of NULL to 0 and an infinite loop.
with recursive qn as (select 123 as a union all
select null from qn where a is not null) select * from qn;

--echo # Mixing really unrelated types: the goal is to report a sensible
--echo # error and not crash.

--echo # The Point becomes a string which is an invalid integer:
--error ER_TRUNCATED_WRONG_VALUE_FOR_FIELD
with recursive qn as (
select 1 as a,1
union all
select a+1,ST_PointFromText('POINT(10 10)') from qn where a<2)
select * from qn;

--echo # POINT in anchor => BLOB in tmp table => not MEMORY engine => Innodb
--error ER_CANT_CREATE_GEOMETRY_OBJECT
with recursive qn as (
select 1 as a,ST_PointFromText('POINT(10 10)')
union all
select a+1,1 from qn where a<2)
select * from qn;

--echo # Same number of columns in anchor and recursive members
--error ER_WRONG_NUMBER_OF_COLUMNS_IN_SELECT
WITH RECURSIVE qn AS
(
select 1
union all
select 3, 0 from qn
)
select * from qn;

--echo # Mismatch in column name and column count; problem specific of
--echo # recursive CTE which creates tmp table earlier in preparation.
--error ER_VIEW_WRONG_LIST
with recursive q (b) as (select 1, 1 union all select 1, 1 from q)
select b from q;

--echo # Cannot have two recursive refs in FROM:

--error ER_CTE_RECURSIVE_REQUIRES_SINGLE_REFERENCE
with recursive qn as (
select 123 as a union all
select 1+qn.a from qn, qn as qn1 where qn1.a<130)
select * from qn;

--echo # Prove that a materialized QN is shared among all references:

flush status;
--sorted_result
with recursive qn as (
select 123 as a union all
select 1+a from qn where a<125)
select * from qn;
show status like "handler_write";
flush status;
--sorted_result
with recursive qn as (
select 123 as a union all
select 1+a from qn where a<125)
select * from qn, qn as qn1;
show status like "handler_write";
show status like 'Created_tmp%table%';

--echo # Also works if references are nested inside other query names:
flush status;
--sorted_result
with recursive inner_ as (
select 123 as a union all
select 1+a from inner_ where a<125),
outer_ as (select * from inner_ limit 10)
select * from outer_, outer_ as outer1;
show status like "handler_write";

flush status;
--sorted_result
with recursive inner_ as (
select 123 as a union all
select 1+a from inner_ where a<125),
outer_ as
(select inner_.a, inner1.a as a1
from inner_, inner_ as inner1 limit 10)
select * from outer_;
show status like "handler_write";

--echo # Even if the two query names are recursive:
flush status;
--sorted_result
with recursive inner_ as (
select 123 as a union all
select 1+a from inner_ where a<125),
outer_ as
(select a from inner_ union all
select a*2 from outer_ where a<1000)
select a from outer_;
show status like "handler_write";

--echo # Optimizer must be allowed to put the recursive reference first

create table t1(a int);
insert into t1 values(1),(2);

--error ER_CTE_RECURSIVE_FORBIDDEN_JOIN_ORDER
WITH RECURSIVE qn AS
(
select 1 from t1
union all
select 1 from t1 straight_join qn
)
select * from qn;

WITH RECURSIVE qn(n) AS
(
SELECT 1 FROM t1
UNION ALL
SELECT n+1 FROM qn STRAIGHT_JOIN t1 WHERE n < 3
)
SELECT COUNT(*) FROM qn;

--error ER_CTE_RECURSIVE_FORBIDDEN_JOIN_ORDER
WITH RECURSIVE qn AS
(
select 1 from t1
union all
select 1 from t1 left join qn on 1
)
select * from qn;

--echo # Empty anchor

WITH RECURSIVE qn AS
(
select a from t1 where 0
union all
select a+1 from qn
)
select * from qn;

WITH RECURSIVE qn AS
(
select a from t1 where a>10
union all
select a+1 from qn
)
select * from qn;

--echo # UNION DISTINCT in anchor parts
insert into t1 values(1),(2);

set @c=0, @d=0;
WITH RECURSIVE qn AS
(
select 1,0 as col from t1
union distinct
select 1,0 from t1
union all
select 3, 0*(@c:=@c+1) from qn where @c<1
union all
select 3, 0*(@d:=@d+1) from qn where @d<1
)
select * from qn;

--echo # UNION DISTINCT affecting recursive member, followed by UNION ALL
insert into t1 values(1),(2);

set @c=0, @d=0;
--error ER_NOT_SUPPORTED_YET
WITH RECURSIVE qn AS
(
select 1,0 as col from t1
union distinct
select 3, 0*(@c:=@c+1) from qn where @c<1
union all
select 3, 0*(@d:=@d+1) from qn where @d<1
)
select * from qn;

--echo # create select
create table t2
with recursive qn as (
select 123 as a union all select 1+a from qn where a<130)
select * from qn;
select * from t2;
drop table t2;

--echo # insert select
delete from t1;
insert into t1
with recursive qn as (
select 123 as a union all select 1+a from qn where a<130)
select * from qn;
select * from t1;

--echo # Using insertion target inside recursive query
delete from t1;
insert into t1 values(1),(2);
insert into t1
with recursive qn as (
select 123 as a union all select 1+qn.a from qn, t1 where qn.a<125)
select * from qn;
select * from t1;

drop table t1;

--echo # insert into tmp table (a likely use case)

create temporary table t1(a int);
insert into t1
with recursive qn as (
select 123 as a union all select 1+a from qn where a<130)
select * from qn;
select * from t1;
drop table t1;

--echo # create view
create view v1 as
with recursive qn as (
select 123 as a union all select 1+a from qn where a<130)
select * from qn;
select * from v1;
drop view v1;

--echo # Recursive QN can be constant (0-row or 1-row) for the
--echo # optimizer if its members have impossible conditions:

let $query=
with recursive qn (n) as (
 select 1 where 0 union all select n+1 from qn where 0
) select * from qn;
eval explain $query;
eval $query;

let $query=
with recursive qn (n) as (
 select 1 where 1 union all select n+1 from qn where 0
) select * from qn;
eval explain $query;
eval $query;

--echo # Recursive refs should never use indexes to read:
--echo # first, optimization of top query creates a key on q.b;
--echo # then optimization of scalar subquery, when it optimizes the
--echo # recursive member, must be prevented from re-using this key
--echo # (it was a bug that it re-used it, as the index is covering
--echo # and adjust_access_methods() has a heuristic which converts a
--echo # table scan to index scan, so it wrongly used an index scan).
let $query=
with recursive q (b) as
 (select 1 union all select 1+b from q where b<10)
 select (select q1.b from q as q2 where q2.b=3) from q as q1 where q1.b=3;
eval explain $query;
eval $query;

--echo # Recursive query to update/delete a table

create table t1(a int);
insert into t1
with recursive qn (n) as (
 select 1 union all select n+1 from qn where n<10
) select * from qn;
select * from t1;
# Multi-table delete
with recursive qn (n) as (
 select 5 union all select n+2 from qn where n<10
) delete t1 from t1 where a in (select * from qn);
select * from t1;
# Single-table delete
with recursive qn (n) as (
 select 4 union all select n+2 from qn where n<10
) delete from t1 where a in (select * from qn);
select * from t1;
with recursive qn (n) as (
 select 2 union all select n+2 from qn where n<10
) update t1,qn set t1.a=qn.n where t1.a=1+qn.n;
select * from t1;

drop table t1;

--echo # This is from my blog so I can use it here.
--echo # Tests depth-first etc

CREATE TABLE employees (
ID INT PRIMARY KEY,
NAME VARCHAR(100),
MANAGER_ID INT,
INDEX (MANAGER_ID),
FOREIGN KEY (MANAGER_ID) REFERENCES employees(ID)
);
INSERT INTO employees VALUES
(333, "Yasmina", NULL),
(198, "John", 333),
(692, "Tarek", 333),
(29, "Pedro", 198),
(4610, "Sarah", 29),
(72, "Pierre", 29),
(123, "Adil", 692);
ANALYZE TABLE employees;

--echo # Depth-first.

--echo # Also test column names, and their reference in the recursive member.
WITH RECURSIVE employees_extended(ID, NAME, PATH)
AS
(
  SELECT ID, NAME, CAST(ID AS CHAR(200))
  FROM employees
  WHERE MANAGER_ID IS NULL
UNION ALL
  SELECT S.ID, S.NAME, CONCAT(M.PATH, ",", S.ID)
  FROM employees_extended M STRAIGHT_JOIN employees S ON M.ID=S.MANAGER_ID
)
SELECT * FROM employees_extended ORDER BY PATH;

--echo # Breadth-first is likely what we get, if no ordering

WITH RECURSIVE employees_extended
AS
(
  SELECT ID, NAME, CAST(ID AS CHAR(200)) AS PATH
  FROM employees
  WHERE MANAGER_ID IS NULL
UNION ALL
  SELECT S.ID, S.NAME, CONCAT(M.PATH, ",", S.ID)
  FROM employees_extended M STRAIGHT_JOIN employees S ON M.ID=S.MANAGER_ID
)
SELECT * FROM employees_extended;

--echo # But to be really sure we have breadth-first, we generate a
--echo # numeric column SEQ. And sort by NAME, to have repeatable
--echo # order of siblings (who have the same SEQ).

WITH RECURSIVE employees_extended
AS
(
  SELECT 0 AS SEQ, ID, NAME, CAST(ID AS CHAR(200)) AS PATH
  FROM employees
  WHERE MANAGER_ID IS NULL
UNION ALL
  SELECT M.SEQ+1, S.ID, S.NAME, CONCAT(M.PATH, ",", S.ID)
  FROM employees_extended M STRAIGHT_JOIN employees S ON M.ID=S.MANAGER_ID
)
SELECT * FROM employees_extended ORDER BY SEQ, NAME;

--echo # Or, use a user variable, then all rows have different number:

WITH RECURSIVE employees_extended
AS
(
  SELECT (@s:=0) AS SEQ, ID, NAME, CAST(ID AS CHAR(200)) AS PATH
  FROM employees
  WHERE MANAGER_ID IS NULL
UNION ALL
  SELECT (@s:=@s+1), S.ID, S.NAME, CONCAT(M.PATH, ",", S.ID)
  FROM employees_extended M STRAIGHT_JOIN employees S ON M.ID=S.MANAGER_ID
)
SELECT * FROM employees_extended ORDER BY SEQ;

--echo # Direct & indirect reports of John = having John in their PATH

--sorted_result
WITH RECURSIVE employees_extended
AS
(
  SELECT ID, NAME, CAST(ID AS CHAR(200)) AS PATH
  FROM employees
  WHERE MANAGER_ID IS NULL
UNION ALL
  SELECT S.ID, S.NAME, CONCAT(M.PATH, ",", S.ID)
  FROM employees_extended M STRAIGHT_JOIN employees S ON M.ID=S.MANAGER_ID
)
SELECT * FROM employees_extended
WHERE FIND_IN_SET((SELECT ID FROM employees WHERE NAME='John'),
                  PATH);

--echo # Exclude John, he's not a report of himself;
--echo # bonus: use a QN to cache his ID.

--sorted_result
WITH RECURSIVE employees_extended(ID, NAME, PATH)
AS
(
  SELECT ID, NAME, CAST(ID AS CHAR(200))
  FROM employees
  WHERE MANAGER_ID IS NULL
UNION ALL
  SELECT S.ID, S.NAME, CONCAT(M.PATH, ",", S.ID)
  FROM employees_extended M STRAIGHT_JOIN employees S ON M.ID=S.MANAGER_ID
),
JOHN_ID AS (SELECT ID FROM employees WHERE NAME='John')
SELECT e.* FROM employees_extended e, JOHN_ID
WHERE FIND_IN_SET(JOHN_ID.ID,
                  PATH)
      AND e.ID<>JOHN_ID.ID;

--echo # Similar, but faster: start dive at John (and include him again).
--sorted_result
WITH RECURSIVE employees_extended
AS
(
  SELECT ID, NAME, CAST(ID AS CHAR(200)) AS PATH
  FROM employees
  WHERE NAME='John'
UNION ALL
  SELECT S.ID, S.NAME, CONCAT(M.PATH, ",", S.ID)
  FROM employees_extended M STRAIGHT_JOIN employees S ON M.ID=S.MANAGER_ID
)
SELECT * FROM employees_extended;

--echo # Get the management chain above Pierre:

--sorted_result
WITH RECURSIVE employees_extended
AS
(
  SELECT ID, NAME, MANAGER_ID, CAST(ID AS CHAR(200)) AS PATH
  FROM employees
  WHERE NAME='Pierre'
UNION ALL
  SELECT S.ID, S.NAME, S.MANAGER_ID, CONCAT(M.PATH, ",", S.ID)
  FROM employees_extended M JOIN employees S ON M.MANAGER_ID=S.ID
)
SELECT * FROM employees_extended;

--echo # Get the management chain above Pierre, without PATH

--sorted_result
WITH RECURSIVE employees_extended
AS
(
  SELECT ID, NAME, MANAGER_ID
  FROM employees
  WHERE NAME='Pierre'
UNION ALL
  SELECT S.ID, S.NAME, S.MANAGER_ID
  FROM employees_extended M JOIN employees S ON M.MANAGER_ID=S.ID
)
SELECT * FROM employees_extended;

--echo # Get the management chain above Pierre and Sarah, without PATH

--sorted_result
WITH RECURSIVE employees_extended
AS
(
  SELECT ID, NAME, MANAGER_ID
  FROM employees
  WHERE NAME='Pierre' OR NAME='Sarah'
UNION ALL
  SELECT S.ID, S.NAME, S.MANAGER_ID
  FROM employees_extended M JOIN employees S ON M.MANAGER_ID=S.ID
)
SELECT * FROM employees_extended;

--echo # Do it without duplicates

--sorted_result
WITH RECURSIVE employees_extended
AS
(
  SELECT ID, NAME, MANAGER_ID
  FROM employees
  WHERE NAME='Pierre' OR NAME='Sarah'
UNION
  SELECT S.ID, S.NAME, S.MANAGER_ID
  FROM employees_extended M JOIN employees S ON M.MANAGER_ID=S.ID
)
SELECT * FROM employees_extended;

--echo # Cycles. Introduce an oddity:

--echo # Sarah is indirect report of John and is his manager.
UPDATE employees SET MANAGER_ID=4610 WHERE NAME="John";

--echo # Previous query now produces infinite PATHs which overflow the column:
# Depending on the query plan, the number of the row causing the
# overflow varies; as it's reported in the error message, we need a
# fixed plan, hence the initial ANALYZE TABLE.

--error ER_DATA_TOO_LONG
WITH RECURSIVE employees_extended
AS
(
  SELECT ID, NAME, CAST(ID AS CHAR(200)) AS PATH
  FROM employees
  WHERE NAME='John'
UNION ALL
  SELECT S.ID, S.NAME, CONCAT(M.PATH, ",", S.ID)
  FROM employees_extended M STRAIGHT_JOIN employees S ON M.ID=S.MANAGER_ID
)
SELECT * FROM employees_extended;

--echo # Add cycle detection: the row closing a cycle is marked with
--echo # IS_CYCLE=1, which stops the iterations. The outer SELECT
--echo # could then want to see only that row, or only previous ones.
--sorted_result
WITH RECURSIVE employees_extended(ID, NAME, PATH, IS_CYCLE)
AS
(
  SELECT ID, NAME, CAST(ID AS CHAR(200)), 0
  FROM employees
  WHERE NAME='John'
UNION ALL
  SELECT S.ID, S.NAME, CONCAT(M.PATH, ",", S.ID), FIND_IN_SET(S.ID, M.PATH)
  FROM employees_extended M STRAIGHT_JOIN employees S ON M.ID=S.MANAGER_ID
  WHERE M.IS_CYCLE=0
)
SELECT * FROM employees_extended;

DROP TABLE employees;

--echo # Two recursive members.

create table t1 (id int, name char(10), leftpar int, rightpar int);
insert into t1 values
(1, "A", 2, 3),
  (2, "LA", 4, 5),
     (4, "LLA", 6, 7),
        (6, "LLLA", null, null),
        (7, "RLLA", null, null),
     (5, "RLA", 8, 9),
        (8, "LRLA", null, null),
        (9, "RRLA", null, null),
  (3, "RA", 10, 11),
     (10, "LRA", 12, 13),
     (11, "RRA", 14, 15),
        (15, "RRRA", null, null),
(16, "B", 17, 18),
  (17, "LB", null, null),
  (18, "RB", null, null)
;

--echo # Shuffle rows to make sure the algorithm works
--echo # with any read order of rows above

create table t2 select * from t1 order by rand();

--echo # Tree-walking query. We turn off the Query Cache: indeed
--echo # sometimes pb2 enables Query Cache and as we run twice the
--echo # same query the 2nd may not actually be executed so the value
--echo # of Created_tmp_tables displayed at end becomes "one less").

let $query=
with recursive tree_of_a as
(
select *, cast(id as char(200)) as path from t2 where name="A"
union all
select t2.*, concat(tree_of_a.path,",",t2.id) from t2 join tree_of_a on
t2.id=tree_of_a.leftpar
union all
select t2.*, concat(tree_of_a.path,",",t2.id) from t2 join tree_of_a on
t2.id=tree_of_a.rightpar
)
select * from tree_of_a;

--echo # Note that without ORDER BY, order of rows would be random as BNL
--echo # implies that the randomized t2 is the driving table in the
--echo # joining of rows.

--replace_column 10 #
eval explain $query order by path;
eval $query order by path;

--echo # Let's turn BNL off to verify that ORDER BY works the same:
set @optimizer_switch_saved= @@optimizer_switch;
set optimizer_switch='block_nested_loop=off';

--replace_column 10 #
eval explain $query order by path;
eval $query order by path;

--echo # Also demonstrates EXPLAIN FORMAT=TREE of recursive CTEs with joins.
--skip_if_hypergraph  # Chooses different plan (hash join a few places).
--replace_regex $elide_metrics
eval EXPLAIN FORMAT=tree $query order by path;

--echo # Without ORDER BY order is different; it is deterministic as
--echo # the CTE is the driving table (no BNL) but a user shouldn't
--echo # rely on it, just as he shouldn't expect some particular order
--echo # when selecting from a derived table containing a join.

eval $query;

set @@optimizer_switch=@optimizer_switch_saved;

--echo # Equivalent query with one single recursive query block:

with recursive tree_of_a as
(
select *, cast(id as char(200)) as path from t2 where name="A"
union all
select t2.*, concat(tree_of_a.path,",",t2.id) from t2 join tree_of_a on
(t2.id=tree_of_a.leftpar or t2.id=tree_of_a.rightpar)
)
select * from tree_of_a
order by path;

--echo # Demonstrate a case where an index is automatically created on
--echo # the derived table and used to read this table in the outer
--echo # query (but correctly not used to read it in the recursive
--echo # query).

let $query=
with recursive tree_of_a as
(
select *, cast(id as char(200)) as path from t2 where name="A"
union all
select t2.*, concat(tree_of_a.path,",",t2.id) from t2 join tree_of_a on
t2.id=tree_of_a.leftpar
union all
select t2.*, concat(tree_of_a.path,",",t2.id) from t2 join tree_of_a on
t2.id=tree_of_a.rightpar
)
select * from tree_of_a where id=2;

--replace_column 10 #
eval explain $query;
eval $query;

drop table t1,t2;

--echo # Verify that after materialization, accessing 3 references to
--echo # the same CTE using different access methods (scan, ref, ref),
--echo # works without one method disturbing the others.
--echo # Turning BNL off since it is faster and allows to have "ref"
--echo # on cte3 which is more interesting.

let $query=
with recursive cte as
(
  select 1 as n union all
  select n+1 from cte where n<10000
)
select /*+ no_bnl(cte3) */
sum(cte1.n*cte2.n*cte3.n)=2490508525950000
from cte cte1, cte cte2, cte cte3
where cte1.n=cte2.n+10 and cte2.n+20=cte3.n;

--replace_column 10 #
eval explain $query;
eval $query;

--echo #
--echo # Transitive closure
--echo #

create table nodes(id int);
create table arcs(from_id int, to_id int);
insert into nodes values(1),(2),(3),(4),(5),(6),(7),(8);
insert into arcs values(1,3), (3,6), (1,4), (4,6), (6,2), (2,1);

--echo # UNION ALL leads to infinite loop as 1 is reachable from 1;
--echo # so we stop it with a maximum depth 8 (8 nodes in graph)

with recursive cte as
(
  select id, 0 as depth from nodes where id=1
  union all
  select to_id, depth+1 from arcs, cte
  where from_id=cte.id and depth<8
)
select count(*), max(depth) from cte;

--echo # Can use cycle detection:

--sorted_result
with recursive cte as
(
  select id, cast(id as char(200)) as path, 0 as is_cycle
  from nodes where id=1
  union all
  select to_id, concat(cte.path, ",", to_id), find_in_set(to_id, path)
  from arcs, cte
  where from_id=cte.id and is_cycle=0
)
select * from cte;

--echo # It is simpler with DISTINCT:
--sorted_result
with recursive cte as
(
  select id from nodes where id=1
  union
  select to_id from arcs, cte where from_id=cte.id
)
select * from cte;

drop table nodes, arcs;

--echo # Hash field and MEMORY don't work together. Make long distinct
--echo # key to force hash field, to see if it switches to InnoDB.

--echo # Not too long key (500 bytes in latin1)

flush status;

--sorted_result
with recursive cte as
 (
  select 1 as n,
  repeat('a',500) as f, '' as g,
  '' as h, '' as i
  union
  select n+1,
  '','','',''
  from cte where n<100)
select sum(n) from cte;

show status like 'Created_tmp_disk_tables';

--echo # Too long key (>3000 bytes in latin1)
--sorted_result
with recursive cte as
 (
  select 1 as n,
  repeat('a',500) as f, repeat('a',500) as g,
  repeat('a',500) as h, repeat('a',500) as i,
  repeat('a',500) as j, repeat('a',500) as k,
  repeat('a',500) as l, repeat('a',500) as m
  union
  select n+1,
  '','','','','','','',''
  from cte where n<100)
select sum(n) from cte;

--echo #
--echo # In query planning, the recursive reference's row count is
--echo # said to be the estimated row count of all non-recursive query
--echo # blocks

flush status;

create table t1(a int);
--echo # 15 rows:
insert into t1 values (), (), (), (), (), (), (), (), (), (), (), (),
(), (), ();
analyze table t1;
--echo # EXPLAIN says: in non-recursive QB we'll read 15 rows of t1,
--echo # in recursive QB we'll read 15 rows of qn, keep only 0.33
--echo # due to WHERE, that makes 4 (due to rounding), and in the
--echo # derived table we'll thus have 15+4=19. That ignores
--echo # next repetitions of the recursive QB which are unpredictable.
explain with recursive qn as
(select 1 as a from t1 union all select a+1 from qn where qn.a<100)
select * from qn;
explain with recursive qn as
(select 1 as a from t1 union distinct select a+1 from qn where qn.a<100)
select * from qn;
drop table t1;

show status like 'Created_tmp_disk_tables';
