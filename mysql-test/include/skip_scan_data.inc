eval CREATE TABLE t (a INT, b INT, c INT, d INT, e INT, PRIMARY KEY(a, b, c, d), KEY(b, d)) ENGINE=$engine;

--disable_query_log
CREATE TEMPORARY TABLE a (a INT);
let $i=10;
while ($i)
{
  --eval INSERT INTO a VALUES ($i)
  dec $i;
}

CREATE TEMPORARY TABLE b (a INT);
let $i=5;
while ($i)
{
  --eval INSERT INTO b VALUES ($i)
  dec $i;
}

CREATE TEMPORARY TABLE c (a INT);
let $i=5;
while ($i)
{
  --eval INSERT INTO c VALUES ($i)
  dec $i;
}

CREATE TEMPORARY TABLE d (a INT);
let $i=10;
while ($i)
{
  --eval INSERT INTO d VALUES ($i)
  dec $i;
}

--echo # Since ANALYZE TABLE only reads a subset of the data, the statistics for
--echo # table t depends on the row order. And since the INSERT INTO ... SELECT
--echo # may be executed using different execution plans, we've added ORDER BY
--echo # to ensure that we rows has the same order every time. If not, the
--echo # estimated number of rows in EXPLAIN may change on different platforms.
INSERT INTO t SELECT a.a, b.a, c.a, d.a, d.a FROM a, b, c, d
  ORDER BY a.a, b.a, c.a, d.a;

DROP TEMPORARY TABLE a, b, c, d;

# The cost estimates vary slightly between runs. Add extra data to the
# table to increase the difference in cost between a table scan and an
# index scan, so that the choice between those scans is less likely to
# change because of small variations in statistics.
ALTER TABLE t ADD COLUMN data VARCHAR(50) DEFAULT (REPEAT('x', 50));

--enable_query_log

ANALYZE TABLE t;

