# This test checks various optimizer-related functions using
# the debug-sync facility.

--source include/have_debug_sync.inc

# The test is specific to the old optimizer.
--source include/not_hypergraph.inc

--echo
--echo BUG#11763382 Assertion 'inited==INDEX' on SELECT MAX(...)
--echo

CREATE TABLE t(i INT NOT NULL PRIMARY KEY, f INT) ENGINE = InnoDB;
INSERT INTO t VALUES (1,1),(2,2);

BEGIN;
UPDATE t SET f=100 WHERE i=2;

--connect (con1,localhost,root,,)

set optimizer_switch='semijoin=off,subquery_materialization_cost_based=off';

SET DEBUG_SYNC='before_index_end_in_subselect WAIT_FOR callit';
--send SELECT f FROM t WHERE i IN ( SELECT i FROM t )

--connection default
let $show_statement= SHOW PROCESSLIST;
let $field= State;
let $condition= = 'debug sync point: before_index_end_in_subselect';
--source include/wait_show_condition.inc

--connect (con2,localhost,root,,)
--send SELECT MAX(i) FROM t FOR UPDATE

--connect (con3,localhost,root,,)
--send SELECT MAX(i) FROM t FOR UPDATE

--connection default

let $wait_condition=
  SELECT COUNT(*) = 2 FROM information_schema.processlist 
  WHERE state = 'Optimizing' and info = 'SELECT MAX(i) FROM t FOR UPDATE';
--source include/wait_condition.inc

SET DEBUG_SYNC='now SIGNAL callit';
COMMIT;

--connection con1
--reap
SET DEBUG_SYNC='RESET';

--connection con2
--reap

--connection con3
--reap

--connection default
DROP TABLE t;
--disconnect con1
--disconnect con2
--disconnect con3


--echo # End of BUG#56080
