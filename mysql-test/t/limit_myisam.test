--source include/force_myisam_default.inc
--source include/have_myisam.inc

--echo #
--echo # Bug #11747810: EXPLAIN SHOWS BOGUS VALUE FOR
--echo #                'FILTERED' COLUMN FOR LIMIT QUERY
--echo #

CREATE TABLE t1 (a INT, KEY (a)) ENGINE=Myisam;
INSERT INTO t1 VALUES (0),(1),(2),(3),(4),(5),(6),(7),(8),(9);
EXPLAIN SELECT * FROM t1 ORDER BY a LIMIT 1;
DROP TABLE t1;

