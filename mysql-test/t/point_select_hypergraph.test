--source include/have_hypergraph.inc
--source include/elide_costs.inc

--echo #
--echo # Bug #32808908: MAKE PREPARED STATEMENT PLACEHOLDERS SARGABLE IN THE HYPERGRAPH OPTIMIZER
--echo #

CREATE TABLE t1 ( pk INTEGER NOT NULL, a INTEGER, PRIMARY KEY ( pk ) );
INSERT INTO t1 VALUES (1,10), (2,20), (3,30), (4,40), (5,50), (6,60), (7,70), (8,80), (9,90), (10,100);
ANALYZE TABLE t1;

PREPARE q FROM 'EXPLAIN FORMAT=tree SELECT * FROM t1 WHERE pk = ?';
SET @v = 2;
--replace_regex $elide_costs
EXECUTE q USING @v;

DROP TABLE t1;

SET optimizer_switch=DEFAULT;

--source include/disable_hypergraph.inc
