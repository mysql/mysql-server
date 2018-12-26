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

INSERT INTO t SELECT a.a, b.a, c.a, d.a, d.a FROM a, b, c, d;

DROP TEMPORARY TABLE a, b, c, d;

--enable_query_log

ANALYZE TABLE t;

