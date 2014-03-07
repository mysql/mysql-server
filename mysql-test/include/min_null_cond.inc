--replace_column 1 x 2 x 3 x 5 x 6 x 7 x 8 x 9 x 10 x
EXPLAIN
SELECT MIN( a ) FROM t1 WHERE a = NULL;
SELECT MIN( a ) FROM t1 WHERE a = NULL;

--replace_column 1 x 2 x 3 x 5 x 6 x 7 x 8 x 9 x 10 x
EXPLAIN
SELECT MIN( a ) FROM t1 WHERE a <> NULL;
SELECT MIN( a ) FROM t1 WHERE a <> NULL;

--replace_column 1 x 2 x 3 x 5 x 6 x 7 x 8 x 9 x 10 x
EXPLAIN
SELECT MIN( a ) FROM t1 WHERE a > NULL;
SELECT MIN( a ) FROM t1 WHERE a > NULL;

--replace_column 1 x 2 x 3 x 5 x 6 x 7 x 8 x 9 x 10 x
EXPLAIN
SELECT MIN( a ) FROM t1 WHERE a < NULL;
SELECT MIN( a ) FROM t1 WHERE a < NULL;

if (!$skip_null_safe_test)
{
--replace_column 1 x 2 x 3 x 5 x 6 x 7 x 8 x 9 x 10 x
EXPLAIN
SELECT MIN( a ) FROM t1 WHERE a <=> NULL;
SELECT MIN( a ) FROM t1 WHERE a <=> NULL;
}
--replace_column 1 x 2 x 3 x 5 x 6 x 7 x 8 x 9 x 10 x
EXPLAIN
SELECT MIN( a ) FROM t1 WHERE a BETWEEN NULL AND 10;
SELECT MIN( a ) FROM t1 WHERE a BETWEEN NULL AND 10;

--replace_column 1 x 2 x 3 x 5 x 6 x 7 x 8 x 9 x 10 x
EXPLAIN
SELECT MIN( a ) FROM t1 WHERE a BETWEEN NULL AND NULL;
SELECT MIN( a ) FROM t1 WHERE a BETWEEN NULL AND NULL;

--replace_column 1 x 2 x 3 x 5 x 6 x 7 x 8 x 9 x 10 x
EXPLAIN
SELECT MIN( a ) FROM t1 WHERE a BETWEEN 10 AND NULL;
SELECT MIN( a ) FROM t1 WHERE a BETWEEN 10 AND NULL;

--replace_column 1 x 2 x 3 x 5 x 6 x 7 x 8 x 9 x 10 x
EXPLAIN
SELECT MIN( a ) FROM t1 WHERE a = (SELECT a FROM t1 WHERE a < 0);
SELECT MIN( a ) FROM t1 WHERE a = (SELECT a FROM t1 WHERE a < 0);

--replace_column 1 x 2 x 3 x 5 x 6 x 7 x 8 x 9 x 10 x
EXPLAIN
SELECT MIN( a ) FROM t1 WHERE a IS NULL;
SELECT MIN( a ) FROM t1 WHERE a IS NULL;
