# MRR tests that are special for InnoDB

--echo #
--echo # Bug#41029 "MRR: SELECT FOR UPDATE fails to lock gaps (InnoDB table)"
--echo #

# This test verifies that a SELECT FOR UPDATE statement executed in
# REPEATABLE READ isolation will lock the entire read interval by verifying
# that a second transaction trying to update data within this interval will
# be blocked.

connect (con1,localhost,root,,);
connect (con2,localhost,root,,);

connection con1;

SET AUTOCOMMIT=0;

CREATE TABLE t1 (
  dummy INT PRIMARY KEY,
  a INT UNIQUE,
  b INT
) ENGINE=InnoDB;

INSERT INTO t1 VALUES (1,1,1),(3,3,3),(5,5,5);
COMMIT;

SET SESSION TRANSACTION ISOLATION LEVEL REPEATABLE READ;
SELECT @@transaction_isolation;
START TRANSACTION;

EXPLAIN SELECT * FROM t1 WHERE a > 2 FOR UPDATE;

SELECT * FROM t1 WHERE a > 2 FOR UPDATE;

connection con2;

SET AUTOCOMMIT=0;
START TRANSACTION;

--error ER_LOCK_WAIT_TIMEOUT
INSERT INTO t1 VALUES (2,2,2);
ROLLBACK;

connection con1;

ROLLBACK;
DROP TABLE t1;

connection default;
disconnect con1;
disconnect con2;

--echo #
--echo # Bug#54286 "Server crash at lock timeout with MRR"
--echo #

connect (con1,localhost,root,,);
connect (con2,localhost,root,,);

connection con1;

SET AUTOCOMMIT=0;

CREATE TABLE t1 (
  dummy INT PRIMARY KEY,
  a INT UNIQUE,
  b INT
) ENGINE=InnoDB;

COMMIT;

INSERT INTO t1 VALUES (1,1,1),(3,3,3),(5,5,5);

connection con2;

SET AUTOCOMMIT=0;

--error ER_LOCK_WAIT_TIMEOUT
SELECT * FROM t1 WHERE a > 2 FOR UPDATE;
ROLLBACK;

connection con1;

ROLLBACK;
DROP TABLE t1;
COMMIT;

connection default;
disconnect con1;
disconnect con2;

--echo #
--echo # Bug#36614948 No information about MRR in EXPLAIN with explain_json_format_version=2
--echo #

SET @saved_explain_json_format_version = @@explain_json_format_version;
SET explain_json_format_version = 2;

CREATE TABLE t1 (
    a INTEGER,
    b INTEGER
);
CREATE TABLE t2 (
    a INTEGER,
    b INTEGER,
    KEY a (a)
);

# Expect bka with mrr. We do not care where they occur, only that both are present,
# as the mrr access path can only exist as a sub-path to bka.
EXPLAIN FORMAT=JSON INTO @var SELECT /*+ BKA(t2) */ * FROM t1 LEFT JOIN t2 ON t1.a = t2.a AND t1.b = 1 AND t1.b = 2;
# Hypergraph does not yet support BKA
--skip_if_hypergraph
SELECT
JSON_SEARCH(@var, 'one', 'Batched key access%', NULL, '$**.operation') IS NOT NULL
AS batched_key_access,
JSON_CONTAINS_PATH(@var, 'one', '$**.multi_range_read')
AS multi_range_read,
JSON_SEARCH(@var, 'one', 'Multi-range%', NULL, '$**.operation') IS NOT NULL
AS operation_contains_multi_range_read;

DROP TABLE t1,t2;

SET explain_json_format_version = @saved_explain_json_format_version;