set session internal_tmp_mem_storage_engine='memory';
# This specifically tests WITH RECURSIVE queries which have
# required changes in InnoDB (done as WL#9248).
create table t1(a int);
insert into t1 values(1),(2),(3);
# Force InnoDB tmp table, not MEMORY
set big_tables=1;
# Problem required this:
set optimizer_switch='block_nested_loop=off';
# with one reader, there was no problem:
with qn as (select * from t1 limit 3) select * from qn;
a
1
2
3
# The problems were in the following scenarios.
# First all writes are done then two readers read
with qn as (select * from t1 limit 3) select * from qn, qn qn1;
a	a
1	1
1	2
1	3
2	1
2	2
2	3
3	1
3	2
3	3
# Second, with one reader and one writer (reader is the query block
# after UNION ALL, writer is the derived materialization code)
with recursive qn as (
select 0 as n
union all
select n+1 from qn where n<10)
select * from qn;
n
0
1
2
3
4
5
6
7
8
9
10
# With two readers (the two recursive query blocks after UNION ALL)
# and one writer (the derived materialization code).
with recursive qn as (
select 0 as n
union all
select 2*n+2 from qn where n<50
union all
select 2*n+1 from qn where n<50
)
select count(n),max(n) from qn;
count(n)	max(n)
101	100
set optimizer_switch=default;
# Test overflow from MEMORY engine to INNODB:
set big_tables=1;
# Set baseline, disk-based results:
set cte_max_recursion_depth=5000;
flush status;
with recursive q (b) as
(select 1 union all select 1+b from q where b<2000)
select min(b),max(b),avg(b) from q;
min(b)	max(b)	avg(b)
1	2000	1000.5000
show status like 'Created_tmp_disk_tables';
Variable_name	Value
Created_tmp_disk_tables	1
# Test when conversion to InnoDB affects recursive references which
# are not open yet (those of q1):
flush status;
with recursive q (b) as
(select 1 union all select 1+b from q where b<2000)
select min(q.b),max(q.b),avg(q.b) from q, q as q1;
min(q.b)	max(q.b)	avg(q.b)
1	2000	1000.5000
show status like 'Created_tmp_disk_tables';
Variable_name	Value
Created_tmp_disk_tables	1
# Same, but make q1 the writer; this is to test overflow when
# the writer isn't first in the 'tmp_tables' list
flush status;
with recursive q (b) as
(select 1 union all select 1+b from q where b<2000)
select min(q.b),max(q.b),avg(q.b) from q right join q as q1 on 1;
min(q.b)	max(q.b)	avg(q.b)
1	2000	1000.5000
show status like 'Created_tmp_disk_tables';
Variable_name	Value
Created_tmp_disk_tables	1
# Test when outer query reads CTE with an index.
# Overflow doesn't happen at same row as queries above, as this
# table has an index which makes it grow faster.
explain with recursive q (b) as
(select 1 union all select 1+b from q where b<2000)
select min(b),max(b),avg(b) from q where b=300;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	PRIMARY	<derived2>	NULL	ref	<auto_key0>	<auto_key0>	9	const	1	100.00	Using index
2	DERIVED	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	No tables used
3	UNION	q	NULL	ALL	NULL	NULL	NULL	NULL	2	50.00	Recursive; Using where
Warnings:
Note	1003	with recursive `q` (`b`) as (/* select#2 */ select 1 AS `1` union all /* select#3 */ select (1 + `q`.`b`) AS `1+b` from `q` where (`q`.`b` < 2000)) /* select#1 */ select min(`q`.`b`) AS `min(b)`,max(`q`.`b`) AS `max(b)`,avg(`q`.`b`) AS `avg(b)` from `q` where (`q`.`b` = 300)
show status like 'Created_tmp_disk_tables';
Variable_name	Value
Created_tmp_disk_tables	1
with recursive q (b) as
(select 1 union all select 1+b from q where b<2000)
select min(b),max(b),avg(b) from q where b=300;
min(b)	max(b)	avg(b)
300	300	300.0000
show status like 'Created_tmp_disk_tables';
Variable_name	Value
Created_tmp_disk_tables	2
# Verify that rows come out in insertion order.
# If they didn't, the sequences of @c and of 'b'
# would not be identical and the sum wouldn't be
# 1^2 + ... + 2000^2 = n(n+1)(2n+1)/6 = 2668667000
set @c:=1;
flush status;
with recursive q (b, c) as
(select 1, 1 union all select (1+b), (@c:=(@c+1)) from q where b<2000)
select sum(b*c) from q;
sum(b*c)
2668667000
Warnings:
Warning	1287	Setting user variables within expressions is deprecated and will be removed in a future release. Consider alternatives: 'SET variable=expression, ...', or 'SELECT expression(s) INTO variables(s)'.
show status like 'Created_tmp_disk_tables';
Variable_name	Value
Created_tmp_disk_tables	1
set big_tables=0;
# Set different limits, to have different overflow scenarios;
# we have overflow at record #122, #1481, #937 depending on
# limits and queries.
set @@tmp_table_size=1024,@@max_heap_table_size=16384;
set cte_max_recursion_depth=5000;
flush status;
with recursive q (b) as
(select 1 union all select 1+b from q where b<2000)
select min(b),max(b),avg(b) from q;
min(b)	max(b)	avg(b)
1	2000	1000.5000
show status like 'Created_tmp_disk_tables';
Variable_name	Value
Created_tmp_disk_tables	1
# Test when conversion to InnoDB affects recursive references which
# are not open yet (those of q1):
flush status;
with recursive q (b) as
(select 1 union all select 1+b from q where b<2000)
select min(q.b),max(q.b),avg(q.b) from q, q as q1;
min(q.b)	max(q.b)	avg(q.b)
1	2000	1000.5000
show status like 'Created_tmp_disk_tables';
Variable_name	Value
Created_tmp_disk_tables	1
# Same, but make q1 the writer; this is to test overflow when
# the writer isn't first in the 'tmp_tables' list
flush status;
with recursive q (b) as
(select 1 union all select 1+b from q where b<2000)
select min(q.b),max(q.b),avg(q.b) from q right join q as q1 on 1;
min(q.b)	max(q.b)	avg(q.b)
1	2000	1000.5000
show status like 'Created_tmp_disk_tables';
Variable_name	Value
Created_tmp_disk_tables	1
# Test when outer query reads CTE with an index.
# Overflow doesn't happen at same row as queries above, as this
# table has an index which makes it grow faster.
explain with recursive q (b) as
(select 1 union all select 1+b from q where b<2000)
select min(b),max(b),avg(b) from q where b=300;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	PRIMARY	<derived2>	NULL	ref	<auto_key0>	<auto_key0>	9	const	1	100.00	Using index
2	DERIVED	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	No tables used
3	UNION	q	NULL	ALL	NULL	NULL	NULL	NULL	2	50.00	Recursive; Using where
Warnings:
Note	1003	with recursive `q` (`b`) as (/* select#2 */ select 1 AS `1` union all /* select#3 */ select (1 + `q`.`b`) AS `1+b` from `q` where (`q`.`b` < 2000)) /* select#1 */ select min(`q`.`b`) AS `min(b)`,max(`q`.`b`) AS `max(b)`,avg(`q`.`b`) AS `avg(b)` from `q` where (`q`.`b` = 300)
show status like 'Created_tmp_disk_tables';
Variable_name	Value
Created_tmp_disk_tables	1
with recursive q (b) as
(select 1 union all select 1+b from q where b<2000)
select min(b),max(b),avg(b) from q where b=300;
min(b)	max(b)	avg(b)
300	300	300.0000
show status like 'Created_tmp_disk_tables';
Variable_name	Value
Created_tmp_disk_tables	2
# Verify that rows come out in insertion order.
# If they didn't, the sequences of @c and of 'b'
# would not be identical and the sum wouldn't be
# 1^2 + ... + 2000^2 = n(n+1)(2n+1)/6 = 2668667000
set @c:=1;
flush status;
with recursive q (b, c) as
(select 1, 1 union all select (1+b), (@c:=(@c+1)) from q where b<2000)
select sum(b*c) from q;
sum(b*c)
2668667000
Warnings:
Warning	1287	Setting user variables within expressions is deprecated and will be removed in a future release. Consider alternatives: 'SET variable=expression, ...', or 'SELECT expression(s) INTO variables(s)'.
show status like 'Created_tmp_disk_tables';
Variable_name	Value
Created_tmp_disk_tables	1
set @@tmp_table_size=30000,@@max_heap_table_size=30000;
Warnings:
Warning	1292	Truncated incorrect max_heap_table_size value: '30000'
set cte_max_recursion_depth=5000;
flush status;
with recursive q (b) as
(select 1 union all select 1+b from q where b<2000)
select min(b),max(b),avg(b) from q;
min(b)	max(b)	avg(b)
1	2000	1000.5000
show status like 'Created_tmp_disk_tables';
Variable_name	Value
Created_tmp_disk_tables	1
# Test when conversion to InnoDB affects recursive references which
# are not open yet (those of q1):
flush status;
with recursive q (b) as
(select 1 union all select 1+b from q where b<2000)
select min(q.b),max(q.b),avg(q.b) from q, q as q1;
min(q.b)	max(q.b)	avg(q.b)
1	2000	1000.5000
show status like 'Created_tmp_disk_tables';
Variable_name	Value
Created_tmp_disk_tables	1
# Same, but make q1 the writer; this is to test overflow when
# the writer isn't first in the 'tmp_tables' list
flush status;
with recursive q (b) as
(select 1 union all select 1+b from q where b<2000)
select min(q.b),max(q.b),avg(q.b) from q right join q as q1 on 1;
min(q.b)	max(q.b)	avg(q.b)
1	2000	1000.5000
show status like 'Created_tmp_disk_tables';
Variable_name	Value
Created_tmp_disk_tables	1
# Test when outer query reads CTE with an index.
# Overflow doesn't happen at same row as queries above, as this
# table has an index which makes it grow faster.
explain with recursive q (b) as
(select 1 union all select 1+b from q where b<2000)
select min(b),max(b),avg(b) from q where b=300;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	PRIMARY	<derived2>	NULL	ref	<auto_key0>	<auto_key0>	9	const	1	100.00	Using index
2	DERIVED	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	No tables used
3	UNION	q	NULL	ALL	NULL	NULL	NULL	NULL	2	50.00	Recursive; Using where
Warnings:
Note	1003	with recursive `q` (`b`) as (/* select#2 */ select 1 AS `1` union all /* select#3 */ select (1 + `q`.`b`) AS `1+b` from `q` where (`q`.`b` < 2000)) /* select#1 */ select min(`q`.`b`) AS `min(b)`,max(`q`.`b`) AS `max(b)`,avg(`q`.`b`) AS `avg(b)` from `q` where (`q`.`b` = 300)
show status like 'Created_tmp_disk_tables';
Variable_name	Value
Created_tmp_disk_tables	1
with recursive q (b) as
(select 1 union all select 1+b from q where b<2000)
select min(b),max(b),avg(b) from q where b=300;
min(b)	max(b)	avg(b)
300	300	300.0000
show status like 'Created_tmp_disk_tables';
Variable_name	Value
Created_tmp_disk_tables	2
# Verify that rows come out in insertion order.
# If they didn't, the sequences of @c and of 'b'
# would not be identical and the sum wouldn't be
# 1^2 + ... + 2000^2 = n(n+1)(2n+1)/6 = 2668667000
set @c:=1;
flush status;
with recursive q (b, c) as
(select 1, 1 union all select (1+b), (@c:=(@c+1)) from q where b<2000)
select sum(b*c) from q;
sum(b*c)
2668667000
Warnings:
Warning	1287	Setting user variables within expressions is deprecated and will be removed in a future release. Consider alternatives: 'SET variable=expression, ...', or 'SELECT expression(s) INTO variables(s)'.
show status like 'Created_tmp_disk_tables';
Variable_name	Value
Created_tmp_disk_tables	1
set @@tmp_table_size=60000,@@max_heap_table_size=60000;
Warnings:
Warning	1292	Truncated incorrect max_heap_table_size value: '60000'
set cte_max_recursion_depth=5000;
flush status;
with recursive q (b) as
(select 1 union all select 1+b from q where b<2000)
select min(b),max(b),avg(b) from q;
min(b)	max(b)	avg(b)
1	2000	1000.5000
show status like 'Created_tmp_disk_tables';
Variable_name	Value
Created_tmp_disk_tables	0
# Test when conversion to InnoDB affects recursive references which
# are not open yet (those of q1):
flush status;
with recursive q (b) as
(select 1 union all select 1+b from q where b<2000)
select min(q.b),max(q.b),avg(q.b) from q, q as q1;
min(q.b)	max(q.b)	avg(q.b)
1	2000	1000.5000
show status like 'Created_tmp_disk_tables';
Variable_name	Value
Created_tmp_disk_tables	0
# Same, but make q1 the writer; this is to test overflow when
# the writer isn't first in the 'tmp_tables' list
flush status;
with recursive q (b) as
(select 1 union all select 1+b from q where b<2000)
select min(q.b),max(q.b),avg(q.b) from q right join q as q1 on 1;
min(q.b)	max(q.b)	avg(q.b)
1	2000	1000.5000
show status like 'Created_tmp_disk_tables';
Variable_name	Value
Created_tmp_disk_tables	0
# Test when outer query reads CTE with an index.
# Overflow doesn't happen at same row as queries above, as this
# table has an index which makes it grow faster.
explain with recursive q (b) as
(select 1 union all select 1+b from q where b<2000)
select min(b),max(b),avg(b) from q where b=300;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	PRIMARY	<derived2>	NULL	ref	<auto_key0>	<auto_key0>	9	const	1	100.00	Using index
2	DERIVED	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	No tables used
3	UNION	q	NULL	ALL	NULL	NULL	NULL	NULL	2	50.00	Recursive; Using where
Warnings:
Note	1003	with recursive `q` (`b`) as (/* select#2 */ select 1 AS `1` union all /* select#3 */ select (1 + `q`.`b`) AS `1+b` from `q` where (`q`.`b` < 2000)) /* select#1 */ select min(`q`.`b`) AS `min(b)`,max(`q`.`b`) AS `max(b)`,avg(`q`.`b`) AS `avg(b)` from `q` where (`q`.`b` = 300)
show status like 'Created_tmp_disk_tables';
Variable_name	Value
Created_tmp_disk_tables	0
with recursive q (b) as
(select 1 union all select 1+b from q where b<2000)
select min(b),max(b),avg(b) from q where b=300;
min(b)	max(b)	avg(b)
300	300	300.0000
show status like 'Created_tmp_disk_tables';
Variable_name	Value
Created_tmp_disk_tables	1
# Verify that rows come out in insertion order.
# If they didn't, the sequences of @c and of 'b'
# would not be identical and the sum wouldn't be
# 1^2 + ... + 2000^2 = n(n+1)(2n+1)/6 = 2668667000
set @c:=1;
flush status;
with recursive q (b, c) as
(select 1, 1 union all select (1+b), (@c:=(@c+1)) from q where b<2000)
select sum(b*c) from q;
sum(b*c)
2668667000
Warnings:
Warning	1287	Setting user variables within expressions is deprecated and will be removed in a future release. Consider alternatives: 'SET variable=expression, ...', or 'SELECT expression(s) INTO variables(s)'.
show status like 'Created_tmp_disk_tables';
Variable_name	Value
Created_tmp_disk_tables	0
# cleanup
drop table t1;
set session internal_tmp_mem_storage_engine=default;
