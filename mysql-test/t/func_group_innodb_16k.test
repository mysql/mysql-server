--echo #
--echo # Bug#13723054 CRASH WITH MIN/MAX AFTER QUICK_GROUP_MIN_MAX_SELECT::NEXT_MIN
--echo #

CREATE TABLE t1(a BLOB, b VARCHAR(255) CHARSET LATIN1, c INT,
                KEY(b, c, a(765))) ENGINE=INNODB;
INSERT INTO t1(a, b, c) VALUES ('', 'a', 0), ('', 'a', null), ('', 'a', 0),
                               ('', 'a', null), ('', 'a', 0);

-- disable_result_log
ANALYZE TABLE t1;
-- enable_result_log

SELECT MIN(c) FROM t1 GROUP BY b;
EXPLAIN SELECT MIN(c) FROM t1 GROUP BY b;

DROP TABLE t1;


--echo End of 5.6 tests
