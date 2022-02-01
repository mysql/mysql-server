set cte_max_recursion_depth=5000;

flush status;
with recursive q (b) as
(select 1 union all select 1+b from q where b<2000)
select min(b),max(b),avg(b) from q;
show status like 'Created_tmp_disk_tables';

--echo # Test when conversion to InnoDB affects recursive references which
--echo # are not open yet (those of q1):
flush status;
with recursive q (b) as
(select 1 union all select 1+b from q where b<2000)
select min(q.b),max(q.b),avg(q.b) from q, q as q1;
show status like 'Created_tmp_disk_tables';

--echo # Same, but make q1 the writer; this is to test overflow when
--echo # the writer isn't first in the 'tmp_tables' list

flush status;
with recursive q (b) as
(select 1 union all select 1+b from q where b<2000)
select min(q.b),max(q.b),avg(q.b) from q right join q as q1 on 1;
show status like 'Created_tmp_disk_tables';

--echo # Test when outer query reads CTE with an index.
--echo # Overflow doesn't happen at same row as queries above, as this
--echo # table has an index which makes it grow faster.

let $query=
with recursive q (b) as
(select 1 union all select 1+b from q where b<2000)
select min(b),max(b),avg(b) from q where b=300;

eval explain $query;
show status like 'Created_tmp_disk_tables';
eval $query;
show status like 'Created_tmp_disk_tables';

--echo # Verify that rows come out in insertion order.
--echo # If they didn't, the sequences of @c and of 'b'
--echo # would not be identical and the sum wouldn't be
--echo # 1^2 + ... + 2000^2 = n(n+1)(2n+1)/6 = 2668667000

set @c:=1;
flush status;
with recursive q (b, c) as
(select 1, 1 union all select (1+b), (@c:=(@c+1)) from q where b<2000)
select sum(b*c) from q;
show status like 'Created_tmp_disk_tables';
