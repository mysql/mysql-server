
BUG#11763382 Assertion 'inited==INDEX' on SELECT MAX(...)

CREATE TABLE t(i INT NOT NULL PRIMARY KEY, f INT) ENGINE = InnoDB;
INSERT INTO t VALUES (1,1),(2,2);
BEGIN;
UPDATE t SET f=100 WHERE i=2;
set optimizer_switch='semijoin=off,subquery_materialization_cost_based=off';
SET DEBUG_SYNC='before_index_end_in_subselect WAIT_FOR callit';
SELECT f FROM t WHERE i IN ( SELECT i FROM t );
SELECT MAX(i) FROM t FOR UPDATE;
SELECT MAX(i) FROM t FOR UPDATE;
SET DEBUG_SYNC='now SIGNAL callit';
COMMIT;
f
1
2
SET DEBUG_SYNC='RESET';
MAX(i)
2
MAX(i)
2
DROP TABLE t;
# End of BUG#56080
