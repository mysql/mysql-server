# Test that infinite WITH RECURSIVE can be killed
SET @@SESSION.cte_max_recursion_depth = 1000000000;
SET DEBUG_SYNC='in_WITH_RECURSIVE SIGNAL with_recursive_has_started';
with recursive q (num, mark) as (
select 1, "a"
 union all select 1+num, "b" from q where mark="a"
 union all select 1+num, "a" from q where mark="b"
)
select num from q;
SET DEBUG_SYNC='now WAIT_FOR with_recursive_has_started';
KILL QUERY @id;
ERROR 70100: Query execution was interrupted
SET DEBUG_SYNC= 'RESET';
SELECT 1;
1
1
SET DEBUG_SYNC= 'RESET';
# Test that it is also killed by max_execution_time.
SET @@SESSION.max_execution_time= 1000;
with recursive q (b) as (select 1 union all select 1+b from q)
select b from q;
Got one of the listed errors
SELECT 1;
1
1
