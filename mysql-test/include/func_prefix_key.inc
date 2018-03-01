--echo # 'Using index' method is used since wild string is suitable for the use with prefix key.
EXPLAIN SELECT COUNT(*) FROM t1 WHERE b like 'a%';
EXPLAIN SELECT COUNT(*) FROM t1 WHERE b like 'ab%';
EXPLAIN SELECT COUNT(*) FROM t1 WHERE b like 'abcd%';
EXPLAIN SELECT COUNT(*) FROM t1 WHERE b like '';
EXPLAIN SELECT COUNT(*) FROM t1 WHERE b like 'abc\%%';
EXPLAIN SELECT COUNT(*) FROM t1 WHERE b like '\%abc%';
EXPLAIN SELECT COUNT(*) FROM t1 WHERE b like 'abc%%';
EXPLAIN SELECT COUNT(*) FROM t1 WHERE b like '%';
EXPLAIN SELECT COUNT(*) FROM t1 WHERE b like '%%';
EXPLAIN SELECT COUNT(*) FROM t1 WHERE b like '____%';
EXPLAIN SELECT COUNT(*) FROM t1 WHERE b like '\_\_\_\_%';
EXPLAIN SELECT COUNT(*) FROM t1 WHERE b like '\%\%\%\%';
EXPLAIN SELECT b LIKE 'abcd' FROM t1 WHERE b like 'ab%';
EXPLAIN SELECT b LIKE 'abc%' FROM t1 WHERE b like 'ab%';
EXPLAIN SELECT COUNT(*) FROM t1 WHERE b like 'aaa\\';
EXPLAIN SELECT COUNT(*) FROM t1 WHERE b like 'aaa\\\\';
EXPLAIN SELECT COUNT(*) FROM t1 WHERE b like 'aaa\\\\%';
EXPLAIN SELECT COUNT(*) FROM t1 WHERE b like 'a_';

--echo # 'Using index' method is not used  since wild string is not suitable for the use with prefix key.
EXPLAIN SELECT COUNT(*) FROM t1 WHERE b like '%a';
EXPLAIN SELECT COUNT(*) FROM t1 WHERE b like 'a%c%';
EXPLAIN SELECT COUNT(*) FROM t1 WHERE b like 'a%c';
EXPLAIN SELECT COUNT(*) FROM t1 WHERE b like 'abcde%';
EXPLAIN SELECT COUNT(*) FROM t1 WHERE b like 'abcde%';
EXPLAIN SELECT b FROM t1 WHERE b like 'ab%';
EXPLAIN SELECT b LIKE 'abcde' FROM t1 WHERE b like 'ab%';
EXPLAIN SELECT b LIKE '%bc' FROM t1 WHERE b like 'ab%';
EXPLAIN SELECT COUNT(*) FROM t1 WHERE b like 'aaaa\\';

--echo # 'Using index' method is not used  since wild string is not suitable for the use with prefix key.
EXPLAIN SELECT COUNT(*) FROM t1 WHERE b like 'a%' OR b like '%a';
--echo # 'Using index' method is used since wild string is suitable for the use with prefix key.
EXPLAIN SELECT COUNT(*) FROM t1 WHERE b like 'ab%';
--echo # 'Using index' method is not used  since 'b' field is used in select output.
EXPLAIN SELECT b FROM t1 WHERE b like 'ab%';

--echo # Index k2 shouldn't be used.
EXPLAIN SELECT COUNT(*) FROM t1 IGNORE INDEX(k2) WHERE b like 'a%';
