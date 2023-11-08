eval
CREATE TABLE t (a INT, b INT, c INT, d INT, KEY k1(a, b, c, d)) ENGINE=$engine;

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

INSERT INTO t SELECT a.a, b.a, c.a, d.a FROM a, b, c, d;

INSERT INTO t VALUES (11, 2, 3, -10);
INSERT INTO t VALUES (11, 3, 3, 20);
INSERT INTO t VALUES (11, 15, NULL, -20);
INSERT INTO t VALUES (11, NULL, 40, 30);

INSERT INTO t VALUES (12, 15, 3, -15);
INSERT INTO t VALUES (12, 15, 40, -20);
INSERT INTO t VALUES (12, 2, 40, -23);
INSERT INTO t VALUES (12, 2, 3, NULL);


DROP TEMPORARY TABLE a, b, c, d;

--enable_query_log

ANALYZE TABLE t;

