#
# Bug #32808908: MAKE PREPARED STATEMENT PLACEHOLDERS SARGABLE IN THE HYPERGRAPH OPTIMIZER
#
CREATE TABLE t1 ( pk INTEGER NOT NULL, a INTEGER, PRIMARY KEY ( pk ) );
INSERT INTO t1 VALUES (1,10), (2,20), (3,30), (4,40), (5,50), (6,60), (7,70), (8,80), (9,90), (10,100);
ANALYZE TABLE t1;
Table	Op	Msg_type	Msg_text
test.t1	analyze	status	OK
PREPARE q FROM 'EXPLAIN FORMAT=tree SELECT * FROM t1 WHERE pk = ?';
SET @v = 2;
EXECUTE q USING @v;
EXPLAIN
-> Single-row index lookup on t1 using PRIMARY (pk=2)  (cost=0.25..0.25 rows=1)

DROP TABLE t1;
SET optimizer_switch=DEFAULT;
