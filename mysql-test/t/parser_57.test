#
# Coverage tests to trace a backward compatibility with the 5.7 syntax.
#
# This file should not diverge between 5.7 and 8.0 without a huge reason
# like a feature deprecation in 5.7 and its removal in 8.0.
#

--echo #
--echo # Test pushing/not pushing of the LIMIT/ORDER BY clauses down into a
--echo # parenthesized query block.
--echo #

CREATE TABLE t1 (k SERIAL, i INT) ENGINE=InnoDB;
INSERT INTO t1 (i) VALUES (10), (2), (30), (4), (50);

--echo # push down -- standard compliant
(SELECT * FROM t1) LIMIT 4;
--echo # push down -- standard compliant
(SELECT * FROM t1) ORDER BY i;
--echo # push down -- standard compliant
(SELECT * FROM t1) ORDER BY i LIMIT 4;

--echo # push down -- standard compliant
(SELECT * FROM t1 LIMIT 3) LIMIT 4;
--echo # don't push down -- standard compliant
(SELECT * FROM t1 LIMIT 3) ORDER BY i DESC;
--echo # don't push down -- standard compliant
(SELECT * FROM t1 LIMIT 3) ORDER BY i DESC LIMIT 4;

--echo # push down -- standard compliant
(SELECT * FROM t1 ORDER BY i DESC LIMIT 3) LIMIT 4;
--echo # don't push down -- standard compliant
(SELECT * FROM t1 ORDER BY i DESC LIMIT 3) ORDER BY i;
--echo # don't push down -- standard compliant
(SELECT * FROM t1 ORDER BY i DESC LIMIT 3) ORDER BY i LIMIT 4;

((SELECT * FROM t1 ORDER BY i) ORDER BY i) ORDER BY i;

--disable_result_log
--echo # 8.0: push down -- standard compliant
--echo #  or
--echo # 5.7: rejected syntax -- non-standard
--error 0,ER_PARSE_ERROR
((SELECT * FROM t1 LIMIT 1) LIMIT 2) LIMIT 3;
--enable_result_log

DROP TABLE t1;
